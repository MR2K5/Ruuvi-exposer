#include "ruuvi/system_info_exposer.hpp"
#include <ruuvi/ruuvi.hpp>
#include <ruuvi/ruuvi_prometheus_exposer.hpp>

#include <cassert>
#include <iostream>

#include <csignal>
#include <future>
#include <thread>

#include <gattlib.h>

class Ruuvitag {
public:
    Ruuvitag()
        : listener(std::bind(&Ruuvitag::ble_callback, this, std::placeholders::_1)),
          exposer("0.0.0.0:9105") {}
    Ruuvitag(Ruuvitag const&)            = delete;
    Ruuvitag& operator=(Ruuvitag const&) = delete;

    void start() { listener.start(); }
    void stop() { listener.stop(); }

    void ble_callback(ble::BlePacket const& p) {
        std::cout << p << "\n";
        if (p.manufacturer_id == 0x0499) {
            auto data = ble::convert_data_format_5(p);
            std::cout << data << "\n\n" << std::flush;
            exposer.update(data);
        }
    }

private:
    ble::BleListener listener;
    ble::RuuviExposer exposer;
};

std::atomic_flag stop_all = ATOMIC_FLAG_INIT;

void stop_handler(int) {
    stop_all.clear();
}

int main() {
    Ruuvitag rv;

    std::thread runner([&rv]() {
        try {
            rv.start();
        } catch (...) {
            // Stop
            stop_all.clear();
        }
    });

    std::thread stopper([&rv]() {
        stop_all.test_and_set();
        while (stop_all.test_and_set()) { std::this_thread::yield(); }
        std::cerr << "Stopping\n";
        rv.stop();
    });

    std::signal(SIGINT, stop_handler);

    stopper.join();
    runner.join();
}
