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
    std::string mac;
    std::string device_name;
    uint16_t manufacturer_id;
    std::vector<uint8_t> manufacturer_data;
    int16_t signal_strength;
};

using listener_callback = void(BlePacket const&);

class BleListener {
public:
    explicit BleListener(std::function<listener_callback> f,
                         std::string const& nm = "hci0");
    ~BleListener();

    void start();
    void stop() noexcept;
    void blacklist(std::string const& mac);

private:
    class Impl;
    const std::unique_ptr<Impl> impl;
};

std::ostream& operator<<(std::ostream& os, ble::BlePacket const& p);

}  // namespace ble

// -------------------------------------------------------------------------

namespace ruuvi {

struct ruuvi_data_format_5 {
    static constexpr auto nan = std::numeric_limits<float>::quiet_NaN();
    float temperature         = nan;
    float humidity            = nan;
    uint32_t pressure         = -1;
    std::array<float, 3> acceleration{nan, nan, nan};
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
struct ruuvi_data_format_3 {
    static constexpr auto nan = std::numeric_limits<float>::quiet_NaN();
    float humidity            = nan;
    float temperature         = nan;
    uint32_t pressure         = -1;
    std::array<float, 3> acceleration{nan, nan, nan};
    float battery_voltage = nan;
    int16_t signal_strength = -32636;
    std::string mac;
    std::string error_msg;
    bool contains_errors = true;
};

std::ostream& operator<<(std::ostream& os, ruuvi_data_format_5 const& data);
std::ostream& operator<<(std::ostream& os, ruuvi_data_format_3 const& data);

inline constexpr int unknown_format = -1;
inline constexpr int not_ruuvitag = -2;
int identify_format(ble::BlePacket const& p);
ruuvi_data_format_5 convert_data_format_5(ble::BlePacket const& p,
                                          bool throw_on_error = false);
ruuvi_data_format_3 convert_data_format_3(ble::BlePacket const& o, bool throw_on_error = false);

}  // namespace ruuvi
