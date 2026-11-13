#include "bambulab_x1c.h"
#include <cstdio>
#include <string>
#include <cmath>

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

static std::string format_temp(const char *state) {
  if (!state || !*state) return "--";
  float v = atof(state);
  char buf[16];
  snprintf(buf, sizeof(buf), "%.0f°", v);
  return buf;
}

static int parse_int(const char *state) {
  if (!state || !*state) return -1;
  return atoi(state);
}

// ─── PrinterStatusTile ─────────────────────────────────────────────────────
// 2x1 to 4x2: shows print status, progress bar, layer, time remaining, temps.

class PrinterStatusTile : public tessera::Tile {
 public:
  explicit PrinterStatusTile(const tessera::Plugin *plugin) : plugin_(plugin) {}

  void create(const tessera::TileContext &c) override {
    width_ = c.width;
    height_ = c.height;

    // Title row: printer icon + status text
    status_label_ = ui::label(c.parent, Font::BODY, theme::INK);
    lv_obj_set_width(status_label_, width_);

    // Progress bar (LVGL native)
    progress_bar_ = lv_bar_create(c.parent);
    lv_obj_set_size(progress_bar_, width_, 8);
    lv_bar_set_range(progress_bar_, 0, 100);
    lv_bar_set_value(progress_bar_, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(progress_bar_, lv_color_hex(0x334155), LV_PART_MAIN);
    lv_obj_set_style_bg_color(progress_bar_, lv_color_hex(0x22d3ee), LV_PART_INDICATOR);
    lv_obj_set_style_radius(progress_bar_, 4, LV_PART_MAIN);
    lv_obj_set_style_radius(progress_bar_, 4, LV_PART_INDICATOR);

    // Progress percentage
    progress_label_ = ui::label(c.parent, Font::BODY, theme::ACCENT);
    lv_obj_set_width(progress_label_, width_);
    lv_obj_set_style_text_align(progress_label_, LV_TEXT_ALIGN_RIGHT, 0);

    // Info row: layer / time remaining
    info_label_ = ui::label(c.parent, Font::CAPTION, theme::MUTED);
    lv_obj_set_width(info_label_, width_);

    // Temps row (nozzle / bed / chamber)
    temps_label_ = ui::label(c.parent, Font::CAPTION, theme::MUTED);
    lv_obj_set_width(temps_label_, width_);

    layout();
  }

  void on_state(JsonObjectConst data) override {
    // entity option data arrives under "entities"
    JsonObjectConst entities = data["entities"];

    auto get_state = [&](const char *key) -> const char * {
      if (!entities[key].isNull()) {
        const char *s = entities[key]["state"] | "";
        return s;
      }
      return "";
    };

    status_text_ = get_state("entity_print_status");
    progress_val_ = parse_int(get_state("entity_progress"));

    const char *layer     = get_state("entity_layer");
    const char *total_lay = get_state("entity_total_layers");
    if (*layer && *total_lay && atoi(total_lay) > 0)
      layer_text_ = std::string(layer) + "/" + total_lay;
    else if (*layer)
      layer_text_ = layer;
    else
      layer_text_ = "";

    const char *jn = get_state("entity_job_name");
    job_name_ = (*jn && std::string(jn) != "unavailable") ? jn : "";

    time_remaining_ = parse_int(get_state("entity_time_remaining"));
    nozzle_temp_ = get_state("entity_nozzle_temp");
    bed_temp_ = get_state("entity_bed_temp");
    chamber_temp_ = get_state("entity_chamber_temp");
  }

  void on_tick(uint32_t /*epoch*/) override {
    // Job name as subtitle, or status if no job name
    std::string headline = status_text_.empty() ? plugin_->text("no_data") : status_text_;
    if (!job_name_.empty()) headline += "  ·  " + job_name_;
    ui::set_text(status_label_, headline);

    // Progress bar
    int pv = (progress_val_ >= 0 && progress_val_ <= 100) ? progress_val_ : 0;
    lv_bar_set_value(progress_bar_, pv, LV_ANIM_OFF);

    char pct[8];
    snprintf(pct, sizeof(pct), "%d%%", pv);
    ui::set_text(progress_label_, pct);

    // Info: layer + time
    std::string info;
    if (!layer_text_.empty() && layer_text_ != "unavailable") {
      info += plugin_->text("layer") + std::string(": ") + layer_text_;
    }
    if (time_remaining_ > 0) {
      if (!info.empty()) info += "  ";
      info += plugin_->text("remaining") + std::string(": ") + format_minutes(time_remaining_);
    }
    ui::set_text(info_label_, info);

    // Temps
    std::string temps;
    if (*nozzle_temp_) temps += plugin_->text("nozzle") + std::string(" ") + format_temp(nozzle_temp_);
    if (*bed_temp_) {
      if (!temps.empty()) temps += "  ";
      temps += plugin_->text("bed") + std::string(" ") + format_temp(bed_temp_);
    }
    if (*chamber_temp_) {
      if (!temps.empty()) temps += "  ";
      temps += plugin_->text("chamber") + std::string(" ") + format_temp(chamber_temp_);
    }
    ui::set_text(temps_label_, temps);

    layout();
  }

  void on_theme() override {
    ui::set_color(status_label_, theme::INK);
    ui::set_color(progress_label_, theme::ACCENT);
    ui::set_color(info_label_, theme::MUTED);
    ui::set_color(temps_label_, theme::MUTED);
    lv_obj_set_style_bg_color(progress_bar_, lv_color_hex(0x334155), LV_PART_MAIN);
    lv_obj_set_style_bg_color(progress_bar_, lv_color_hex(0x22d3ee), LV_PART_INDICATOR);
  }

 private:
  void layout() {
    int line_h = ui::line_height(Font::BODY);
    int cap_h  = ui::line_height(Font::CAPTION);
    int bar_h  = 8;
    int gap    = 4;

    int total = line_h + gap + bar_h + gap + cap_h + gap + cap_h;
    int y = (height_ - total) / 2;

    lv_obj_set_pos(status_label_, 0, y);
    lv_obj_set_height(status_label_, line_h);
    y += line_h + gap;

    lv_obj_set_pos(progress_bar_, 0, y);
    // progress_label_ overlaps bar (right-aligned)
    lv_obj_set_pos(progress_label_, 0, y - cap_h);
    lv_obj_set_height(progress_label_, cap_h);
    y += bar_h + gap;

    lv_obj_set_pos(info_label_, 0, y);
    lv_obj_set_height(info_label_, cap_h);
    y += cap_h + gap;

    lv_obj_set_pos(temps_label_, 0, y);
    lv_obj_set_height(temps_label_, cap_h);
  }

  const tessera::Plugin *plugin_;
  int width_ = 0, height_ = 0;

  std::string status_text_;
  std::string job_name_;
  int progress_val_ = 0;
  std::string layer_text_;
  int time_remaining_ = -1;
  const char *nozzle_temp_ = "";
  const char *bed_temp_    = "";
  const char *chamber_temp_ = "";

  lv_obj_t *status_label_{};
  lv_obj_t *progress_bar_{};
  lv_obj_t *progress_label_{};
  lv_obj_t *info_label_{};
  lv_obj_t *temps_label_{};
};

// ─── PrinterTempsTile ──────────────────────────────────────────────────────
// 1x1 to 2x2: compact 3-row temperature overview.

class PrinterTempsTile : public tessera::Tile {
 public:
  explicit PrinterTempsTile(const tessera::Plugin *plugin) : plugin_(plugin) {}

