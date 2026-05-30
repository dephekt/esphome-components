import esphome.codegen as cg
from esphome.components import select
from esphome.const import ENTITY_CATEGORY_CONFIG

from . import ezo_types_ns

CONF_CELL_CONSTANT = "cell_constant"

CellConstantSelect = ezo_types_ns.class_(
    "CellConstantSelect", select.Select, cg.Component
)


def _cell_constant_select_schema():
    return select.select_schema(
        CellConstantSelect,
        icon="mdi:alpha-k-box-outline",
        entity_category=ENTITY_CATEGORY_CONFIG,
    )
