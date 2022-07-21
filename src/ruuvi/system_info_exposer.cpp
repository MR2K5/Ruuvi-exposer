#include "system_info_exposer.hpp"

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <list>
#include <map>
#include <optional>
#include <sstream>

#include <prometheus/family.h>
#include <prometheus/gauge.h>

#include <sys/sysinfo.h>
#include <unistd.h>

#include "logging.hpp"

using namespace sys_info;
using namespace prometheus;

std::shared_ptr<sys_info::SystemInfo> SystemInfo::create() {
    return std::make_shared<SystemInfo>(init());
}

namespace {

struct system_info {
    using ul = unsigned long;
    // From /proc/meminfo
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

    // From sysinfo()
    ul Processes;
    ul MemShared;

    // From getloadavg
    double Loads[3];

    // From /proc/stat
    double UserTime;
    double SystemTime;
    double IrqTime;
    double VmTime;

    bool has_errors;
    std::string errors;
    static std::atomic_llong errors_count;
    double get_errors_count() const { return errors_count; }

    static std::unique_ptr<const system_info> create();

private:
    std::optional<std::ifstream> open_file(std::string const& name);
    std::optional<std::ifstream> open_stat_file() {
        static const bool has_file = []() {
            try {
                std::ifstream ifs(SystemInfo::stat_location);
                return ifs.good();
            } catch (std::exception const& e) {
                log(e.what());
                return false;
            }
        }();
        if (!has_file) {
            error("File check for ", SystemInfo::stat_location, " Failed previously");
            return std::nullopt;
        }
        return open_file(SystemInfo::stat_location);
    }
    std::optional<std::ifstream> open_meminfo_file() {
        static const bool has_file = []() {
            try {
                std::ifstream ifs(SystemInfo::meminfo_location);
                return ifs.good();
            } catch (std::exception const& e) {
                log(e.what());
                return false;
            }
        }();
        if (!has_file) {
            error("File check for ", SystemInfo::meminfo_location, " Failed previously");
            return std::nullopt;
        }
        return open_file(SystemInfo::meminfo_location);
    }

    double get_clock_hz();

    system_info() = default;

    void read_meminfo();
    void read_stat();
    void get_sysinfo();
    void get_loadavg();

    double get_unit(std::string const& u);

    struct meminfo_line {
        std::string name;
        unsigned long value;
    };
    std::list<meminfo_line> parse_lines(std::istream& is);

