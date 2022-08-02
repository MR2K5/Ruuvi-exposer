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
}
