import esphome.codegen as cg

CODEOWNERS = ["@dephekt"]
AUTO_LOAD = ["ezo"]
DEPENDENCIES = ["i2c"]
MULTI_CONF = True

ezo_types_ns = cg.esphome_ns.namespace("ezo_types")
