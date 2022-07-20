#include "system_info_exposer.hpp"

#include <algorithm>
#include <cassert>
#include <fstream>
#include <functional>
#include <iostream>
#include <list>
#include <map>
#include <sstream>

#include <sys/sysinfo.h>

using namespace sys_info;
using namespace prometheus;

std::shared_ptr<sys_info::SystemInfo> SystemInfo::create() {
    return std::make_shared<SystemInfo>(init());
}

namespace {

struct system_info {
    using ul = unsigned long;
    ul MemTotal;
    ul MemFree;
    ul MemAvailable;
    ul Buffers;
    ul Cached;
    ul SwapCached;
    ul Active;
    ul Inactive;
    //    ul HighTotal;
    //    ul HighFree;
    //    ul LowTotal;
    //    ul LowFree;
    ul SwapTotal;
    ul SwapFree;
    ul Dirty;
    ul Writeback;

    ul Processes;
    bool has_errors;
};

double get_unit(std::string const& u) {
    if (u == "kB") return 1024;
    if (u == "B" || u == "") return 1;
    if (u == "MB") return 1'048'576;
    throw std::runtime_error("/proc/meminfo: unknown unit: " + u);
}

struct meminfo_line {
    std::string name;
    unsigned long value;
};

auto parse_lines(std::istream& is) {
    std::list<meminfo_line> lines;

    std::string line;
    while (std::getline(is, line)) {
        meminfo_line m;
        std::stringstream ss(line);
        std::string multiplier;

        ss >> m.name >> m.value >> multiplier;
        // Remove trailing ':'
        m.name.pop_back();

        m.value *= get_unit(multiplier);
        lines.push_back(std::move(m));
    }

    return lines;
}

system_info read_meminfo() {
    system_info r;
    r.has_errors = false;
    std::ifstream file(SystemInfo::meminfo_location);

    auto lines = parse_lines(file);
    file.close();

    std::string_view to_find;
    typename decltype(lines)::iterator it;
    auto predicate = [&to_find](meminfo_line const& v) { return v.name == to_find; };

#define MEMINFO_READ(value_name)                                   \
    to_find = #value_name;                                         \
    it      = std::find_if(lines.begin(), lines.end(), predicate); \
    if (it != lines.end()) {                                       \
        r.value_name = (*it).value;                                \
        lines.erase(it);                                           \
    } else {                                                       \
        r.has_errors = true;                                       \
    }

    MEMINFO_READ(MemTotal);
    MEMINFO_READ(MemFree);
    MEMINFO_READ(MemAvailable);
    MEMINFO_READ(Buffers);
    MEMINFO_READ(Cached);
    MEMINFO_READ(SwapCached);
    MEMINFO_READ(Active);
    MEMINFO_READ(Inactive);
    //    MEMINFO_READ(HighTotal);
    //    MEMINFO_READ(HighFree);
    //    MEMINFO_READ(LowTotal);
    //    MEMINFO_READ(LowFree);
    MEMINFO_READ(SwapTotal);
    MEMINFO_READ(SwapFree);
    MEMINFO_READ(Dirty);
    MEMINFO_READ(Writeback);

#undef MEMINFO_READ

    return r;
}

system_info get_system_info() {
    system_info info = read_meminfo();
    struct sysinfo sinfo;
    int e = sysinfo(&sinfo);
    if (e) {
        info.has_errors = true;
    } else {
        info.Processes = sinfo.procs;
    }
    return info;
}

using cb_func = std::vector<ClientMetric>(system_info const&);

}  // namespace

unsigned long sys_info::test() {
    auto x = read_meminfo();
    return x.has_errors;
}

class raw_gauge_builder;

class raw_gauge {
public:
    MetricFamily Collect(system_info const& info) const {
        auto m   = gauge;
        m.metric = callback(info);
        m.metric.reserve(m.metric.size() + labels.size());
        for (auto& [name, value] : labels) {
            for (auto& n : m.metric) { n.label.push_back({ name, value }); }
        }
        return m;
    }

    friend class raw_gauge_builder;

private:
    std::function<cb_func> callback;
    MetricFamily gauge;
    std::map<std::string, std::string> labels;
    raw_gauge() = default;
};

class raw_gauge_builder {
public:
    raw_gauge_builder() = default;
    raw_gauge_builder&& Name(std::string const& n) && {
        name = n;
        return std::move(*this);
    }
    raw_gauge_builder&& Help(std::string const& n) && {
        help = n;
        return std::move(*this);
    }
    raw_gauge_builder&& Type(MetricType t) && {
        type = t;
        return std::move(*this);
    }
    raw_gauge_builder&& Labels(std::map<std::string, std::string> const& n) && {
        labels = n;
        return std::move(*this);
    }
    raw_gauge_builder&& Callback(std::function<cb_func> const& n) && {
        callback = n;
        return std::move(*this);
    }

