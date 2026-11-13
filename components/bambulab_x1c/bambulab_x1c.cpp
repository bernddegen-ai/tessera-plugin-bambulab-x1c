#include "bambulab_x1c.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <cctype>

namespace esphome::bambulab_x1c {

using tessera::Font;
namespace ui = tessera::ui;

// ─── helpers ───────────────────────────────────────────────────────────────

static std::string format_minutes(int minutes) {
  if (minutes <= 0) return "";
  if (minutes < 60) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%dm", minutes);
    return buf;
  }
  char buf[16];
  snprintf(buf, sizeof(buf), "%dh %dm", minutes / 60, minutes % 60);
  return buf;
}

static std::string format_temp(const std::string &s) {
  if (s.empty() || s == "unavailable") return "--";
  float v = atof(s.c_str());
  char buf[16];
  snprintf(buf, sizeof(buf), "%.0f°", v);
  return buf;
}

static lv_color_t status_color(const std::string &status) {
  std::string s = status;
  for (auto &c : s) c = (char)tolower((unsigned char)c);
  if (s.find("print") != std::string::npos || s.find("druckt") != std::string::npos)
    return lv_color_hex(0x22d3ee);
  if (s.find("paus") != std::string::npos)
    return lv_color_hex(0xeab308);
  if (s.find("fail") != std::string::npos || s.find("fehler") != std::string::npos ||
      s.find("error") != std::string::npos)
    return lv_color_hex(0xef4444);
  return lv_color_hex(0x94a3b8);
}

// ─── PrinterStatusTile ─────────────────────────────────────────────────────
// 2x1 to 4x2: circular arc progress on the left, status info on the right.

class PrinterStatusTile : public tessera::Tile {
 public:
  explicit PrinterStatusTile(BambulabX1C *plugin) : plugin_(plugin) {}

