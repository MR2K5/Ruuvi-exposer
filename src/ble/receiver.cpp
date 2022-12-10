#include "receiver.hpp"

#include "logging/logging.hpp"

using namespace ble;
using namespace logging;

BleListener::Impl::Impl(std::function<listener_callback> cb, const std::string& nm)
    : callback_(std::move(cb)), adapter_name(nm), started(false) {
    if (!callback_) { throw std::logic_error("BleListener initialized with mpty callback"); }
}

void BleListener::Impl::start() {
    connection = sdbus::createConnection();

    manager = sdbus::createProxy(*connection, "org.bluez", "/org/bluez/hci0");

    std::map<std::string, sdbus::Variant> dict;
    dict["DuplicateData"] = sdbus::Variant(true);
    manager->callMethod("SetDiscoveryFilter")
        .onInterface("org.bluez.Adapter1")
        .withArguments(dict)
        .storeResultsTo();

    auto objmanager = sdbus::createProxy(*connection, "org.bluez", "/");

    objmanager->uponSignal("InterfacesAdded")
        .onInterface("org.freedesktop.DBus.ObjectManager")
        .call([this](sdbus::ObjectPath const& obj,
                     std::map<std::string, std::map<std::string, sdbus::Variant>> const& m) {
            this->add_cb(obj, m);
        });

    //    objmanager->uponSignal("InterfacesRemoved")
    //        .onInterface("org.freedesktop.DBus.ObjectManager")
    //        .call([this](sdbus::Signal& s) { this->rem_cb(s); });

    objmanager->finishRegistration();

    log("Starting bluetooth discovery");
    manager->callMethod("StartDiscovery").onInterface("org.bluez.Adapter1").storeResultsTo();

    started.store(true);
    connection->enterEventLoop();
}

void BleListener::Impl::add_cb(
    sdbus::ObjectPath const& obj,
    std::map<std::string, std::map<std::string, sdbus::Variant>> const&) {

    if (listeners.find(obj) != listeners.end()) { return; }

    logging::log("Added ", obj);

    auto properties = sdbus::createProxy(*connection, "org.bluez", obj);

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
        listeners.insert({ obj, std::move(properties) });
    }
    emit_packet(obj);
}

void BleListener::Impl::rem_cb(sdbus::Signal&) {
    //    log("")
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

    auto& proxy = listeners[obj];

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

void BleListener::Impl::stop() noexcept {
    if (!started.load()) return;
    try {
        log("Stopping bluetooth discovery");
        manager->callMethod("StopDiscovery").onInterface("org.bluez.Adapter1").storeResultsTo();
    } catch (sdbus::Error const& e) {
        logging::log("Failed to stop discovery: ", e.getName(), " - ", e.getMessage());
    }
    try {
        connection->leaveEventLoop();
    } catch (sdbus::Error const& e) {
        log("Failed to stop event loop: ", e.getName(), " - ", e.getMessage());
    }
}
