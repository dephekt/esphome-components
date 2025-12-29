from esphome import automation
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_TRIGGER_ID

DEPENDENCIES = ["esp32"]

m5cores3_touch_ns = cg.esphome_ns.namespace("m5cores3_touch")
M5CoreS3Touch = m5cores3_touch_ns.class_("M5CoreS3Touch", cg.Component)
TouchTrigger = m5cores3_touch_ns.class_("TouchTrigger", automation.Trigger)
TouchPoint = m5cores3_touch_ns.struct("TouchPoint")

CONF_ON_TOUCH = "on_touch"
CONF_ON_RELEASE = "on_release"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(M5CoreS3Touch),
        cv.Optional(CONF_ON_TOUCH): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(TouchTrigger),
            }
        ),
        cv.Optional(CONF_ON_RELEASE): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                    automation.Trigger.template()
                ),
            }
        ),
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    if CONF_ON_TOUCH in config:
        for conf in config[CONF_ON_TOUCH]:
            trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
            await automation.build_automation(trigger, [(TouchPoint, "touch")], conf)

    if CONF_ON_RELEASE in config:
        for conf in config[CONF_ON_RELEASE]:
            trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID])
            cg.add(var.add_on_release_trigger(trigger))
            await automation.build_automation(trigger, [], conf)