    operator raw_gauge() && {
        raw_gauge rg;
        rg.callback   = std::move(callback);
        rg.gauge.name = std::move(name);
        rg.gauge.help = std::move(help);
        rg.gauge.type = type;
        rg.labels     = std::move(labels);
        return rg;
    }

private:
    std::string name;
    std::string help;
    MetricType type;
    std::map<std::string, std::string> labels;
    std::function<cb_func> callback;
};

template<class F> auto to_double_single(F&& f) {
    return [fn = std::forward<F>(f)](system_info const& i) {
        ClientMetric m;
        m.gauge.value = static_cast<double>(std::invoke(fn, i));
        return std::vector{ m };
    };
}

auto BuildRawGauge() {
    return raw_gauge_builder();
}

class SystemInfo::Impl {
public:
    Impl() { create_gauges(); }

    std::vector<MetricFamily> Collect() const {
        auto info = get_system_info();

        std::vector<MetricFamily> metrics;
        metrics.reserve(gauges.size());
        for (auto& g : gauges) { metrics.push_back(g.Collect(info)); }
        return metrics;
    }

private:
    std::vector<raw_gauge> gauges;

    void create_gauges() {
        gauges.push_back(BuildRawGauge()
                             .Name("sysinfo_memory_size_bytes")
                             .Help("Total memory available")
                             .Type(MetricType::Gauge)
                             .Callback(to_double_single(&system_info::MemTotal)));
        gauges.push_back(BuildRawGauge()
                             .Name("sysinfo_memory_free_bytes")
                             .Help("Free memory (excluding buffered and cached memory)")
                             .Type(MetricType::Gauge)
                             .Callback(to_double_single(&system_info::MemFree)));
        gauges.push_back(
            BuildRawGauge()
                .Name("sysinfo_memory_available_bytes")
                .Help("Estimated available memory for starting new applications, without swapping")
                .Type(MetricType::Gauge)
                .Callback(to_double_single(&system_info::MemAvailable)));
        gauges.push_back(BuildRawGauge()
                             .Name("sysinfo_memory_buffers_bytes")
                             .Help("Temporary storage for raw disk blocks")
                             .Type(MetricType::Gauge)
                             .Callback(to_double_single(&system_info::Buffers)));
        gauges.push_back(BuildRawGauge()
                             .Name("sysinfo_memory_cached_bytes")
                             .Help("Cached files in RAM (page cache), exluding swap cache")
                             .Type(MetricType::Gauge)
                             .Callback(to_double_single(&system_info::Cached)));
        gauges.push_back(
            BuildRawGauge()
                .Name("sysinfo_swap_cache_bytes")
                .Help("Memory that was swapped out and back in but is still also in the swap file")
                .Type(MetricType::Gauge)
                .Callback(to_double_single(&system_info::SwapCached)));
        gauges.push_back(
            BuildRawGauge()
                .Name("sysinfo_memory_active_bytes")
                .Help(
                    "Memory that was used more recently, not reclaimed unless absolutely necessary")
                .Type(MetricType::Gauge)
                .Callback(to_double_single(&system_info::Active)));
        gauges.push_back(BuildRawGauge()
                             .Name("sysinfo_memory_active_bytes")
                             .Help("Memory that was used less recently, likely to be reclaimed")
                             .Type(MetricType::Gauge)
                             .Callback(to_double_single(&system_info::Inactive)));
        gauges.push_back(BuildRawGauge()
                             .Name("sysinfo_swap_size_bytes")
                             .Help("Total swap memory available")
                             .Type(MetricType::Gauge)
                             .Callback(to_double_single(&system_info::SwapTotal)));
        gauges.push_back(BuildRawGauge()
                             .Name("sysinfo_swap_free_bytes")
                             .Help("Amount of unused swap memory")
                             .Type(MetricType::Gauge)
                             .Callback(to_double_single(&system_info::SwapFree)));
        gauges.push_back(BuildRawGauge()
                             .Name("sysinfo_processes")
                             .Help("Amount of running processes")
                             .Type(MetricType::Gauge)
                             .Callback(to_double_single(&system_info::Processes)));
    }
};

SystemInfo::SystemInfo(init): impl(std::make_unique<Impl>()) {}

SystemInfo::~SystemInfo() = default;

std::vector<MetricFamily> SystemInfo::Collect() const {
    return impl->Collect();
}
