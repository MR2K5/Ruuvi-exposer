#include "ruuvi.hpp"

#include <cassert>
#include <cmath>
#include <future>
#include <iomanip>

#include <gattlib.h>

using namespace ble;

namespace {

std::string gattlib_error_string(int err) {
    assert(err != 0 && "throwing error with success code");
    switch (err) {
    case GATTLIB_INVALID_PARAMETER: return "Invalid parameter";
    case GATTLIB_NOT_FOUND: return "Device not found";
    case GATTLIB_OUT_OF_MEMORY: return "Out of memory";
    case GATTLIB_NOT_SUPPORTED: return "Not supported";
    case GATTLIB_DEVICE_ERROR: return "Device error";
    case GATTLIB_ERROR_DBUS: return "Dbus error";
    case GATTLIB_ERROR_BLUEZ: return "Bluez error";
    case GATTLIB_ERROR_INTERNAL: return "Internal error";
    default: return "Unknown error";
    }
}

void check_gatt_errors(int err) {
    if (err != 0) { throw gattlib_error(err); }
}

template<typename I> std::string n2hexstr(I w, size_t hex_len = sizeof(I) << 1) {
    static constexpr char digits[] = "0123456789ABCDEF";
    std::string rc(hex_len, '0');
    for (size_t i = 0, j = (hex_len - 1) * 4; i < hex_len; ++i, j -= 4)
        rc[i] = digits[(w >> j) & 0x0f];
    return rc;
}

}  // namespace

gattlib_error::gattlib_error(int err)
    : std::runtime_error(
        ("Gattlib error " + std::to_string(err) + ": ").append(gattlib_error_string(err))) {}

class BleListener::Impl {
public:
    Impl(std::function<listener_callback> cb, std::string const& nm)
        : callback_(std::move(cb)), adapter_(nullptr), adapter_name(nm) {
        if (!callback_) { throw std::logic_error("BleListener initialized with mpty callback"); }
    }

    void start();
    void stop() {
        int err;
        if (scan_enabled) {
            err          = gattlib_adapter_scan_disable(adapter_);
            scan_enabled = false;
            check_gatt_errors(err);
        }
        if (adapter_open) {
            err          = gattlib_adapter_close(adapter_);
            adapter_open = false;
            check_gatt_errors(err);
        }
    }

private:
    static void gatt_internal_cb(void* adapter, char const* addr, char const* name,
                                 void* user_data) {
        auto* impl = reinterpret_cast<BleListener::Impl*>(user_data);

        gattlib_advertisement_data_t* data;
        size_t data_count;
        uint16_t man_id;
        uint8_t* man_data;
        size_t man_count;
        int err = gattlib_get_advertisement_data_from_mac(adapter, addr, &data, &data_count,
                                                          &man_id, &man_data, &man_count);
        check_gatt_errors(err);

        BlePacket packet;
        packet.adapter = adapter;
        packet.mac     = addr;
        if (name) packet.device_name = name;
        packet.manufacturer_id        = man_id;
        packet.manufacturer_data      = man_data;
        packet.manufacturer_data_size = man_count;
        err = gattlib_get_rssi_from_mac(adapter, addr, &packet.signal_strength);
        check_gatt_errors(err);

        impl->callback_(packet);
    }

    const std::function<listener_callback> callback_;
    void* adapter_;
    const std::string adapter_name;
    bool adapter_open = false;
    bool scan_enabled = false;
};

void BleListener::Impl::start() {
    int err = 0;
    gattlib_adapter_open(adapter_name.empty() ? nullptr : adapter_name.c_str(), &adapter_);
    check_gatt_errors(err);
    adapter_open = true;

    scan_enabled = true;
    gattlib_adapter_scan_enable_with_filter(
        adapter_, nullptr, 0, GATTLIB_DISCOVER_FILTER_NOTIFY_CHANGE, &gatt_internal_cb, 0, this);
    scan_enabled = false;
    check_gatt_errors(err);
}

BleListener::BleListener(std::function<listener_callback> cb, std::string const& nm)
    : impl(std::make_unique<Impl>(std::move(cb), nm)) {}

BleListener::~BleListener() = default;

void BleListener::start() {
    impl->start();
}
void BleListener::stop() {
    impl->stop();
}

