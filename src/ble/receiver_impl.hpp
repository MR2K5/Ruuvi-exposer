#pragma once

#include <ble/receiver.hpp>

#include <atomic>
#include <mutex>

#include <sdbus-c++/sdbus-c++.h>

namespace ble {

class BleListener::Impl {
public:
    Impl(std::function<listener_callback> cb, std::string const& nm);
    ~Impl();

    void stop() noexcept;
    void start();

    void blacklist(std::string const& mac);
    std::vector<std::string> get_blacklist() const;

    bool is_discovering() const;

private:
    std::function<listener_callback> callback_;
    std::string adapter_name;

    std::unique_ptr<sdbus::IConnection> connection;
    std::unique_ptr<sdbus::IProxy> manager;
    std::unique_ptr<sdbus::IProxy> objmanager;

    std::map<sdbus::ObjectPath,
             std::pair<std::unique_ptr<sdbus::IProxy>, std::string>>
        listeners;
    std::mutex listeners_mtx;

    std::vector<std::string> blist;
    mutable std::mutex blist_mtx;

    std::atomic_bool should_discover   = false;
    std::atomic_bool exited_with_error = false;

    void add_cb(
        sdbus::ObjectPath const& obj,
        std::map<std::string, std::map<std::string, sdbus::Variant>> const& m);
    void rem_cb(sdbus::ObjectPath const& obj,
                std::vector<std::string> const& interfaces);
    void
    discovery_failed_cb(sdbus::ObjectPath const& obj,
                        std::string const& interface,
                        std::map<std::string, sdbus::Variant> const& changed,
                        std::vector<std::string> const& invalid);
    void properties_cb(sdbus::ObjectPath const& obj,
                       std::string const& interface,
                       std::map<std::string, sdbus::Variant> const& changed,
                       std::vector<std::string> const& invalid);

    void emit_packet(sdbus::ObjectPath const& obj);

    void create_connection();
    void start_discovery();
    void stop_discovery();
    bool retry_discovery(int times                 = 2,
                         std::chrono::seconds wait = std::chrono::seconds(1));
};

}  // namespace ble
