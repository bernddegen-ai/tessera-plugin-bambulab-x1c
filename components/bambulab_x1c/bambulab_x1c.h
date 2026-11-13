#pragma once
#include "esphome/core/component.h"
#include "esphome/components/smart_display/plugin_api.h"
#include "esphome/components/api/custom_api_device.h"
#include <cstdlib>
#include <string>

namespace esphome::bambulab_x1c {

class BambulabX1C : public Component, public tessera::Plugin, public api::CustomAPIDevice {
 public:
  void setup() override;
  void set_prefix(const std::string &p) { prefix_ = p; }
  float get_setup_priority() const override { return setup_priority::DATA; }

  std::string print_status;
  int progress_val{-1};
  int layer_val{-1};
  int total_layers_val{-1};
  std::string job_name;
  std::string nozzle_temp;
  std::string bed_temp;
  std::string chamber_temp;

  // Minutes left, -1 when unknown. Home Assistant may report the sensor in minutes, hours or seconds.
  int remaining_minutes() const {
    if (time_raw_ < 0) return -1;
    float m = time_raw_;
    if (time_unit_ == "h") m *= 60.0f;
    else if (time_unit_ == "s") m /= 60.0f;
    else if (time_unit_ == "d") m *= 1440.0f;
    return static_cast<int>(m + 0.5f);
  }

 private:
  std::string prefix_;
  float time_raw_{-1};
  std::string time_unit_;

  // -1 for anything that is not a number ("unavailable", "unknown", "").
  static float number(const std::string &s) {
    char *end = nullptr;
    float v = strtof(s.c_str(), &end);
    return end == s.c_str() ? -1.0f : v;
  }
  static int whole(const std::string &s) {
    float v = number(s);
    return v < 0 ? -1 : static_cast<int>(v + 0.5f);
  }
  static std::string known(const std::string &s) { return (s == "unavailable" || s == "unknown") ? "" : s; }

  void on_print_status(std::string s)   { print_status = s; }
  void on_progress(std::string s)       { progress_val = whole(s); }
  void on_layer(std::string s)          { layer_val = whole(s); }
  void on_total_layers(std::string s)   { total_layers_val = whole(s); }
  void on_time_remaining(std::string s) { time_raw_ = number(s); }
  void on_time_unit(std::string s)      { time_unit_ = s; }
  void on_job_name(std::string s)       { job_name = known(s); }
  void on_nozzle(std::string s)         { nozzle_temp = known(s); }
  void on_bed(std::string s)            { bed_temp = known(s); }
  void on_chamber(std::string s)        { chamber_temp = known(s); }
};

}  // namespace esphome::bambulab_x1c