// ------------------------------------------------------------------------------------

float ruuvi_data_format_5::acceleration_total() const {
    return std::hypot(acceleration[0], acceleration[1], acceleration[2]);
}

ruuvi_data_format_5 ble::convert_data_format_5(BlePacket const& p, bool throw_on_error) {
    ruuvi_data_format_5 result;
    auto* data = p.manufacturer_data;

    auto _throw = [throw_on_error, &result](std::string const& s) {
        if (throw_on_error) throw std::runtime_error("Data format 5 converison failed: " + s);
        if (!result.error_msg.empty()) result.error_msg += " - ";
        result.error_msg += s;
        result.contains_errors = true;
    };

    if (p.manufacturer_data_size != 24) {
        _throw("Expected data szie 24, got " + std::to_string(p.manufacturer_data_size));
    }
    if (data[0] != 0x05) _throw("Expected data format 5, got " + std::to_string(data[0]));

    // Read values
    uint16_t temperature    = (static_cast<uint16_t>(data[1]) << 8u) | data[2];
    uint16_t humidity       = (static_cast<uint16_t>(data[3]) << 8u) | data[4];
    uint16_t pressure       = (static_cast<uint16_t>(data[5]) << 8u) | data[6];
    uint16_t acceleration_x = (static_cast<uint16_t>(data[7] << 8u) | data[8]);
    uint16_t acceleration_y = (static_cast<uint16_t>(data[9] << 8u) | data[10]);
    uint16_t acceleration_z = (static_cast<uint16_t>(data[11] << 8u) | data[12]);
    uint16_t battery_voltage =
        (static_cast<uint16_t>(data[13]) << 3u) | ((data[14] & 0b1110'0000u) >> 5u);
    int8_t tx_power               = data[14] & 0b0001'1111u;
    uint8_t movement_counter      = data[15];
    uint16_t measurement_sequence = (static_cast<uint16_t>(data[16]) << 8u) | data[17];

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
        result.temperature = 0.005 * static_cast<int16_t>(temperature);

    if (humidity == 0xFFFF)
        _throw("Humidity 0xFFFF invalid");
    else if (humidity > 40'000)
        _throw("Humidity > 40 000 (100%) invalid");
    else
        result.humidity = 0.0025 * humidity;

    if (pressure == 0xFFFF)
        _throw("Pressure 0xFFFF invalid");
    else
        result.pressure = pressure + 50'000;

    if (acceleration_x == 0x8000)
        _throw("X-acceleration 0x8000 invalid");
    else
        result.acceleration[0] = static_cast<int16_t>(acceleration_x) / 1000.0;
    if (acceleration_y == 0x8000)
        _throw("Y-acceleration 0x8000 invalid");
    else
        result.acceleration[1] = static_cast<int16_t>(acceleration_y) / 1000.0;
    if (acceleration_z == 0x8000)
        _throw("Z-acceleration 0x8000 invalid");
    else
        result.acceleration[2] = static_cast<int16_t>(acceleration_z) / 1000.0;

    if (battery_voltage == 2047)
        _throw("Battery voltage 2047 invalid");
    else
        result.battery_voltage = 1.6 + battery_voltage / 1000.0;

    if (measurement_sequence == 65535)
        _throw("Measurement sequence number 65535 invalid");
    else
        result.measurement_sequence = measurement_sequence;

    if (tx_power == 31)
        _throw("Tx power 31 invalid");
    else
        result.tx_power = -40 + tx_power * 2;

    if (movement_counter == 255)
        _throw("Movement counter 255 invalid");
    else
        result.movement_counter = movement_counter;

    if (p.mac != packet_mac) {
        _throw("Receiver and packet MAC addresses differ");
        result.mac = "";
    } else
        result.mac = std::move(packet_mac);

    result.signal_strength = p.signal_strength;

    return result;
}

std::ostream& ble::operator<<(std::ostream& os, const ruuvi_data_format_5& data) {
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
    os << std::showbase << std::hex << std::setfill('0') << std::setw(2) << std::right;
    for (size_t i = 0; i < p.manufacturer_data_size; ++i) { os << +p.manufacturer_data[i] << ' '; }
    os.flags(fmt);

    os << "\n";
    return os;
}
