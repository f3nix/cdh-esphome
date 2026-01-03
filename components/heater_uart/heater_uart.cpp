#include "heater_uart.h"
#include "esphome/core/log.h"

// CRC-16/MODBUS Lookup Table
static const uint16_t wCRCTable[] = {
    0X0000, 0XC0C1, 0XC181, 0X0140, 0XC301, 0X03C0, 0X0280, 0XC241,
    0XC601, 0X06C0, 0X0780, 0XC741, 0X0500, 0XC5C1, 0XC481, 0X0440,
    0XCC01, 0X0CC0, 0X0D80, 0XCD41, 0X0F00, 0XCFC1, 0XCE81, 0X0E40,
    0X0A00, 0XCAC1, 0XCB81, 0X0B40, 0XC901, 0X09C0, 0X0880, 0XC841,
    0XD801, 0X18C0, 0X1980, 0XD941, 0X1B00, 0XDBC1, 0XDA81, 0X1A40,
    0X1E00, 0XDEC1, 0XDF81, 0X1F40, 0XDD01, 0X1DC0, 0X1C80, 0XDC41,
    0X1400, 0XD4C1, 0XD581, 0X1540, 0XD701, 0X17C0, 0X1680, 0XD641,
    0XD201, 0X12C0, 0X1380, 0XD341, 0X1100, 0XD1C1, 0XD081, 0X1040,
    0XF001, 0X30C0, 0X3180, 0XF141, 0X3300, 0XF3C1, 0XF281, 0X3240,
    0X3600, 0XF6C1, 0XF781, 0X3740, 0XF501, 0X35C0, 0X3480, 0XF441,
    0X3C00, 0XFCC1, 0XFD81, 0X3D40, 0XFF01, 0X3FC0, 0X3E80, 0XFE41,
    0XFA01, 0X3AC0, 0X3B80, 0XFB41, 0X3900, 0XF9C1, 0XF881, 0X3840,
    0X2800, 0XE8C1, 0XE981, 0X2940, 0XEB01, 0X2BC0, 0X2A80, 0XEA41,
    0XEE01, 0X2EC0, 0X2F80, 0XEF41, 0X2D00, 0XEDC1, 0XEC81, 0X2C40,
    0XE401, 0X24C0, 0X2580, 0XE541, 0X2700, 0XE7C1, 0XE681, 0X2640,
    0X2200, 0XE2C1, 0XE381, 0X2340, 0XE101, 0X21C0, 0X2080, 0XE041,
    0XA001, 0X60C0, 0X6180, 0XA141, 0X6300, 0XA3C1, 0XA281, 0X6240,
    0X6600, 0XA6C1, 0XA781, 0X6740, 0XA501, 0X65C0, 0X6480, 0XA441,
    0X6C00, 0XACC1, 0XAD81, 0X6D40, 0XAF01, 0X6FC0, 0X6E80, 0XAE41,
    0XAA01, 0X6AC0, 0X6B80, 0XAB41, 0X6900, 0XA9C1, 0XA881, 0X6840,
    0X7800, 0XB8C1, 0XB981, 0X7940, 0XBB01, 0X7BC0, 0X7A80, 0XBA41,
    0XBE01, 0X7EC0, 0X7F80, 0XBF41, 0X7D00, 0XBDC1, 0XBC81, 0X7C40,
    0XB401, 0X74C0, 0X7580, 0XB541, 0X7700, 0XB7C1, 0XB681, 0X7640,
    0X7200, 0XB2C1, 0XB381, 0X7340, 0XB101, 0X71C0, 0X7080, 0XB041,
    0X5000, 0X90C1, 0X9181, 0X5140, 0X9301, 0X53C0, 0X5280, 0X9241,
    0X9601, 0X56C0, 0X5780, 0X9741, 0X5500, 0X95C1, 0X9481, 0X5440,
    0X9C01, 0X5CC0, 0X5D80, 0X9D41, 0X5F00, 0X9FC1, 0X9E81, 0X5E40,
    0X5A00, 0X9AC1, 0X9B81, 0X5B40, 0X9901, 0X59C0, 0X5880, 0X9841,
    0X8801, 0X48C0, 0X4980, 0X8941, 0X4B00, 0X8BC1, 0X8A81, 0X4A40,
    0X4E00, 0X8EC1, 0X8F81, 0X4F40, 0X8D01, 0X4DC0, 0X4C80, 0X8C41,
    0X4400, 0X84C1, 0X8581, 0X4540, 0X8701, 0X47C0, 0X4680, 0X8641,
    0X8201, 0X42C0, 0X4380, 0X8341, 0X4100, 0X81C1, 0X8081, 0X4040
};