    template<class... As> void error(As&&... as) {
        // Increment only first time
        if (has_errors == false) {
            has_errors = true;
            errors_count += 1;
        }

        if constexpr (sizeof...(As) >= 1) {
            std::stringstream ss;
            (ss << ... << as);
            errors += "\n" + ss.str();
            log(ss.rdbuf());
        }
    }
};

std::atomic_llong system_info::errors_count = 0;

std::optional<std::ifstream> system_info::open_file(std::string const& name) {
    std::optional<std::ifstream> opt(std::nullopt);

    try {
        opt.emplace(std::ifstream(name));
        if (!opt.has_value() || !opt->good()) {
            error("Failed to open ", name);
            opt.reset();
        }
    } catch (std::exception& e) { error("Exception while opening ", name, ": ", e.what()); }

    return opt;
}

double system_info::get_clock_hz() {
    static const double v = []() {
        long ticks_per_s = sysconf(_SC_CLK_TCK);
        if (ticks_per_s == -1) {
            log("Failed to read _SC_CLK_TCK: ", std::strerror(errno));
            return -1.0;
        }
        return 1.0 / ticks_per_s;
    }();
    if (v < 0) error();
    return v;
}

std::unique_ptr<const system_info> system_info::create() {
    std::unique_ptr<system_info> info(new system_info{});
    try {
        info->read_meminfo();
        info->read_stat();
        info->get_sysinfo();
        info->get_loadavg();
    } catch (std::exception const& e) {
        info->error("Exception in system_info::create(): ", e.what());
    } catch (...) { info->error("Unknown exception in system_info::create()"); }

    return info;
}

void system_info::read_meminfo() {
    auto file = open_meminfo_file();
    if (file.has_value()) {

        auto lines = parse_lines(*file);
        file->close();

        std::string_view to_find;
        typename decltype(lines)::iterator it;
        auto predicate = [&to_find](meminfo_line const& v) { return v.name == to_find; };

#define MEMINFO_READ(value_name)                                                            \
    to_find = #value_name;                                                                  \
    it      = std::find_if(lines.begin(), lines.end(), predicate);                          \
    if (it != lines.end()) {                                                                \
        value_name = (*it).value;                                                           \
        lines.erase(it);                                                                    \
    } else {                                                                                \
        error("Value named ", #value_name, " not found in ", SystemInfo::meminfo_location); \
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
    } else {
        error("Error while reading ", SystemInfo::meminfo_location);
    }
}

void system_info::read_stat() {
    auto file            = open_stat_file();
    const double user_hz = get_clock_hz();

    if (file.has_value() && file->good() && user_hz > 0) {
        std::string id;
        long user       = 0;
        long nice       = 0;
        long system     = 0;
        long idle       = 0;
        long iowait     = 0;
        long irq        = 0;
        long softirq    = 0;
        long steal      = 0;
        long guest      = 0;
        long guest_nice = 0;
        *file >> id >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal >> guest
            >> guest_nice;
        if (!file->good() || id != "cpu") error("Error while parsing ", SystemInfo::stat_location);
        UserTime   = (user + nice) * user_hz;
        SystemTime = system * user_hz;
        IrqTime    = (irq + softirq) * user_hz;
        VmTime     = (steal + guest + guest_nice) * user_hz;
    } else {
        error("Error while reading ", SystemInfo::stat_location);
    }
}

void system_info::get_sysinfo() {
    struct sysinfo si {};
    int e = sysinfo(&si);
    if (e) {
        error("Failed to get sysinfo(): ", std::strerror(errno));
    } else {
        Processes = si.procs;
        MemShared = si.sharedram * si.mem_unit;
    }
}

void system_info::get_loadavg() {
    int written = getloadavg(Loads, 3);
    if (written != 3) { error("Failed to call getloadavg(): returned ", written); }
}

double system_info::get_unit(const std::string& u) {
    if (u == "kB") return 1000;
    if (u == "B" || u == "") return 1;
    if (u == "MB") return 1'000'000;
    error("Unknown unit in meminfo file: ", u);
    return 1;
}

std::list<system_info::meminfo_line> system_info::parse_lines(std::istream& is) {
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

using cb_func = std::vector<ClientMetric>(system_info const&);

}  // namespace

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
        auto info = system_info::create();

        std::vector<MetricFamily> metrics;
        metrics.reserve(gauges.size());

        for (auto& g : gauges) { metrics.push_back(g.Collect(*info)); }
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
        gauges.push_back(BuildRawGauge()
                             .Name("sysinfo_memory_shared_bytes")
                             .Help("Amount of shared memory")
                             .Type(MetricType::Gauge)
                             .Callback(to_double_single(&system_info::MemShared)));
        gauges.push_back(
            BuildRawGauge()
                .Name("sysinfo_avg_load")
                .Help("1 minute cpu load average")
                .Type(MetricType::Gauge)
                .Callback(to_double_single([](system_info const& i) { return i.Loads[0]; })));
        gauges.push_back(BuildRawGauge()
                             .Name("sysinfo_cpu_user_seconds")
                             .Help("Time spent in user mode since boot")
                             .Type(MetricType::Gauge)
                             .Callback(to_double_single(&system_info::UserTime)));
        gauges.push_back(BuildRawGauge()
                             .Name("sysinfo_cpu_system_seconds")
                             .Help("Time spent in system mode since boot")
                             .Type(MetricType::Gauge)
                             .Callback(to_double_single(&system_info::SystemTime)));
        gauges.push_back(BuildRawGauge()
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
        gauges.push_back(BuildRawGauge()
                             .Name("sysinfo_errors_count")
                             .Help("Number of scrapes containing errors")
                             .Type(MetricType::Gauge)
                             .Callback(to_double_single(&system_info::get_errors_count)));
    }
};

SystemInfo::SystemInfo(init): impl(std::make_unique<Impl>()) {}

SystemInfo::~SystemInfo() = default;

std::vector<MetricFamily> SystemInfo::Collect() const {
    return impl->Collect();
}
