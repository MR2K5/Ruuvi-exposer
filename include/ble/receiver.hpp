#pragma once

#include "ruuvi/ruuvi.hpp"

#include <atomic>
#include <sdbus-c++/sdbus-c++.h>

namespace ble {

class BleListener::Impl {
public:
    Impl(std::function<listener_callback> cb, std::string const& nm);

    void start();
    void stop() noexcept;

private:
    std::function<listener_callback> callback_;
    std::string adapter_name;

    std::unique_ptr<sdbus::IConnection> connection;
    std::unique_ptr<sdbus::IProxy> manager;
    std::atomic_bool started;

    std::map<sdbus::ObjectPath, std::unique_ptr<sdbus::IProxy>> listeners;
    std::mutex listeners_mtx;

    void add_cb(sdbus::ObjectPath const& obj,
                std::map<std::string, std::map<std::string, sdbus::Variant>> const& m);
    void rem_cb(sdbus::Signal& s);
    void properties_cb(sdbus::ObjectPath const& obj, std::string const& interface,
                       std::map<std::string, sdbus::Variant> const& changed,
                       std::vector<std::string> const& invalid);

    void emit_packet(sdbus::ObjectPath const& obj);
};

}  // namespace ble
