#pragma once
#include "esphome/core/component.h"
#include "esphome/components/smart_display/plugin_api.h"

namespace esphome::bambulab_x1c {

class BambulabX1C : public Component, public tessera::Plugin {
 public:
  void setup() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
};

}  // namespace esphome::bambulab_x1c
