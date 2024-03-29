#include "ruuvi.hpp"

#include <cassert>
#include <cmath>
#include <future>
#include <iomanip>

using namespace ble;
using namespace ruuvi;

namespace {

template<typename I>
std::string n2hexstr(I w, size_t hex_len = sizeof(I) << 1) {
    static constexpr char digits[] = "0123456789ABCDEF";
    std::string rc(hex_len, '0');
    for (size_t i = 0, j = (hex_len - 1) * 4; i < hex_len; ++i, j -= 4)
        rc[i] = digits[(w >> j) & 0x0f];
    return rc;
}

}  // namespace
// ------------------------------------------------------------------------------------

float ruuvi_data_format_5::acceleration_total() const {
    return std::hypot(acceleration[0], acceleration[1], acceleration[2]);
}

int ruuvi::identify_format(BlePacket const& p) {
    if (p.manufacturer_id != 0x0499) return not_ruuvitag;
    if (p.manufacturer_data[0] == 0x03) return 3;
    if (p.manufacturer_data[0] == 0x04) return 4;
    if (p.manufacturer_data[0] == 0x05) return 5;
    if (p.manufacturer_data[0] == 0x08) return 8;
    return unknown_format;
}

ruuvi_data_format_5 ruuvi::convert_data_format_5(BlePacket const& p,
                                                 bool throw_on_error) {
    ruuvi_data_format_5 result;
    auto& data = p.manufacturer_data;

    auto _throw = [throw_on_error, &result](std::string const& s) {
        if (throw_on_error)
            throw std::runtime_error("Data format 5 converison failed: " + s);
        if (!result.error_msg.empty()) result.error_msg += " - ";
        result.error_msg       += s;
        result.contains_errors = true;
    };

    if (p.manufacturer_data.size() != 24) {
        _throw("Expected data size 24, got "
               + std::to_string(p.manufacturer_data.size()));
    }
    if (data[0] != 0x05)
        _throw("Expected data format 5, got " + std::to_string(data[0]));

    // Read values
    uint16_t temperature    = (static_cast<uint16_t>(data[1]) << 8u) | data[2];
    uint16_t humidity       = (static_cast<uint16_t>(data[3]) << 8u) | data[4];
    uint16_t pressure       = (static_cast<uint16_t>(data[5]) << 8u) | data[6];
    uint16_t acceleration_x = (static_cast<uint16_t>(data[7] << 8u) | data[8]);
    uint16_t acceleration_y = (static_cast<uint16_t>(data[9] << 8u) | data[10]);
    uint16_t acceleration_z =
        (static_cast<uint16_t>(data[11] << 8u) | data[12]);
    uint16_t battery_voltage = (static_cast<uint16_t>(data[13]) << 3u)
                             | ((data[14] & 0b1110'0000u) >> 5u);
    int8_t tx_power          = data[14] & 0b0001'1111u;
    uint8_t movement_counter = data[15];
    uint16_t measurement_sequence =
        (static_cast<uint16_t>(data[16]) << 8u) | data[17];

    std::string packet_mac;
    packet_mac.reserve(18);
    for (size_t i = 18; i < 24; ++i) {
        packet_mac += (n2hexstr(data[i], 2));
        packet_mac.push_back(':');
    }
    packet_mac.pop_back();

    result.contains_errors = false;

    // Error checking
    // Convert values and populate result
    if (temperature == 0x8000)
        _throw("Temperature 0x8000 invalid");
    else
        result.temperature = 0.005f * static_cast<int16_t>(temperature);

    if (humidity == 0xFFFF)
        _throw("Humidity 0xFFFF invalid");
    else if (humidity > 40'000)
        _throw("Humidity > 40 000 (100%) invalid");
    else
        result.humidity = 0.0025f * humidity;

    if (pressure == 0xFFFF)
        _throw("Pressure 0xFFFF invalid");
    else
        result.pressure = uint32_t(pressure) + 50'000;

    if (acceleration_x == 0x8000)
        _throw("X-acceleration 0x8000 invalid");
    else
        result.acceleration[0] = static_cast<int16_t>(acceleration_x) / 1000.0f;
    if (acceleration_y == 0x8000)
        _throw("Y-acceleration 0x8000 invalid");
    else
        result.acceleration[1] = static_cast<int16_t>(acceleration_y) / 1000.0f;
    if (acceleration_z == 0x8000)
        _throw("Z-acceleration 0x8000 invalid");
    else
        result.acceleration[2] = static_cast<int16_t>(acceleration_z) / 1000.0f;

    if (battery_voltage == 2047)
        _throw("Battery voltage 2047 invalid");
    else
        result.battery_voltage = 1.6f + battery_voltage / 1000.0f;

    //    if (measurement_sequence == 65535)
    //        _throw("Measurement sequence number 65535 invalid");
    //    else
    result.measurement_sequence = measurement_sequence;

    if (tx_power == 31)
        _throw("Tx power 31 invalid");
    else
        result.tx_power = -40 + tx_power * 2;

    //    if (movement_counter == 255)
    //        _throw("Movement counter 255 invalid");
    //    else
    result.movement_counter = movement_counter;

    if (p.mac != packet_mac) {
        _throw("Receiver and packet MAC addresses differ");
        result.mac = "";
    } else
        result.mac = std::move(packet_mac);

    result.signal_strength = p.signal_strength;

    return result;
}

ruuvi_data_format_3 ruuvi::convert_data_format_3(BlePacket const& p,
                                                 bool throw_on_error) {
    ruuvi_data_format_3 result;
    auto& data = p.manufacturer_data;

    auto _throw = [throw_on_error, &result](std::string const& s) {
        if (throw_on_error)
            throw std::runtime_error("Data format 5 converison failed: " + s);
        if (!result.error_msg.empty()) result.error_msg += " - ";
        result.error_msg       += s;
        result.contains_errors = true;
    };

    uint8_t humidity      = data[1];
    int8_t temperature    = ((data[2] & 0x80) == 0 ? 1 : -1) * (data[2] & 0x7F);
    uint8_t temp_fraction = data[3];
    uint16_t pressure     = (uint16_t(data[4]) << 8) | data[5];
    uint16_t accel_x      = (uint16_t(data[6]) << 8) | data[7];
    uint16_t accel_y      = (uint16_t(data[8]) << 8) | data[9];
    uint16_t accel_z      = (uint16_t(data[10]) << 8) | data[11];
    uint16_t voltage      = (uint16_t(data[12]) << 8) | data[13];

    result.contains_errors = false;

    if (humidity > 200) {
        _throw("Humidity > 200 is invalid");
    } else {
        result.humidity = humidity * 0.5;
    }
    if (temp_fraction > 99) {
        _throw("Temperature fraction > 99 is invalid");
    } else {
        result.temperature =
            temperature + ((data[2] & 0x80) ? -0.01f : 0.01f) * temp_fraction;
    }
    result.pressure        = uint32_t(pressure) + 50'000;
    result.acceleration[0] = static_cast<int16_t>(accel_x) * 0.001f;
    result.acceleration[1] = static_cast<int16_t>(accel_y) * 0.001f;
    result.acceleration[2] = static_cast<int16_t>(accel_z) * 0.001f;
    result.battery_voltage = voltage * 0.001f;

    result.signal_strength = p.signal_strength;
    result.mac             = p.mac;

    return result;
}

std::ostream& ruuvi::operator<<(std::ostream& os,
                                ruuvi_data_format_5 const& data) {
    os << "Data from MAC " << data.mac << "\n";
    os << "Ruuvi data format: " << 5;
    os << "\nTemperature: " << data.temperature;
    os << "\nPressure: " << data.pressure;
    os << "\nHumidity: " << data.humidity;
    os << "\nAcceleration-x: " << data.acceleration[0];
    os << "\nAcceleration-y: " << data.acceleration[1];
    os << "\nAcceleration-z: " << data.acceleration[2];
    os << "\nBattery voltage: " << data.battery_voltage;
    os << "\nTx power: " << +data.tx_power;
    os << "\nMovement counter: " << +data.movement_counter;
    os << "\nMeasurement sequence: " << data.measurement_sequence;
    os << "\nRssi signal strength: " << data.signal_strength;
    if (data.contains_errors) { os << "\nErrors: " << data.error_msg; }
    os << "\n";
    return os;
}
std::ostream& ruuvi::operator<<(std::ostream& os,
                                ruuvi_data_format_3 const& data) {
    os << "Data from MAC " << data.mac << "\n";
    os << "Ruuvi data format: " << 5;
    os << "\nTemperature: " << data.temperature;
    os << "\nPressure: " << data.pressure;
    os << "\nHumidity: " << data.humidity;
    os << "\nAcceleration-x: " << data.acceleration[0];
    os << "\nAcceleration-y: " << data.acceleration[1];
    os << "\nAcceleration-z: " << data.acceleration[2];
    os << "\nBattery voltage: " << data.battery_voltage;
    os << "\nRssi signal strength: " << data.signal_strength;
    if (data.contains_errors) { os << "\nErrors: " << data.error_msg; }
    os << "\n";
    return os;
}

std::ostream& ble::operator<<(std::ostream& os, BlePacket const& p) {
    os << "Ble packet MAC: " << p.mac;
    os << "\nDevice name: ";
    if (!p.device_name.empty())
        os << p.device_name;
    else
        os << "{unnamed}";
    os << "\nSignal strength: " << p.signal_strength;
    os << "\nManufacturer id: " << p.manufacturer_id;
    os << "\nManufacturer data: \n";

    auto fmt = os.flags();
    os << std::showbase << std::hex << std::setfill('0') << std::setw(2)
       << std::right;
    for (size_t i = 0; i < p.manufacturer_data.size(); ++i) {
        os << +p.manufacturer_data[i] << ' ';
    }
    os.flags(fmt);

    os << "\n";
    return os;
}
