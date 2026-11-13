#pragma once
#include "esphome/core/component.h"
#include "esphome/components/smart_display/plugin_api.h"
#include "esphome/components/api/custom_api_device.h"
#include <string>

namespace esphome::bambulab_x1c {

class BambulabX1C : public Component, public tessera::Plugin, public api::CustomAPIDevice {
 public:
  void setup() override;
  void set_prefix(const std::string &p) { prefix_ = p; }
  float get_setup_priority() const override { return setup_priority::DATA; }

  // State — written by HA subscriptions, read by tiles in on_tick
  std::string print_status;
  int progress_val{0};
  int layer_val{-1};
  int total_layers_val{-1};
  int time_remaining{-1};
  std::string job_name;
  std::string nozzle_temp;
  std::string bed_temp;
  std::string chamber_temp;

 private:
  std::string prefix_;

  void on_print_status(std::string s)   { print_status = s; }
  void on_progress(std::string s)       { progress_val = atoi(s.c_str()); }
  void on_layer(std::string s)          { layer_val = atoi(s.c_str()); }
  void on_total_layers(std::string s)   { total_layers_val = atoi(s.c_str()); }
  void on_time_remaining(std::string s) { time_remaining = atoi(s.c_str()); }
  void on_job_name(std::string s)       { job_name = (s == "unavailable") ? "" : s; }
  void on_nozzle(std::string s)         { nozzle_temp = s; }
  void on_bed(std::string s)            { bed_temp = s; }
  void on_chamber(std::string s)        { chamber_temp = s; }
};

}  // namespace esphome::bambulab_x1c
