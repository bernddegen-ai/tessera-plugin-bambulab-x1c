import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import smart_display, text_sensor
from esphome.const import CONF_ID

DEPENDENCIES = ["smart_display"]

SOURCES = ['status', 'progress', 'layer', 'total_layers', 'time_left', 'time_unit', 'job', 'nozzle', 'bed', 'chamber']

bambulab_x1c_ns = cg.esphome_ns.namespace("bambulab_x1c")
BambulabX1C = bambulab_x1c_ns.class_("BambulabX1C", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {cv.GenerateID(): cv.declare_id(BambulabX1C)}
).extend({cv.Required(key): cv.use_id(text_sensor.TextSensor) for key in SOURCES}).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await smart_display.register_plugin(var, __file__)
    for index, key in enumerate(SOURCES):
        cg.add(var.set_source(index, await cg.get_variable(config[key])))
