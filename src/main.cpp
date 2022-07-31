#include "prometheus/counter.h"
#include "prometheus/exposer.h"
#include "prometheus/registry.h"

#include <ruuvi/ruuvi.hpp>
#include <ruuvi/ruuvi_prometheus_exposer.hpp>

#include <cassert>
#include <iostream>

#include <csignal>
#include <future>
#include <thread>

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
#ifndef NDBEUG
        std::cout << p << "\n";
#endif
        if (p.manufacturer_id == 0x0499) {
            auto data = ruuvi::convert_data_format_5(p);
#ifndef NDEBUG
            std::cout << data << "\n\n" << std::flush;
#endif
            exposer.update(data);
        }
    }

private:
    ble::BleListener listener;
    ruuvi::RuuviExposer exposer;
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
        } catch (std::exception const& e) {
            // Stop
            stop_all.clear();
            std::cerr << "Runner thread exited with error: " << e.what() << "\n";
        }
    });

    std::thread stopper([&rv]() {
        stop_all.test_and_set();
        while (stop_all.test_and_set()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
        std::cerr << "Stopping...\n";
        rv.stop();
    });

    std::signal(SIGTERM, stop_handler);
    std::signal(SIGINT, stop_handler);

    stopper.join();
    runner.join();
}
