#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>

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
    std::vector<std::string> get_blacklist() const;

private:
    class Impl;
    const std::unique_ptr<Impl> impl;
};

std::ostream& operator<<(std::ostream& os, ble::BlePacket const& p);



}  // namespace ble
