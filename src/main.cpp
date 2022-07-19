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

int main() {
    Ruuvitag rv;

    std::thread runner([&rv]() { rv.start(); });

    std::string line;
    //    std::this_thread::sleep_for(std::chrono::seconds(10));
    while (std::cin.good() && std::getline(std::cin, line))
        ;

    rv.stop();
    runner.join();
}
