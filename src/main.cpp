#include <prometheus/counter.h>
#include <prometheus/exposer.h>
#include <prometheus/registry.h>
#include <sysinfo/system_info_exposer.hpp>

#include <ruuvi/ruuvi.hpp>
#include <ruuvi/ruuvi_prometheus_exposer.hpp>

#include <cassert>
#include <iostream>

#include <csignal>
#include <mutex>
#include <thread>

inline constexpr char const* address = "0.0.0.0:9105";

std::unique_ptr<prometheus::Exposer> start_exposer() {
    return std::make_unique<prometheus::Exposer>(address);
}

int main() {
    auto exposer = start_exposer();

    auto reg      = std::make_shared<prometheus::Registry>();
    auto& c       = prometheus::BuildCounter().Name("TEST").Help("").Register(*reg);
    auto& counter = c.Add({});
    counter.Increment();
    exposer->RegisterCollectable(reg);

    std::this_thread::sleep_for(std::chrono::seconds(5));
    std::terminate();
}
