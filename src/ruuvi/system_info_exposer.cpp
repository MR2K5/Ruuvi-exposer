#include "system_info_exposer.hpp"

#include <atomic>
#include <cassert>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <list>
#include <map>

#include <prometheus/family.h>
#include <prometheus/gauge.h>

#include "raw_gauge.hpp"
#include "system_info.hpp"
#include <logging/logging.hpp>

using namespace sys_info;
using namespace prometheus;
using logging::log;

std::shared_ptr<sys_info::SystemInfoCollector> SystemInfoCollector::create() {
    return std::make_shared<SystemInfoCollector>(init());
}

#ifdef ENABLE_SYSINFO_EXPOSER

template<class F> auto to_double_single(F&& f) {
    return [fn = std::forward<F>(f)](
               system_info const& i) -> std::vector<ClientMetric> {
        std::vector<ClientMetric> ret;
        ClientMetric& m = ret.emplace_back();
        m.gauge.value   = static_cast<double>(std::invoke(fn, i));
        return ret;
    };
}

class SystemInfoCollector::Impl {
public:
    Impl() { create_gauges(); }

    std::vector<MetricFamily> Collect() const {
        auto info = system_info::create();

        std::vector<MetricFamily> metrics;
        metrics.reserve(gauges.size());

        for (auto& g : gauges) { metrics.push_back(g.Collect(*info)); }
        return metrics;
    }

private:
    std::vector<raw_gauge> gauges;

