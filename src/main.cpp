#include <logging/logging.hpp>
#include <prometheus/counter.h>
#include <prometheus/exposer.h>
#include <prometheus/registry.h>
#include <ruuvi/ruuvi.hpp>
#include <ruuvi/ruuvi_prometheus_exposer.hpp>
#include <sysinfo/diskstat_exposer.hpp>
#include <sysinfo/system_info_exposer.hpp>

#include <cassert>
#include <iostream>

#include <csignal>
#include <future>
#include <thread>
#include <spdlog/sinks/systemd_sink.h>

class Ruuvitag {
public:
    Ruuvitag()
        : listener(
            std::bind(&Ruuvitag::ble_callback, this, std::placeholders::_1)),
          exposer("[::]:9105,9105"),
          rvexposer(std::make_shared<ruuvi::RuuviExposer>()),
          sysinfo(sys_info::SystemInfoCollector::create()),
          diskstat(std::make_shared<sys_info::DiskstatExposer>()) {
        exposer.RegisterCollectable(rvexposer);
        exposer.RegisterCollectable(sysinfo);
        exposer.RegisterCollectable(diskstat);
        spdlog::debug("Collectables registered");
    }
    Ruuvitag(Ruuvitag const&)            = delete;
    Ruuvitag& operator=(Ruuvitag const&) = delete;

    void start() {
        spdlog::info("Starting ble listener");
        listener.start();
    }
    void stop() {
        spdlog::info("Stopping ble listener");
        listener.stop();
    }

    void ble_callback(ble::BlePacket const& p) {
        // log(p);
        if (p.manufacturer_id == 0x0499) {
            auto data = ruuvi::convert_data_format_5(p);
            //            log(data);
            rvexposer->update(data);
            if (data.contains_errors) {
                spdlog::info("Ruuvitag message errors from ", data.mac, ": ",
                    data.error_msg);
            }
        } else {
            listener.blacklist(p.mac);
        }
    }

    void print_debug() const noexcept {
        try {
            spdlog::info("\nBlacklisted macs: ");
            for (auto const& mac: listener.get_blacklist()) {
                spdlog::info(mac);
            }

            spdlog::info("");
        } catch (...) {}
    }

private:
    ble::BleListener listener;
    prometheus::Exposer exposer;
    std::shared_ptr<ruuvi::RuuviExposer> rvexposer;
    std::shared_ptr<sys_info::SystemInfoCollector> sysinfo;
    std::shared_ptr<sys_info::DiskstatExposer> diskstat;
};

namespace {
std::atomic_flag stop_all           = ATOMIC_FLAG_INIT;
std::atomic_bool stopped_with_error = false;
std::atomic_flag debug_print = ATOMIC_FLAG_INIT;
}

extern "C" void stop_handler(int) {
    //
    stop_all.clear();
}

extern "C" void sigusr_handler(int) {
    debug_print.clear(std::memory_order_relaxed);
}

void config_logger() {
    spdlog::flush_on(spdlog::level::err);
}

int main() {
    try {
        config_logger();

        Ruuvitag rv;
        stop_all.test_and_set();
        debug_print.test_and_set();

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
                if (debug_print.test_and_set(std::memory_order_relaxed) == false) {
                    rv.print_debug();
                }
            }
            spdlog::info("Stopping...");
            rv.stop();
        });

        std::signal(SIGTERM, stop_handler);
        std::signal(SIGINT, stop_handler);
        std::signal(SIGUSR1, sigusr_handler);

        std::this_thread::sleep_for(std::chrono::seconds(1));

        stopper.join();
        runner.join();
    } catch (std::exception const& e) {
        spdlog::error("Uncaught exception: ", e.what());
        stopped_with_error = true;
    } catch (...) {
        spdlog::error("Uncaught exception of unknown type in main()");
        stopped_with_error = true;
    }
    return stopped_with_error ? EXIT_FAILURE : EXIT_SUCCESS;
}
