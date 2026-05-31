import esphome.codegen as cg
from esphome.components import switch
from esphome.const import ENTITY_CATEGORY_CONFIG

from . import ezo_types_ns

CONF_EXTENDED_SCALE = "extended_scale"

ExtendedScaleSwitch = ezo_types_ns.class_("ExtendedScaleSwitch", switch.Switch, cg.Component)


def _extended_scale_switch_schema():
    return switch.switch_schema(
        ExtendedScaleSwitch,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon="mdi:arrow-expand-horizontal",
    )
