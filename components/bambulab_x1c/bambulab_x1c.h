#pragma once
#include "esphome/core/component.h"
#include "esphome/components/smart_display/plugin_api.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include <cstdlib>
#include <string>

namespace esphome::bambulab_x1c {

// The printer's sensors, in the order of SOURCES in __init__.py.
enum Source : uint8_t { STATUS, PROGRESS, LAYER, TOTAL_LAYERS, TIME_LEFT, TIME_UNIT, JOB, NOZZLE, BED, CHAMBER, SOURCES };

class BambulabX1C : public Component, public tessera::Plugin {
 public:
  void setup() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  void set_source(int index, text_sensor::TextSensor *sensor) { sources_[index] = sensor; }

  // What a sensor says now; "" while Home Assistant has no value for it.
  std::string value(Source source) const {
    const text_sensor::TextSensor *s = sources_[source];
    if (!s || !s->has_state()) return "";
    const std::string &v = s->state;
    return (v == "unavailable" || v == "unknown") ? "" : v;
  }
  // A sensor's value as a whole number, -1 when it is none.
  int whole(Source source) const {
    const float v = number(value(source));
    return v < 0 ? -1 : static_cast<int>(v + 0.5f);
  }
  // Minutes left, -1 when unknown. Home Assistant may report the sensor in minutes, hours or seconds.
  int remaining_minutes() const {
    float m = number(value(TIME_LEFT));
    if (m < 0) return -1;
    const std::string unit = value(TIME_UNIT);
    if (unit == "h") m *= 60.0f;
    else if (unit == "s") m /= 60.0f;
    else if (unit == "d") m *= 1440.0f;
    return static_cast<int>(m + 0.5f);
  }

 private:
  static float number(const std::string &s) {
    char *end = nullptr;
    const float v = strtof(s.c_str(), &end);
    return end == s.c_str() ? -1.0f : v;
  }
  text_sensor::TextSensor *sources_[SOURCES]{};
};

}  // namespace esphome::bambulab_x1c
