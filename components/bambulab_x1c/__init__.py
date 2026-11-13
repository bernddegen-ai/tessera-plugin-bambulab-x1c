import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import smart_display
from esphome.const import CONF_ID

DEPENDENCIES = ["smart_display"]

CONF_PREFIX = "prefix"

bambulab_x1c_ns = cg.esphome_ns.namespace("bambulab_x1c")
BambulabX1C = bambulab_x1c_ns.class_("BambulabX1C", cg.Component)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(BambulabX1C),
    cv.Required(CONF_PREFIX): cv.string,
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_prefix(config[CONF_PREFIX]))
    await smart_display.register_plugin(var, __file__)
