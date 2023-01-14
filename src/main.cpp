#include <logging/logging.hpp>
#include <prometheus/counter.h>
#include <prometheus/exposer.h>
#include <prometheus/registry.h>
#include <ruuvi/ruuvi.hpp>
#include <ruuvi/ruuvi_prometheus_exposer.hpp>
#include <sysinfo/system_info_exposer.hpp>

#include <cassert>
#include <iostream>

#include <csignal>
#include <future>
#include <thread>

using logging::log;

class Ruuvitag {
public:
    Ruuvitag()
        : listener(
            std::bind(&Ruuvitag::ble_callback, this, std::placeholders::_1)),
          exposer("0.0.0.0:9105"),
          rvexposer(std::make_shared<ruuvi::RuuviExposer>())
#ifdef ENABLE_SYSINFO_EXPOSER
          ,
          sysinfo(sys_info::SystemInfoCollector::create())
#endif
    {
        exposer.RegisterCollectable(rvexposer);
        exposer.RegisterCollectable(sysinfo);
        log("Collectables registered");
    }
    Ruuvitag(Ruuvitag const&)            = delete;
    Ruuvitag& operator=(Ruuvitag const&) = delete;

    void start() {
        log("Starting ble listener");
        listener.start();
    }
    void stop() {
        log("Stopping ble listener");
        listener.stop();
    }

    void ble_callback(ble::BlePacket const& p) {
        // log(p);
        if (p.manufacturer_id == 0x0499) {
            auto data = ruuvi::convert_data_format_5(p);
            //            log(data);
            rvexposer->update(data);
            if (data.contains_errors) {
                log("Ruuvitag message errors from ", data.mac, ": ",
                    data.error_msg);
            }
        } else {
            listener.blacklist(p.mac);
        }
    }

private:
    ble::BleListener listener;
    prometheus::Exposer exposer;
    std::shared_ptr<ruuvi::RuuviExposer> rvexposer;
    std::shared_ptr<sys_info::SystemInfoCollector> sysinfo;
};

namespace {
std::atomic_flag stop_all           = ATOMIC_FLAG_INIT;
std::atomic_bool stopped_with_error = false;
}

void stop_handler(int) {
    //
    stop_all.clear();
}

int main() {
    try {
        Ruuvitag rv;
        stop_all.test_and_set();

        std::thread runner([&rv]() {
            try {
                rv.start();
            } catch (std::exception const& e) {
                // Stop
                stop_all.clear();
                stopped_with_error = true;
                std::cerr << "Runner thread exited with error: " << e.what()
                          << "\n";
            }
        });

        std::thread stopper([&rv]() {
            while (stop_all.test_and_set()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
            log("Stopping...");
            rv.stop();
        });

        std::signal(SIGTERM, stop_handler);
        std::signal(SIGINT, stop_handler);

        std::this_thread::sleep_for(std::chrono::seconds(1));

        stopper.join();
        runner.join();
    } catch (std::exception const& e) {
        logging::log("Uncaught exception: ", e.what());
        stopped_with_error = true;
    } catch (...) {
        logging::log("Uncaught exception of unknown type in main()");
        stopped_with_error = true;
    }
    return stopped_with_error ? EXIT_FAILURE : EXIT_SUCCESS;
}
