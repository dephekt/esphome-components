import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text, time
from esphome.const import CONF_TIME_ID, ENTITY_CATEGORY_CONFIG

CODEOWNERS = ["@dephekt"]
DEPENDENCIES = ["time"]

timezone_ns = cg.esphome_ns.namespace("timezone")
TimezoneText = timezone_ns.class_("TimezoneText", text.Text, cg.Component)

CONFIG_SCHEMA = (
    text.text_schema(
        TimezoneText,
        icon="mdi:map-clock",
        entity_category=ENTITY_CATEGORY_CONFIG,
        mode="TEXT",
    )
    .extend({cv.GenerateID(CONF_TIME_ID): cv.use_id(time.RealTimeClock)})
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    # The C++ side uses time::parse_posix_tz and RealTimeClock::set_timezone,
    # which only exist under USE_TIME_TIMEZONE. The time component emits that
    # define only when a build-time timezone is configured, so force it here
    # so a config with timezone handling disabled (timezone: '') still
    # compiles and can run override-only.
    cg.add_define("USE_TIME_TIMEZONE")
    # Deliberately larger than the on-device 63-char persistence limit: the
    # text core silently drops writes above the trait max WITHOUT re-publishing
    # state, so over-length values must reach control() for its reject +
    # re-publish path to keep the MQTT echo contract uniform. control()
    # enforces the real TZ_BUFFER_SIZE limit.
    var = await text.new_text(config, max_length=255)
    await cg.register_component(var, config)
    rtc = await cg.get_variable(config[CONF_TIME_ID])
    cg.add(var.set_time(rtc))