  void create(const tessera::TileContext &c) override {
    width_ = c.width;
    height_ = c.height;

    int pad = 4;
    arc_size_ = height_ - 2 * pad;
    // compact mode when there's not enough horizontal space for arc + info
    compact_ = (width_ < arc_size_ + 40);

    int info_x = compact_ ? 0 : (pad + arc_size_ + 8);
    int info_w = compact_ ? width_ : (width_ - info_x - pad);

    // Circular progress arc (hidden in compact mode)
    arc_ = lv_arc_create(c.parent);
    lv_obj_set_size(arc_, arc_size_, arc_size_);
    lv_obj_set_pos(arc_, pad, pad);
    lv_arc_set_range(arc_, 0, 100);
    lv_arc_set_value(arc_, 0);
    lv_arc_set_bg_angles(arc_, 135, 45);
    lv_arc_set_mode(arc_, LV_ARC_MODE_NORMAL);
    lv_obj_set_style_arc_color(arc_, lv_color_hex(0x1e293b), LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc_, lv_color_hex(0x94a3b8), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc_, 6, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc_, 6, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(arc_, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_pad_all(arc_, 0, LV_PART_KNOB);
    lv_obj_clear_flag(arc_, LV_OBJ_FLAG_CLICKABLE);
    if (compact_) lv_obj_add_flag(arc_, LV_OBJ_FLAG_HIDDEN);

    // Percentage label — centered over arc in normal mode, top of tile in compact
    pct_label_ = ui::label(c.parent, Font::BODY, theme::INK);
    lv_obj_set_style_text_align(pct_label_, LV_TEXT_ALIGN_CENTER, 0);

    // Status + job name
    status_label_ = ui::label(c.parent, Font::BODY, theme::INK);
    lv_obj_set_width(status_label_, info_w);

    // Layer / time remaining
    info_label_ = ui::label(c.parent, Font::BODY, theme::MUTED);
    lv_obj_set_width(info_label_, info_w);

    // Temperatures
    temps_label_ = ui::label(c.parent, Font::BODY, theme::MUTED);
    lv_obj_set_width(temps_label_, info_w);

    info_x_ = info_x;
    info_w_ = info_w;
    layout();
  }

  void on_tick(uint32_t) override {
    int pv = (plugin_->progress_val >= 0 && plugin_->progress_val <= 100) ? plugin_->progress_val : 0;
    lv_arc_set_value(arc_, pv);

    lv_color_t col = status_color(plugin_->print_status);
    lv_obj_set_style_arc_color(arc_, col, LV_PART_INDICATOR);

    char pct[8];
    snprintf(pct, sizeof(pct), "%d%%", pv);
    ui::set_text(pct_label_, pct);

    // Status line
    std::string stat = plugin_->print_status.empty() ? plugin_->text("no_data") : plugin_->print_status;
    if (!plugin_->job_name.empty()) stat += "  ·  " + plugin_->job_name;
    ui::set_text(status_label_, stat);
    lv_obj_set_style_text_color(status_label_, col, 0);

    // Layer + time
    std::string info;
    if (plugin_->layer_val >= 0) {
      info = std::string(plugin_->text("layer")) + ": " + std::to_string(plugin_->layer_val);
      if (plugin_->total_layers_val > 0)
        info += "/" + std::to_string(plugin_->total_layers_val);
    }
    if (plugin_->time_remaining > 0) {
      if (!info.empty()) info += "  ";
      info += std::string(plugin_->text("remaining")) + ": " + format_minutes(plugin_->time_remaining);
    }
    ui::set_text(info_label_, info);

    // Temps
    std::string temps;
    auto add_temp = [&](const char *label_key, const std::string &val) {
      if (val.empty() || val == "unavailable") return;
      if (!temps.empty()) temps += "  ";
      temps += std::string(plugin_->text(label_key)) + " " + format_temp(val);
    };
    add_temp("nozzle", plugin_->nozzle_temp);
    add_temp("bed",    plugin_->bed_temp);
    add_temp("chamber",plugin_->chamber_temp);
    ui::set_text(temps_label_, temps);

    layout();
  }

  void on_theme() override {
    lv_obj_set_style_arc_color(arc_, lv_color_hex(0x1e293b), LV_PART_MAIN);
    ui::set_color(info_label_, theme::MUTED);
    ui::set_color(temps_label_, theme::MUTED);
    lv_obj_set_style_arc_color(arc_, status_color(plugin_->print_status), LV_PART_INDICATOR);
  }

 private:
  void layout() {
    int pad = 4;
    int lh  = ui::line_height(Font::BODY);
    int gap = 4;

    if (compact_) {
      // compact 1×1: pct big, status below, no arc, no info/temps
      int total = 2 * lh + gap;
      int y = (height_ - total) / 2;
      lv_obj_set_width(pct_label_, width_);
      lv_obj_set_pos(pct_label_, 0, y);
      lv_obj_set_width(status_label_, width_);
      lv_obj_set_pos(status_label_, 0, y + lh + gap);
      lv_obj_set_height(status_label_, lh);
      lv_obj_add_flag(info_label_, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(temps_label_, LV_OBJ_FLAG_HIDDEN);
    } else {
      int total = 3 * lh + 2 * gap;
      int y = (height_ - total) / 2;

      lv_obj_set_pos(arc_, pad, (height_ - arc_size_) / 2);

      lv_obj_set_width(pct_label_, arc_size_);
      lv_obj_set_pos(pct_label_, pad, (height_ - lh) / 2);

      lv_obj_set_pos(status_label_, info_x_, y);
      lv_obj_set_height(status_label_, lh);
      lv_obj_set_width(status_label_, info_w_);

      lv_obj_set_pos(info_label_, info_x_, y + lh + gap);
      lv_obj_set_height(info_label_, lh);
      lv_obj_set_width(info_label_, info_w_);

      lv_obj_set_pos(temps_label_, info_x_, y + 2 * (lh + gap));
      lv_obj_set_height(temps_label_, lh);
      lv_obj_set_width(temps_label_, info_w_);
    }
  }

  BambulabX1C *plugin_;
  int width_{0}, height_{0};
  int arc_size_{0}, info_x_{0}, info_w_{0};
  bool compact_{false};

  lv_obj_t *arc_{};
  lv_obj_t *pct_label_{};
  lv_obj_t *status_label_{};
  lv_obj_t *info_label_{};
  lv_obj_t *temps_label_{};
};

// ─── PrinterTempsTile ──────────────────────────────────────────────────────
// 1x1 to 2x2: compact 3-row temperature overview.

class PrinterTempsTile : public tessera::Tile {
 public:
  explicit PrinterTempsTile(BambulabX1C *plugin) : plugin_(plugin) {}

  void create(const tessera::TileContext &c) override {
    width_ = c.width;
    height_ = c.height;
    for (int i = 0; i < 3; i++) {
      labels_[i] = ui::label(c.parent, Font::BODY, i == 0 ? theme::INK : theme::MUTED);
      lv_obj_set_width(labels_[i], width_);
    }
  }

  void on_tick(uint32_t) override {
    ui::set_text(labels_[0], std::string(plugin_->text("nozzle"))   + ": " + format_temp(plugin_->nozzle_temp));
    ui::set_text(labels_[1], std::string(plugin_->text("bed"))     + ": " + format_temp(plugin_->bed_temp));
    ui::set_text(labels_[2], std::string(plugin_->text("chamber")) + ": " + format_temp(plugin_->chamber_temp));

    int lh = ui::line_height(Font::BODY);
    int gap = 6;
    int total = 3 * lh + 2 * gap;
    int y = (height_ - total) / 2;
    for (int i = 0; i < 3; i++) {
      lv_obj_set_pos(labels_[i], 0, y + i * (lh + gap));
      lv_obj_set_height(labels_[i], lh);
    }
  }

  void on_theme() override {
    ui::set_color(labels_[0], theme::INK);
    ui::set_color(labels_[1], theme::MUTED);
    ui::set_color(labels_[2], theme::MUTED);
  }

 private:
  BambulabX1C *plugin_;
  int width_{0}, height_{0};
  lv_obj_t *labels_[3]{};
};

// ─── Plugin setup ──────────────────────────────────────────────────────────

void BambulabX1C::setup() {
  subscribe_homeassistant_state(&BambulabX1C::on_print_status,   prefix_ + "_druckstatus");
  subscribe_homeassistant_state(&BambulabX1C::on_progress,       prefix_ + "_druckfortschritt");
  subscribe_homeassistant_state(&BambulabX1C::on_layer,          prefix_ + "_aktuelle_schicht");
  subscribe_homeassistant_state(&BambulabX1C::on_total_layers,   prefix_ + "_gesamtzahl_der_schichten");
  subscribe_homeassistant_state(&BambulabX1C::on_time_remaining, prefix_ + "_verbleibende_zeit");
  subscribe_homeassistant_state(&BambulabX1C::on_job_name,       prefix_ + "_name_der_aufgabe");
  subscribe_homeassistant_state(&BambulabX1C::on_nozzle,         prefix_ + "_temperatur_der_duse");
  subscribe_homeassistant_state(&BambulabX1C::on_bed,            prefix_ + "_druckbetttemperatur");
  subscribe_homeassistant_state(&BambulabX1C::on_chamber,        prefix_ + "_temperatur_im_druckraum");

  add_tile("printer_status", [this]() { return new PrinterStatusTile(this); });
  add_tile("printer_temps",  [this]() { return new PrinterTempsTile(this); });
}

}  // namespace esphome::bambulab_x1c
