#include "receiver.hpp"
#include "receiver_impl.hpp"

#include "logging/logging.hpp"

#include <thread>

using namespace ble;
using namespace logging;

BleListener::BleListener(std::function<listener_callback> cb,
                         std::string const& nm)
    : impl(std::make_unique<Impl>(std::move(cb), nm)) {}

BleListener::~BleListener() = default;

void BleListener::start() { impl->start(); }
void BleListener::stop() noexcept { impl->stop(); }

void BleListener::blacklist(std::string const& mac) { impl->blacklist(mac); }


BleListener::Impl::Impl(std::function<listener_callback> cb, const std::string& nm)
    : callback_(std::move(cb)), adapter_name(nm) {
    if (!callback_) { throw std::logic_error("BleListener initialized with mpty callback"); }
    create_connection();
}

BleListener::Impl::~Impl() {
    stop();
}

void BleListener::Impl::create_connection() {
    connection = sdbus::createConnection();

    manager = sdbus::createProxy(*connection, "org.bluez", "/org/bluez/hci0");

    objmanager = sdbus::createProxy(*connection, "org.bluez", "/");

    objmanager->uponSignal("InterfacesAdded")
        .onInterface("org.freedesktop.DBus.ObjectManager")
        .call([this](sdbus::ObjectPath const& obj,
                     std::map<std::string, std::map<std::string, sdbus::Variant>> const& m) {
            this->add_cb(obj, m);
        });

    manager->uponSignal("PropertiesChanged")
        .onInterface("org.freedesktop.DBus.Properties")
        .call([this](std::string const& interface,
                     std::map<std::string, sdbus::Variant> const& changed,
                     std::vector<std::string> const& invalid) {
            this->discovery_failed_cb("/org/bluez/hci0", interface, changed, invalid);
        });

    manager->finishRegistration();
    objmanager->finishRegistration();
}

void BleListener::Impl::start() {
    start_discovery();
    connection->enterEventLoop();
    if (exited_with_error) throw std::runtime_error("BleListener exited with error");
}

void BleListener::Impl::blacklist(const std::string& mac) {
    log("Blacklisting ", mac);
    {
        std::lock_guard g(blist_mtx);
        if (std::find(blist.begin(), blist.end(), mac) != blist.end())  // Duplicate
            return;
        blist.push_back(mac);
    }

    std::lock_guard g(listeners_mtx);
    for (auto& [key, val] : listeners) {
        if (val.second == mac) {
            listeners.erase(key);
            break;
        }
    }
}

void BleListener::Impl::add_cb(
    sdbus::ObjectPath const& obj,
    [[maybe_unused]] std::map<std::string, std::map<std::string, sdbus::Variant>> const& interfaces) {

    try {
        if (listeners.find(obj) != listeners.end()) { return; }

        auto properties = sdbus::createProxy(*connection, "org.bluez", obj);
        // Get mac
        sdbus::Variant macv;
        properties->callMethod("Get")
            .onInterface("org.freedesktop.DBus.Properties")
            .withArguments("org.bluez.Device1", "Address")
            .storeResultsTo(macv);
        auto mac = macv.get<std::string>();
        {
            std::lock_guard g(blist_mtx);
            if (std::find(blist.begin(), blist.end(), mac) != blist.end()) {
                // mac blacklisted
                return;
            }
        }

        logging::log("Added ", obj);

        properties->uponSignal("PropertiesChanged")
            .onInterface("org.freedesktop.DBus.Properties")
            .call([this, obj](std::string const& interface,
                              std::map<std::string, sdbus::Variant> const& changed,
                              std::vector<std::string> const& invalid) {
                this->properties_cb(obj, interface, changed, invalid);
            });

        properties->finishRegistration();

        {
            std::lock_guard g(listeners_mtx);
            listeners.insert({ obj, std::make_pair(std::move(properties), mac) });
        }
        emit_packet(obj);
    } catch (sdbus::Error const& e) {
        log("Failed to add device: ", e.getName(), " - ", e.getMessage());
    }
}

void BleListener::Impl::rem_cb(sdbus::ObjectPath const& obj,
                               std::vector<std::string> const& interfaces) {

    bool found = false;
    for (auto& i : interfaces) {
        if (i == "org.bluez.Device1") {
            found = true;
            break;
        }
    }
    if (!found) return;

    log("Removed ", obj);
    std::lock_guard g(listeners_mtx);
    auto p = listeners.find(obj);
    if (p != listeners.end()) { listeners.erase(p); }
}