uint16_t calculate_crc16(const uint8_t *nData, size_t wLength) {
    uint8_t nTemp;
    uint16_t wCRCWord = 0xFFFF;
    while (wLength--) {
        nTemp = *nData++ ^ wCRCWord;
        wCRCWord >>= 8;
        wCRCWord ^= wCRCTable[nTemp];
    }
    return wCRCWord;
}

namespace esphome {
namespace heater_uart {

static const char *TAG = "heater_uart";

// Mappings for error and run states
const std::map<int, std::string> HeaterUart::run_state_map = {
    {0, "Off / Standby"}, {1, "Start Acknowledge"}, {2, "Glow Plug Pre-heat"},
    {3, "Failed Ignition - Pause for Retry"}, {4, "Ignited – Heating to Full Temp"},
    {5, "Running"}, {6, "Stop Acknowledge"}, {7, "Stopping - Post Run Glow Re-heat"},
    {8, "Cooldown"}
};

const std::map<int, std::string> HeaterUart::error_code_map = {
    {0, "No Error"}, {1, "No Error, But Started"}, {2, "Voltage Too Low"},
    {3, "Voltage Too High"}, {4, "Ignition Plug Failure"}, {5, "Pump Failure – Over Current"},
    {6, "Too Hot"}, {7, "Motor Failure"}, {8, "Serial Connection Lost"},
    {9, "Fire Extinguished"}, {10, "Temperature Sensor Failure"}
};

void HeaterUart::setup() {
    ESP_LOGCONFIG(TAG, "Setting up Heater UART...");
}

void HeaterUart::loop() {
    const int FRAME_SIZE = 48;
    const int TX_FRAME_END_INDEX = 23;
    const int RX_FRAME_START_INDEX = 24;
    const uint8_t END_OF_FRAME_MARKER = 0x00;

    while (available()) {
        uint8_t byte = read();
        if (waiting_for_start_) {
            if (byte == 0x76) {
                frame_[frame_index_++] = byte;
                waiting_for_start_ = false;
            }
        } else {
            frame_[frame_index_++] = byte;
            if (frame_index_ == TX_FRAME_END_INDEX + 1) {
                if (frame_[21] == END_OF_FRAME_MARKER) {
                    ESP_LOGW("heater_uart", "Invalid Transmit Packet. Resetting frame.");
                    reset_frame();
                    return;
                }
            }
            if (frame_index_ == FRAME_SIZE) {
                if (frame_[45] == END_OF_FRAME_MARKER && frame_[RX_FRAME_START_INDEX] == 0x76) {
                    // 1. Calculate CRC for the Heater's Response (Rx Packet)
                    // Rx packet starts at index 24. We check the first 22 bytes.
                    uint16_t calc_crc = calculate_crc16(&frame_[24], 22);

                    // 2. Extract the received CRC (Last 2 bytes of the packet)
                    // Rx CRC is at indices 46 (MSB) and 47 (LSB)
                    uint16_t recv_crc = (frame_[46] << 8) | frame_[47];

                    // 3. Verify
                    if (calc_crc == recv_crc) {
                        parse_frame(frame_, FRAME_SIZE);
                    } else {
                        crc_error_count_value_++;
                        ESP_LOGW(TAG, "CRC Mismatch! Calc: 0x%04X, Recv: 0x%04X", calc_crc, recv_crc);
                    }

                } else {
                    ESP_LOGW("heater_uart", "Invalid Receive Packet or incorrect order. Resetting frame.");
                }
                reset_frame();
            }
        }
    }
}

void HeaterUart::update() {
    for (const auto &sensor_entry : sensors_) {
        const std::string &key = sensor_entry.first;
        sensor::Sensor *sensor = sensor_entry.second;

        if (key == "current_temperature")
            sensor->publish_state(current_temperature_value_);
        else if (key == "fan_speed")
            sensor->publish_state(fan_speed_value_);
        else if (key == "supply_voltage")
            sensor->publish_state(supply_voltage_value_);
        else if (key == "heat_exchanger_temp")
            sensor->publish_state(heat_exchanger_temp_value_);
        else if (key == "glow_plug_voltage")
            sensor->publish_state(glow_plug_voltage_value_);
        else if (key == "glow_plug_current")
            sensor->publish_state(glow_plug_current_value_);
        else if (key == "pump_frequency")
            sensor->publish_state(pump_frequency_value_);
        else if (key == "fan_voltage")
            sensor->publish_state(fan_voltage_value_);
        else if (key == "desired_temperature")
            sensor->publish_state(desired_temperature_value_);
        else if (key == "crc_errors")
            sensor->publish_state(crc_error_count_value_);
    }

    for (const auto &text_entry : text_sensors_) {
        const std::string &key = text_entry.first;
        text_sensor::TextSensor *text_sensor = text_entry.second;

        if (key == "run_state")
            text_sensor->publish_state(run_state_description_);
        else if (key == "error_code")
            text_sensor->publish_state(error_code_description_);
    }

    for (const auto &binary_entry : binary_sensors_) {
        const std::string &key = binary_entry.first;
        binary_sensor::BinarySensor *binary_sensor = binary_entry.second;

        if (key == "on_off_state")
            binary_sensor->publish_state(on_off_value_);
    }
}

void HeaterUart::parse_frame(const uint8_t *frame, size_t length) {
    if (length != 48) {
        ESP_LOGW(TAG, "Invalid frame length: %d bytes (expected 48)", length);
        return;
    }

    const uint8_t *command_frame = &frame[0];
    const uint8_t *response_frame = &frame[24];

    current_temperature_value_ = command_frame[3];
    desired_temperature_value_ = command_frame[4];
    fan_speed_value_ = (response_frame[6] << 8) | response_frame[7];
    supply_voltage_value_ = ((response_frame[4] << 8) | response_frame[5]) * 0.1;
    heat_exchanger_temp_value_ = ((response_frame[10] << 8) | response_frame[11]);
    glow_plug_voltage_value_ = ((response_frame[12] << 8) | response_frame[13]) * 0.1;
    glow_plug_current_value_ = ((response_frame[14] << 8) | response_frame[15]) * 0.01;
    pump_frequency_value_ = response_frame[16] * 0.1;
    fan_voltage_value_ = ((response_frame[8] << 8) | response_frame[9]) * 0.1;
    run_state_value_ = response_frame[2];
    on_off_value_ = response_frame[3] == 1;
    error_code_value_ = response_frame[17];

    run_state_description_ = run_state_map.count(run_state_value_)
                                ? run_state_map.at(run_state_value_)
                                : "Unknown Run State";

    error_code_description_ = error_code_map.count(error_code_value_)
                                ? error_code_map.at(error_code_value_)
                                : "Unknown Error Code";
}

void HeaterUart::reset_frame() {
    frame_index_ = 0;
    waiting_for_start_ = true;
}

void HeaterUart::set_sensor(const std::string &key, sensor::Sensor *sensor) {
    sensors_[key] = sensor;
}

void HeaterUart::set_text_sensor(const std::string &key, text_sensor::TextSensor *text_sensor) {
    text_sensors_[key] = text_sensor;
}

void HeaterUart::set_binary_sensor(const std::string &key, binary_sensor::BinarySensor *binary_sensor) {
    binary_sensors_[key] = binary_sensor;
}

}  // namespace heater_uart
}  // namespace esphome
