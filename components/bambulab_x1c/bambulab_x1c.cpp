#include "bambulab_x1c.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace esphome::bambulab_x1c {

using tessera::Font;
namespace ui = tessera::ui;

namespace {

// The tiles keep their own dark look in the screen's light and dark theme, like Tessera's media card.
struct Rgb {
  uint8_t r, g, b;
};
constexpr Rgb C_GROUND{15, 17, 23};
constexpr Rgb C_TRACK{30, 36, 51};
constexpr Rgb C_CHIP{28, 33, 48};
constexpr Rgb C_LINE{35, 40, 56};
constexpr Rgb C_INK{255, 255, 255};
constexpr Rgb C_MUTED{139, 147, 167};
constexpr Rgb C_COOL{163, 177, 204};
constexpr Rgb C_HOT{251, 146, 60};
constexpr Rgb C_CYAN{34, 211, 238};
constexpr Rgb C_YELLOW{234, 179, 8};
constexpr Rgb C_RED{239, 68, 68};
constexpr Rgb C_GREEN{34, 197, 94};
constexpr Rgb C_IDLE{100, 116, 139};

constexpr float HOT_FROM = 45.0f;

lv_color_t col(Rgb c) { return lv_color_make(c.r, c.g, c.b); }

enum class State : uint8_t { OFFLINE, IDLE, PREPARE, RUNNING, PAUSED, FINISHED, FAILED };

// Home Assistant sends the raw state of the Bambu Lab integration's print status sensor.
State parse_state(const std::string &raw) {
  std::string s = raw;
  for (auto &c : s) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
  if (s == "running") return State::RUNNING;
  if (s == "pause" || s == "paused") return State::PAUSED;
  if (s == "failed") return State::FAILED;
  if (s == "finish" || s == "finished") return State::FINISHED;
  if (s == "idle") return State::IDLE;
  if (s == "prepare" || s == "slicing" || s == "init") return State::PREPARE;
  return State::OFFLINE;
}

Rgb state_color(State s) {
  switch (s) {
    case State::RUNNING:
    case State::PREPARE: return C_CYAN;
    case State::PAUSED: return C_YELLOW;
    case State::FAILED: return C_RED;
    case State::FINISHED: return C_GREEN;
    default: return C_IDLE;
  }
}

const char *state_key(State s) {
  switch (s) {
    case State::RUNNING: return "st_running";
    case State::PREPARE: return "st_prepare";
    case State::PAUSED: return "st_paused";
    case State::FAILED: return "st_failed";
    case State::FINISHED: return "st_finished";
    case State::IDLE: return "st_idle";
    default: return "st_offline";
  }
}

std::string format_minutes(int minutes) {
  char buf[16];
  if (minutes < 60) snprintf(buf, sizeof(buf), "%dm", minutes);
  else snprintf(buf, sizeof(buf), "%dh %dm", minutes / 60, minutes % 60);
  return buf;
}

std::string format_temp(const std::string &s) {
  if (s.empty()) return "--";
  char buf[16];
  snprintf(buf, sizeof(buf), "%.0f°", atof(s.c_str()));
  return buf;
}

Rgb temp_color(const std::string &s) { return (!s.empty() && atof(s.c_str()) >= HOT_FROM) ? C_HOT : C_COOL; }

lv_obj_t *plain(lv_obj_t *parent, Rgb fill) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_set_style_bg_color(o, col(fill), 0);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
  lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  return o;
}

lv_obj_t *ground(lv_obj_t *parent, int w, int h) {
  lv_obj_t *o = plain(parent, C_GROUND);
  lv_obj_set_pos(o, 0, 0);
  lv_obj_set_size(o, w, h);
  lv_obj_set_style_radius(o, ui::px(12), 0);
  lv_obj_set_style_clip_corner(o, true, 0);
  return o;
}

lv_obj_t *words(lv_obj_t *parent, Font font, Rgb color) {
  lv_obj_t *l = ui::label(parent, font, theme::INK);
  lv_obj_set_style_text_color(l, col(color), 0);
  return l;
}

