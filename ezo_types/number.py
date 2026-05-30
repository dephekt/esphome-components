import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import CONF_ID, ENTITY_CATEGORY_CONFIG

from . import ezo_types_ns

CODEOWNERS = ["@dephekt"]
DEPENDENCIES = ["ezo_types", "number"]

TDSConversionFactorNumber = ezo_types_ns.class_(
    "TDSConversionFactorNumber", number.Number, cg.Component
)

CONFIG_SCHEMA = number.number_schema(
    TDSConversionFactorNumber,
    entity_category=ENTITY_CATEGORY_CONFIG,
    icon="mdi:water-percent",
).extend({cv.GenerateID(CONF_ID): cv.declare_id(TDSConversionFactorNumber)})


async def to_code(config):
    var = await number.new_number(
        config,
        min_value=0.01,
        max_value=1.0,
        step=0.01,
    )
    await cg.register_component(var, config)