void BleListener::Impl::discovery_failed_cb(const sdbus::ObjectPath& /*obj*/,
                                            const std::string& interface,
                                            const std::map<std::string, sdbus::Variant>& changed,
                                            const std::vector<std::string>& /*invalid*/) {
    log("Discovery parameters changed");

    if (interface != "org.bluez.Adapter1") return;
    if (should_discover == false) return;

    auto p = changed.find("Discovering");
    if (p != changed.end()) {
        bool new_state = p->second.get<bool>();
        if (new_state == false) {
            log("Restarting discovery");
            if (!retry_discovery()) {
                should_discover   = false;
                exited_with_error = true;
                stop();
            }
        }
    }
}

bool BleListener::Impl::retry_discovery(int times, std::chrono::seconds wait) {
    for (int i = 0; i < times; ++i) {
        std::this_thread::sleep_for(wait);
        try {
            start_discovery();
            return true;
        } catch (sdbus::Error const& e) {
            log("Failed to restart discovery, ", times - i, " remaining: ", e.getName(), " - ",
                e.getMessage());
        }
    }
    return false;
}

void BleListener::Impl::properties_cb(sdbus::ObjectPath const& obj,
                                      std::string const& /*interface*/,
                                      std::map<std::string, sdbus::Variant> const& changed,
                                      std::vector<std::string> const& /*invalid*/) {

    auto md = changed.find("ManufacturerData");
    if (md != changed.end()) { emit_packet(obj); }
}

void BleListener::Impl::emit_packet(const sdbus::ObjectPath& obj) {
    BlePacket packet;
    const std::string intf = "org.bluez.Device1";

    auto& proxy = listeners[obj].first;

    std::map<std::string, sdbus::Variant> properties_;
    proxy->callMethod("GetAll")
        .onInterface("org.freedesktop.DBus.Properties")
        .withArguments(intf)
        .storeResultsTo(properties_);
    auto const& properties = std::as_const(properties_);

    auto end = properties.end();
    auto mac = properties.find("Address");
    if (mac != end) { packet.mac = mac->second.get<std::string>(); }
    auto rssi = properties.find("RSSI");
    if (rssi != end) { packet.signal_strength = rssi->second.get<int16_t>(); }
    auto name = properties.find("Name");
    if (name != end) { packet.device_name = name->second.get<std::string>(); }

    auto md = properties.find("ManufacturerData");
    if (md != end) {
        auto dict = md->second.get<std::map<uint16_t, sdbus::Variant>>();
        assert(dict.size() <= 1);
        if (dict.size() == 1) {
            for (auto& d : dict) {
                packet.manufacturer_id   = d.first;
                packet.manufacturer_data = d.second.get<std::vector<uint8_t>>();
            }
        }
    }

    callback_(packet);
}

void BleListener::Impl::start_discovery() {
    log("Starting bluetooth discovery");

    std::map<std::string, sdbus::Variant> dict;
    dict["DuplicateData"] = sdbus::Variant(true);
    manager->callMethod("SetDiscoveryFilter")
        .onInterface("org.bluez.Adapter1")
        .withArguments(dict)
        .storeResultsTo();

    should_discover = true;
    manager->callMethod("StartDiscovery").onInterface("org.bluez.Adapter1").storeResultsTo();
}

void BleListener::Impl::stop_discovery() {
    log("Stopping bluetooth discovery");
    if (should_discover) {
        should_discover = false;
        manager->callMethod("StopDiscovery").onInterface("org.bluez.Adapter1").storeResultsTo();
    }
}

void BleListener::Impl::stop() noexcept {
    try {
        stop_discovery();
    } catch (sdbus::Error const& e) {
        log("Failed to stop discovery: ", e.getName(), " - ", e.getMessage());
    }
    connection->leaveEventLoop();
}

bool BleListener::Impl::is_discovering() const {
    bool r = false;
    manager->callMethod("Get")
        .onInterface("org.freedesktop.DBus.Properties")
        .withArguments("org.bluez.Adapter1", "Discovering")
        .storeResultsTo(r);

    return r;
}
