import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    UNIT_CELSIUS,
    UNIT_VOLT,
    UNIT_AMPERE,
    UNIT_HERTZ,
    UNIT_EMPTY,
    UNIT_REVOLUTIONS_PER_MINUTE,
    ICON_THERMOMETER,
    ICON_CURRENT_AC,
    ICON_FLASH,
    ICON_FAN,
    ICON_EMPTY,
    ENTITY_CATEGORY_DIAGNOSTIC,
)

from . import heater_uart_ns, HeaterUart

DEPENDENCIES = ["heater_uart"]

CONF_HEATER_UART_ID = "heater_uart_id"

# Define all sensors with their unit, icon, and device class
SENSORS = {
    "current_temperature": ("Current Temperature", UNIT_CELSIUS, ICON_THERMOMETER),
    "fan_speed": ("Fan Speed", UNIT_REVOLUTIONS_PER_MINUTE, ICON_FAN),
    "supply_voltage": ("Supply Voltage", UNIT_VOLT, ICON_FLASH),
    "heat_exchanger_temp": ("Heat Exchanger Temperature", UNIT_CELSIUS, ICON_THERMOMETER),
    "glow_plug_voltage": ("Glow Plug Voltage", UNIT_VOLT, ICON_FLASH),
    "glow_plug_current": ("Glow Plug Current", UNIT_AMPERE, ICON_CURRENT_AC),
    "pump_frequency": ("Pump Frequency", UNIT_HERTZ, ICON_EMPTY),
    "fan_voltage": ("Fan Voltage", UNIT_VOLT, ICON_FLASH),
    "desired_temperature": ("Desired Temperature", UNIT_CELSIUS, ICON_THERMOMETER),
    "crc_errors": ("CRC Errors", UNIT_EMPTY, "mdi:alert-circle-outline"),
    "pump_min_limit": ("Pump Min Limit", UNIT_HERTZ, ICON_EMPTY),
    "pump_max_limit": ("Pump Max Limit", UNIT_HERTZ, ICON_EMPTY),
    "fan_min_limit": ("Fan Min Limit", UNIT_REVOLUTIONS_PER_MINUTE, ICON_FAN),
    "fan_max_limit": ("Fan Max Limit", UNIT_REVOLUTIONS_PER_MINUTE, ICON_FAN),
    "altitude": ("Altitude", "m", "mdi:image-filter-hdr"),
    "target_frequency": ("Target Frequency", UNIT_HERTZ, ICON_EMPTY),
}

DIAGNOSTIC_SENSORS = [
    "crc_errors",
    "pump_min_limit",
    "pump_max_limit",
    "fan_min_limit",
    "fan_max_limit",
    "altitude"
]

# CONFIG_SCHEMA = cv.Schema(
#     {
#         cv.GenerateID(): cv.use_id(HeaterUart),
#         **{
#             cv.Optional(sensor_key): sensor.sensor_schema(
#                 unit_of_measurement=unit,
#                 icon=icon,
#                 accuracy_decimals=1,
#             )
#             for sensor_key, (_, unit, icon) in SENSORS.items()
#         },
#     }
# )

schema_dict = {
    cv.GenerateID(CONF_HEATER_UART_ID): cv.use_id(HeaterUart),
}

for sensor_key, (name, unit, icon) in SENSORS.items():
    # Start with required args (or args with safe defaults)
    kwargs = {
        "accuracy_decimals": 0 if sensor_key in ["crc_errors", "fan_speed", "fan_min_limit", "fan_max_limit", "altitude"] else 1
    }

    # Only add optional args if they are NOT None
    if unit is not UNIT_EMPTY:
        kwargs["unit_of_measurement"] = unit
    if icon is not ICON_EMPTY:
        kwargs["icon"] = icon
    if sensor_key in DIAGNOSTIC_SENSORS:
        kwargs["entity_category"] = ENTITY_CATEGORY_DIAGNOSTIC

    # Add to schema
    schema_dict[cv.Optional(sensor_key)] = sensor.sensor_schema(**kwargs)

CONFIG_SCHEMA = cv.Schema(schema_dict)

async def to_code(config):
    # Resolve the HeaterUart parent component
    parent = await cg.get_variable(config[CONF_HEATER_UART_ID])

    for sensor_key, (name, _, _) in SENSORS.items():
        if sensor_key in config:
            sens = await sensor.new_sensor(config[sensor_key])
            cg.add(parent.set_sensor(sensor_key, sens))