  void create(const tessera::TileContext &c) override {
    width_ = c.width;
    height_ = c.height;
    for (int i = 0; i < 3; i++) {
      labels_[i] = ui::label(c.parent, Font::BODY, i == 0 ? theme::INK : theme::MUTED);
      lv_obj_set_width(labels_[i], width_);
    }
  }

  void on_state(JsonObjectConst data) override {
    JsonObjectConst entities = data["entities"];
    auto get = [&](const char *key) -> const char * {
      return entities[key].isNull() ? "" : (entities[key]["state"] | "");
    };
    nozzle_ = get("entity_nozzle_temp");
    bed_    = get("entity_bed_temp");
    chamber_= get("entity_chamber_temp");
  }

  void on_tick(uint32_t /*epoch*/) override {
    ui::set_text(labels_[0], plugin_->text("nozzle") + std::string(": ") + format_temp(nozzle_));
    ui::set_text(labels_[1], plugin_->text("bed")    + std::string(": ") + format_temp(bed_));
    ui::set_text(labels_[2], plugin_->text("chamber")+ std::string(": ") + format_temp(chamber_));

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
  const tessera::Plugin *plugin_;
  int width_ = 0, height_ = 0;
  const char *nozzle_  = "";
  const char *bed_     = "";
  const char *chamber_ = "";
  lv_obj_t *labels_[3]{};
};

// ─── Plugin setup ──────────────────────────────────────────────────────────

void BambulabX1C::setup() {
  add_tile("printer_status", [this]() { return new PrinterStatusTile(this); });
  add_tile("printer_temps",  [this]() { return new PrinterTempsTile(this); });
}

}  // namespace esphome::bambulab_x1c
