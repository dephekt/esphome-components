import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG

from . import ezo_types_ns

CONF_DATALOGGER = "datalogger"
CONF_EXTENDED_SCALE = "extended_scale"
CONF_INTERVAL = "interval"

DataloggerSwitch = ezo_types_ns.class_("DataloggerSwitch", switch.Switch, cg.Component)
ExtendedScaleSwitch = ezo_types_ns.class_("ExtendedScaleSwitch", switch.Switch, cg.Component)


def _datalogger_switch_schema():
    return switch.switch_schema(
        DataloggerSwitch,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon="mdi:database-clock",
    ).extend({cv.Optional(CONF_INTERVAL, default=60): cv.positive_int})


def _extended_scale_switch_schema():
    return switch.switch_schema(
        ExtendedScaleSwitch,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon="mdi:arrow-expand-horizontal",
    )
