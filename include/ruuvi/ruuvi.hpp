#pragma once

#include <array>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>

namespace ble {

struct BlePacket {
    void* adapter;
    std::string mac;
    std::string device_name;
    uint16_t manufacturer_id;
    uint8_t* manufacturer_data;
    int16_t signal_strength;
    size_t manufacturer_data_size;
};

class gattlib_error: public std::runtime_error {
public:
    gattlib_error(int err);
};

using listener_callback = void(BlePacket const&);

class BleListener {
public:
    explicit BleListener(std::function<listener_callback> f, std::string const& nm = {});
    ~BleListener();

    void start();
    void stop();

private:
    class Impl;
    const std::unique_ptr<Impl> impl;
};

// -------------------------------------------------------------------------

struct ruuvi_data_format_5 {
    static constexpr auto nan = std::numeric_limits<float>::quiet_NaN();
    float temperature         = nan;
    float humidity            = nan;
    uint32_t pressure         = -1;
    std::array<float, 3> acceleration{ nan, nan, nan };
    float battery_voltage         = nan;
    uint16_t measurement_sequence = -1;
    int8_t tx_power               = std::numeric_limits<int8_t>::min();
    uint8_t movement_counter      = -1;
    int16_t signal_strength       = -32636;
    std::string mac;
    std::string error_msg;
    bool contains_errors = true;

    float acceleration_total() const;
};

std::ostream& operator<<(std::ostream& os, ruuvi_data_format_5 const& data);
std::ostream& operator<<(std::ostream& os, BlePacket const& p);

ruuvi_data_format_5 convert_data_format_5(BlePacket const& p, bool throw_on_error = false);

}  // namespace ble