    void create_gauges() {
        // ------ memstat --------
        gauges.push_back(
            BuildRawGauge()
                .Name("sysinfo_memory_size_bytes")
                .Help("Total memory available")
                .Type(MetricType::Gauge)
                .Callback(to_double_single(&system_info::MemTotal)));
        gauges.push_back(
            BuildRawGauge()
                .Name("sysinfo_memory_free_bytes")
                .Help("Free memory (excluding buffered and cached memory)")
                .Type(MetricType::Gauge)
                .Callback(to_double_single(&system_info::MemFree)));
        gauges.push_back(
            BuildRawGauge()
                .Name("sysinfo_memory_available_bytes")
                .Help("Estimated available memory for starting new "
                      "applications, without swapping")
                .Type(MetricType::Gauge)
                .Callback(to_double_single(&system_info::MemAvailable)));
        gauges.push_back(
            BuildRawGauge()
                .Name("sysinfo_memory_buffers_bytes")
                .Help("Temporary storage for raw disk blocks")
                .Type(MetricType::Gauge)
                .Callback(to_double_single(&system_info::Buffers)));
        gauges.push_back(
            BuildRawGauge()
                .Name("sysinfo_memory_cached_bytes")
                .Help("Cached files in RAM (page cache), exluding swap cache")
                .Type(MetricType::Gauge)
                .Callback(to_double_single(&system_info::Cached)));
        gauges.push_back(
            BuildRawGauge()
                .Name("sysinfo_swap_cache_bytes")
                .Help("Memory that was swapped out and back in but is still "
                      "also in the swap file")
                .Type(MetricType::Gauge)
                .Callback(to_double_single(&system_info::SwapCached)));
        gauges.push_back(BuildRawGauge()
                             .Name("sysinfo_memory_active_bytes")
                             .Help("Memory that was used more recently, not "
                                   "reclaimed unless absolutely necessary")
                             .Type(MetricType::Gauge)
                             .Callback(to_double_single(&system_info::Active)));
        gauges.push_back(
            BuildRawGauge()
                .Name("sysinfo_memory_active_bytes")
                .Help("Memory that was used less recently, likely to be "
                      "reclaimed")
                .Type(MetricType::Gauge)
                .Callback(to_double_single(&system_info::Inactive)));
        gauges.push_back(
            BuildRawGauge()
                .Name("sysinfo_swap_size_bytes")
                .Help("Total swap memory available")
                .Type(MetricType::Gauge)
                .Callback(to_double_single(&system_info::SwapTotal)));
        gauges.push_back(
            BuildRawGauge()
                .Name("sysinfo_swap_free_bytes")
                .Help("Amount of unused swap memory")
                .Type(MetricType::Gauge)
                .Callback(to_double_single(&system_info::SwapFree)));
        gauges.push_back(
            BuildRawGauge()
                .Name("sysinfo_memory_dirty_bytes")
                .Help("Amount of 'dirty memory' to be written to ram"
                      "but not yet synced")
                .Type(MetricType::Gauge)
                .Callback(to_double_single(&system_info::Dirty)));
        gauges.push_back(
            BuildRawGauge()
                .Name("sysinfo_memory_writeback_bytes")
                .Help("Amount of memory currently being written to disk")
                .Type(MetricType::Gauge)
                .Callback(to_double_single(&system_info::Writeback)));

        // ------ sysinfo() --------

        gauges.push_back(
            BuildRawGauge()
                .Name("sysinfo_processes")
                .Help("Amount of running processes")
                .Type(MetricType::Gauge)
                .Callback(to_double_single(&system_info::Processes)));
        gauges.push_back(
            BuildRawGauge()
                .Name("sysinfo_memory_shared_bytes")
                .Help("Amount of shared memory")
                .Type(MetricType::Gauge)
                .Callback(to_double_single(&system_info::MemShared)));

        // ------ loadavg() --------

        gauges.push_back(
            BuildRawGauge()
                .Name("sysinfo_avg_load")
                .Help("1 minute cpu load average")
                .Type(MetricType::Gauge)
                .Callback(to_double_single(
                    [](system_info const& i) { return i.Loads[0]; })));

        // ------ /proc/stat --------

        gauges.push_back(
            BuildRawGauge()
                .Name("sysinfo_cpu_user_seconds")
                .Help("Time spent in user mode since boot")
                .Type(MetricType::Gauge)
                .Callback(to_double_single(&system_info::UserTime)));
        gauges.push_back(
            BuildRawGauge()
                .Name("sysinfo_cpu_system_seconds")
                .Help("Time spent in system mode since boot")
                .Type(MetricType::Gauge)
                .Callback(to_double_single(&system_info::SystemTime)));
        gauges.push_back(
            BuildRawGauge()
                .Name("sysinfo_cpu_irq_seconds")
                .Help("Time spent servicing interrupts since boot")
                .Type(MetricType::Gauge)
                .Callback(to_double_single(&system_info::IrqTime)));
        gauges.push_back(BuildRawGauge()
                             .Name("sysinfo_cpu_vm_seconds")
                             .Help("Time spent in virtual machines since boot")
                             .Type(MetricType::Gauge)
                             .Callback(to_double_single(&system_info::VmTime)));
        gauges.push_back(BuildRawGauge()
                             .Name("sysinfo_cpu_vm_seconds")
                             .Help("Time spent in virtual machines since boot")
                             .Type(MetricType::Gauge)
                             .Callback(to_double_single(&system_info::VmTime)));
        gauges.push_back(
            BuildRawGauge()
                .Name("sysinfo_errors_count")
                .Help("Number of scrapes containing errors")
                .Type(MetricType::Gauge)
                .Callback(to_double_single(&system_info::get_errors_count)));

        // ------ /proc/net/netsat --------

        gauges.push_back(
            BuildRawGauge()
                .Name("sysinfo_network_in_bytes")
                .Help("Count of reveived octets (bytes) since boot")
                .Type(MetricType::Gauge)
                .Callback(to_double_single(&system_info::InOctets)));
        gauges.push_back(
            BuildRawGauge()
                .Name("sysinfo_network_out_bytes")
                .Help("Count of sent octets (bytes) since boot")
                .Type(MetricType::Gauge)
                .Callback(to_double_single(&system_info::OutOctets)));

        // sys/class/thermal/
        auto parse_sensors = [](system_info const& info) {
            std::vector<ClientMetric> metrics;
            metrics.reserve(info.SensorTemps.size());
            for (auto& sensor : info.SensorTemps) {
                ClientMetric& m = metrics.emplace_back();
                m.gauge.value   = sensor.value_celsius;
                m.label.push_back({"type", sensor.type});
            }
            return metrics;
        };
        gauges.push_back(
            BuildRawGauge()
                .Name("sysinfo_sensor_temperature_celsius")
                .Help("Temperature of a sensor with its type as a label")
                .Type(MetricType::Gauge)
                .Callback(parse_sensors));
    }
};

#else

class SystemInfoCollector::Impl {
public:
    std::vector<MetricFamily> Collect() const { return {}; }
};

#endif

SystemInfoCollector::SystemInfoCollector(init)
    : impl(std::make_unique<Impl>()) {}

SystemInfoCollector::~SystemInfoCollector() = default;

std::vector<MetricFamily> SystemInfoCollector::Collect() const {
    return impl->Collect();
}