void place(lv_obj_t *o, int x, int y, int w) {
  lv_obj_set_pos(o, x, y);
  lv_obj_set_width(o, w);
}

void show(lv_obj_t *o, bool on) {
  if (on) lv_obj_clear_flag(o, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
}

void center(lv_obj_t *l) { lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0); }

// The first of the candidates that fits the width; the last one when none does.
const std::string &fitting(const std::string *candidates, int count, int width) {
  for (int i = 0; i < count - 1; i++)
    if (!candidates[i].empty() && ui::text_width(candidates[i], Font::BODY) <= width) return candidates[i];
  return candidates[count - 1];
}

}  // namespace

// ─── PrinterStatusTile ─────────────────────────────────────────────────────
// A progress ring with the percentage in it, beside or above the status, the job, layer and time left, and the three
// temperatures. What does not fit the tile's size gives way: the job's name first, then the temperatures, then the ring.

class PrinterStatusTile : public tessera::Tile {
 public:
  explicit PrinterStatusTile(BambulabX1C *plugin) : plugin_(plugin) {}

  void create(const tessera::TileContext &c) override {
    const int w = c.width, h = c.height;
    lv_obj_t *root = ground(c.parent, w, h);

    const int lh = ui::line_height(Font::BODY);
    const int gap = ui::px(4);
    const int pad = std::min(w, h) < ui::px(110) ? ui::px(6) : ui::px(12);
    const int iw = w - 2 * pad, ih = h - 2 * pad;
    int chip_pad = ui::px(3);
    int chip_h = lh + 2 * chip_pad;

    arc_ = lv_arc_create(root);
    lv_obj_remove_style(arc_, nullptr, LV_PART_KNOB);
    lv_obj_clear_flag(arc_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_pad_all(arc_, 0, LV_PART_MAIN);
    lv_arc_set_rotation(arc_, 270);
    lv_arc_set_bg_angles(arc_, 0, 360);
    lv_arc_set_range(arc_, 0, 100);
    lv_arc_set_value(arc_, 0);
    lv_obj_set_style_arc_color(arc_, col(C_TRACK), LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc_, col(C_IDLE), LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(arc_, true, LV_PART_INDICATOR);

    pct_ = words(root, Font::BODY, C_INK);
    center(pct_);
    status_ = words(root, Font::BODY, C_IDLE);
    job_ = words(root, Font::BODY, C_INK);
    info_ = words(root, Font::BODY, C_MUTED);
    temps_ = words(root, Font::BODY, C_MUTED);
    for (auto &chip : chips_) {
      chip = words(root, Font::BODY, C_COOL);
      center(chip);
      show(chip, false);
    }

    // Which arrangement fits: the ring above the lines on a tile that stands up, beside them on one that lies down.
    int arc = 0, rows = 1;
    bool stacked = ih > iw * 9 / 10, ring = true;
    if (stacked) {
      rows = 4;
      while (true) {
        arc = std::min(iw * 8 / 10, ih - rows * (lh + gap) - gap);
        if (arc >= ui::px(72) || rows == 1) break;
        rows--;
      }
      text_w_ = iw;
      if (arc < ui::px(44)) ring = false;
    } else {
      const int between = ui::px(12);
      arc = std::min(ih, iw * 4 / 10);
      text_w_ = iw - arc - between;
      if (arc < ui::px(44) || text_w_ < ui::px(80)) ring = false;
    }

    bool with_job = false, with_info = false, with_temps = false;
    int text_x = pad, y = pad;

    if (!ring) {
      // No room for a ring: the percentage as a line of its own, over the status.
      show(arc_, false);
      text_w_ = iw;
      const int ph = ui::line_height(Font::TITLE);
      ui::set_font(pct_, Font::TITLE);
      with_info = ph + 2 * (lh + gap) <= ih;
      const int total = ph + gap + lh + (with_info ? gap + lh : 0);
      y = pad + std::max(0, (ih - total) / 2);
      place(pct_, pad, y, iw);
      y += ph + gap;
      for (lv_obj_t *l : {status_, job_, info_, temps_}) center(l);
    } else if (stacked) {
      with_job = rows >= 4;
      with_info = rows >= 2;
      with_temps = chips_on_ = chips_centered_ = rows >= 3;
      const int total = arc + gap + rows * (lh + gap);
      chips_filled_ = total + chip_h - lh <= ih;
      y = pad + std::max(0, (ih - total) / 2);
      ring_at(pad + (iw - arc) / 2, y, arc);
      y += arc + 2 * gap;
      for (lv_obj_t *l : {status_, job_, info_, temps_}) center(l);
    } else {
      text_x = pad + arc + ui::px(12);
      ring_at(pad, pad + (ih - arc) / 2, arc);
      int total;
      if (3 * lh + chip_h + 3 * gap <= ih) {
        with_job = with_info = with_temps = chips_on_ = chips_filled_ = true;
        total = 3 * lh + chip_h + 3 * gap;
      } else if (3 * lh + 2 * gap <= ih) {
        with_info = with_temps = chips_on_ = true;
        job_inline_ = text_w_ >= ui::px(220);
        total = 3 * lh + 2 * gap;
        // Slim chips with a ground of their own, when two pixels more still fit.
        if (total + 2 * ui::px(1) <= ih) {
          chips_filled_ = true;
          chip_pad = ui::px(1);
          chip_h = lh + 2 * chip_pad;
          total += 2 * chip_pad;
        }
      } else if (2 * lh + gap <= ih) {
        with_info = true;
        job_inline_ = text_w_ >= ui::px(220);
        total = 2 * lh + gap;
      } else {
        total = lh;
      }
      y = pad + (ih - total) / 2;
    }

    place(status_, text_x, y, text_w_);
    status_x_ = text_x;
    if (job_inline_) lv_obj_set_y(job_, y);
    y += lh + gap;
    show(job_, with_job || job_inline_);
    if (with_job) {
      place(job_, text_x, y, text_w_);
      y += lh + gap;
    }
    show(info_, with_info);
    if (with_info) {
      place(info_, text_x, y, text_w_);
      y += lh + gap;
    }
    show(temps_, with_temps && !chips_on_);
    if (with_temps) {
      place(temps_, text_x, y, text_w_);
      chips_x_ = text_x;
      chips_y_ = y;
    }
    if (chips_filled_)
      for (auto *chip : chips_) {
        lv_obj_set_height(chip, chip_h);
        lv_obj_set_style_pad_top(chip, chip_pad, 0);
        lv_obj_set_style_bg_color(chip, col(C_CHIP), 0);
        lv_obj_set_style_bg_opa(chip, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(chip, ui::px(6), 0);
      }

    update();
  }

  void on_tick(uint32_t) override { update(); }

 private:
  void ring_at(int x, int y, int size) {
    const int stroke = std::max(ui::px(4), std::min(ui::px(10), size / 12));
    lv_obj_set_pos(arc_, x, y);
    lv_obj_set_size(arc_, size, size);
    lv_obj_set_style_arc_width(arc_, stroke, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc_, stroke, LV_PART_INDICATOR);
    const int inside = size - 2 * stroke - ui::px(8);
    Font font = Font::BODY;
    for (Font f : {Font::HEADLINE, Font::TITLE})
      if (ui::text_width("100%", f) <= inside) {
        font = f;
        break;
      }
    ui::set_font(pct_, font);
    place(pct_, x, y + (size - ui::line_height(font)) / 2, size);
  }

  void update() {
    const State state = parse_state(plugin_->value(STATUS));
    const lv_color_t tone = col(state_color(state));
    const int progress = std::max(0, std::min(100, plugin_->whole(PROGRESS)));

    lv_arc_set_value(arc_, progress);
    lv_obj_set_style_arc_color(arc_, tone, LV_PART_INDICATOR);
    char pct[8];
    snprintf(pct, sizeof(pct), "%d%%", progress);
    ui::set_text(pct_, pct);

    const std::string status = plugin_->text(state_key(state));
    ui::set_text(status_, status);
    lv_obj_set_style_text_color(status_, tone, 0);
    ui::set_text(job_, plugin_->value(JOB));
    if (job_inline_ && status != status_shown_) {
      // The job's name follows the status on its line.
      status_shown_ = status;
      const int taken = ui::text_width(status, Font::BODY) + ui::px(10);
      lv_obj_set_width(status_, taken);
      lv_obj_set_x(job_, status_x_ + taken);
      lv_obj_set_width(job_, std::max(0, text_w_ - taken));
    }

    const bool active = state == State::RUNNING || state == State::PAUSED || state == State::PREPARE;
    std::string layers, left;
    if ((active || state == State::FINISHED) && plugin_->whole(LAYER) >= 0) {
      layers = std::to_string(plugin_->whole(LAYER));
      if (plugin_->whole(TOTAL_LAYERS) > 0) layers += "/" + std::to_string(plugin_->whole(TOTAL_LAYERS));
    }
    const int minutes = plugin_->remaining_minutes();
    if (active && minutes > 0) left = format_minutes(minutes);
    std::string info[3];
    if (!layers.empty()) info[0] = std::string(plugin_->text("layer")) + " " + layers;
    if (!left.empty()) {
      if (!info[0].empty()) info[0] += "   ";
      info[0] += tessera::fill(plugin_->text("remaining"), "t", left);
    }
    info[1] = layers;
    if (!left.empty()) info[1] += (info[1].empty() ? "" : "   ") + left;
    info[2] = left.empty() ? layers : left;
    ui::set_text(info_, fitting(info, 3, text_w_));

    const std::string raw[3] = {plugin_->value(NOZZLE), plugin_->value(BED), plugin_->value(CHAMBER)};
    const std::string n = format_temp(raw[0]), b = format_temp(raw[1]), c = format_temp(raw[2]);
    const std::string signature = n + b + c;
    if (signature == temps_shown_) return;
    temps_shown_ = signature;

    if (chips_on_ && lay_chips(raw, n, b, c)) {
      show(temps_, false);
      return;
    }
    for (auto *chip : chips_) show(chip, false);
    std::string temps[3];
    temps[0] = std::string(plugin_->text("nozzle")) + " " + n + "  " + plugin_->text("bed") + " " + b + "  " +
               plugin_->text("chamber") + " " + c;
    temps[1] = n + "  " + b + "  " + c;
    temps[2] = n + "  " + b;
    ui::set_text(temps_, fitting(temps, 3, text_w_));
    show(temps_, chips_y_ > 0);
  }

  // The temperatures as three chips, with their names when the row has room for them. False when not even the values fit.
  bool lay_chips(const std::string *raw, const std::string &n, const std::string &b, const std::string &c) {
    const std::string values[3] = {n, b, c};
    const char *keys[3] = {"nozzle", "bed", "chamber"};
    const int inner = chips_filled_ ? ui::px(7) : 0, between = chips_filled_ ? ui::px(6) : ui::px(10);
    std::string texts[3];
    int widths[3];
    // With their names, then the values alone, then without the chamber.
    for (int attempt = 0; attempt < 3; attempt++) {
      const int count = attempt == 2 ? 2 : 3;
      int total = (count - 1) * between;
      for (int i = 0; i < count; i++) {
        texts[i] = attempt == 0 ? std::string(plugin_->text(keys[i])) + " " + values[i] : values[i];
        widths[i] = ui::text_width(texts[i], Font::BODY) + 2 * inner + ui::px(2);
        total += widths[i];
      }
      if (total > text_w_) continue;
      int x = chips_x_ + (chips_centered_ ? (text_w_ - total) / 2 : 0);
      for (int i = 0; i < 3; i++) {
        show(chips_[i], i < count);
        if (i >= count) continue;
        ui::set_text(chips_[i], texts[i]);
        lv_obj_set_style_text_color(chips_[i], col(temp_color(raw[i])), 0);
        place(chips_[i], x, chips_y_, widths[i]);
        x += widths[i] + between;
      }
      return true;
    }
    return false;
  }

  BambulabX1C *plugin_;
  int text_w_{0}, status_x_{0}, chips_x_{0}, chips_y_{0};
  bool chips_on_{false}, chips_filled_{false}, chips_centered_{false}, job_inline_{false};
  std::string temps_shown_{"?"}, status_shown_{"?"};

  lv_obj_t *arc_{};
  lv_obj_t *pct_{};
  lv_obj_t *status_{};
  lv_obj_t *job_{};
  lv_obj_t *info_{};
  lv_obj_t *temps_{};
  lv_obj_t *chips_[3]{};
};

// ─── PrinterTempsTile ──────────────────────────────────────────────────────
// Three rows, the name on the left and the temperature on the right, warm values in orange.

class PrinterTempsTile : public tessera::Tile {
 public:
  explicit PrinterTempsTile(BambulabX1C *plugin) : plugin_(plugin) {}

  void create(const tessera::TileContext &c) override {
    const int w = c.width, h = c.height;
    lv_obj_t *root = ground(c.parent, w, h);

    const int pad = std::min(w, h) < ui::px(110) ? ui::px(8) : ui::px(14);
    const int iw = w - 2 * pad, ih = h - 2 * pad;
    const int lb = ui::line_height(Font::BODY), lt = ui::line_height(Font::TITLE);

    const bool header = ih >= lb + ui::px(6) + 3 * (lt + ui::px(6));
    const int top = pad + (header ? lb + ui::px(6) : 0);
    const int row = (pad + ih - top) / 3;
    const Font value_font = lt <= row ? Font::TITLE : Font::BODY;
    const int lv = ui::line_height(value_font);

    if (header) {
      lv_obj_t *title = words(root, Font::BODY, C_MUTED);
      place(title, pad, pad, iw);
      ui::set_text(title, plugin_->text("temps"));
    }
    const char *keys[3] = {"nozzle", "bed", "chamber"};
    for (int i = 0; i < 3; i++) {
      const int y = top + i * row;
      if (i > 0 && row >= lv + ui::px(8)) {
        lv_obj_t *line = plain(root, C_LINE);
        lv_obj_set_pos(line, pad, y);
        lv_obj_set_size(line, iw, 1);
      }
      lv_obj_t *name = words(root, Font::BODY, C_MUTED);
      place(name, pad, y + (row - lb) / 2, iw / 2);
      ui::set_text(name, plugin_->text(keys[i]));
      values_[i] = words(root, value_font, C_COOL);
      lv_obj_set_style_text_align(values_[i], LV_TEXT_ALIGN_RIGHT, 0);
      place(values_[i], pad + iw - iw / 2, y + (row - lv) / 2, iw / 2);
    }
    update();
  }

  void on_tick(uint32_t) override { update(); }

 private:
  void update() {
    const std::string raw[3] = {plugin_->value(NOZZLE), plugin_->value(BED), plugin_->value(CHAMBER)};
    for (int i = 0; i < 3; i++) {
      ui::set_text(values_[i], format_temp(raw[i]));
      lv_obj_set_style_text_color(values_[i], col(temp_color(raw[i])), 0);
    }
  }

  BambulabX1C *plugin_;
  lv_obj_t *values_[3]{};
};

// ─── Plugin setup ──────────────────────────────────────────────────────────

void BambulabX1C::setup() {
  add_tile("printer_status", [this]() { return new PrinterStatusTile(this); });
  add_tile("printer_temps",  [this]() { return new PrinterTempsTile(this); });
}

}  // namespace esphome::bambulab_x1c
