#include "system_info.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <sys/sysinfo.h>
#include <unistd.h>

#include "system_info_exposer.hpp"
#include <logging/logging.hpp>

using namespace logging;

namespace sys_info {

std::atomic_llong system_info::errors_count = 0;

bool system_info::test_file(std::string const& name) {
    try {
        std::ifstream ifs(name);
        return ifs.good();
    } catch (std::exception const& e) {
        log(e.what());
        return false;
    }
}

std::optional<std::ifstream> system_info::open_stat_file() {
    static bool const has_file = test_file(SystemInfoCollector::stat_location);
    if (!has_file) {
        error("File check for ", SystemInfoCollector::stat_location,
              " Failed previously");
        return std::nullopt;
    }
    return open_file(SystemInfoCollector::stat_location);
}

std::optional<std::ifstream> system_info::open_meminfo_file() {
    static bool const has_file =
        test_file(SystemInfoCollector::meminfo_location);
    if (!has_file) {
        error("File check for ", SystemInfoCollector::meminfo_location,
              " Failed previously");
        return std::nullopt;
    }
    return open_file(SystemInfoCollector::meminfo_location);
}

std::optional<std::ifstream> system_info::open_netstat_file() {
    static bool const has_file =
        test_file(SystemInfoCollector::netstat_location);
    if (!has_file) {
        error("File check for ", SystemInfoCollector::netstat_location,
              " Failed previously");
        return std::nullopt;
    }
    return open_file(SystemInfoCollector::netstat_location);
}

std::optional<std::ifstream> system_info::open_file(std::string const& name) {
    std::optional<std::ifstream> opt(std::nullopt);

    try {
        opt.emplace(std::ifstream(name));
        if (!opt.has_value() || !opt->good()) {
            error("Failed to open ", name);
            opt.reset();
        }
    } catch (std::exception& e) {
        error("Exception while opening ", name, ": ", e.what());
    }

    return opt;
}

double system_info::get_clock_hz() {
    static double const v = []() {
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

int system_info::get_sectorsize() {
    // Always 512 B on linux
    return 512;
}

double system_info::get_unit(std::string const& u) {
    if (u == "kB") return 1000;
    if (u == "B" || u == "") return 1;
    if (u == "MB") return 1'000'000;
    error("Unknown unit in meminfo file: ", u);
    return 1;
}

std::unique_ptr<system_info const> system_info::create() {
    std::unique_ptr<system_info> info(new system_info{});
    try {
        info->read_meminfo();
        info->read_stat();
        info->get_sysinfo();
        info->get_loadavg();
        info->read_netstat();
        info->read_thermal_sensors();
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
        auto predicate = [&to_find](meminfo_line const& v) {
            return v.name == to_find;
        };

#define MEMINFO_READ(value_name)                                               \
    to_find = #value_name;                                                     \
    it      = std::find_if(lines.begin(), lines.end(), predicate);             \
    if (it != lines.end()) {                                                   \
        value_name = (*it).value;                                              \
        lines.erase(it);                                                       \
    } else {                                                                   \
        error("Value named ", #value_name, " not found in ",                   \
              SystemInfoCollector::meminfo_location);                          \
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
        error("Error while reading ", SystemInfoCollector::meminfo_location);
    }
}

void system_info::read_stat() {
    auto file            = open_stat_file();
    double const user_hz = get_clock_hz();

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
        *file >> id >> user >> nice >> system >> idle >> iowait >> irq
            >> softirq >> steal >> guest >> guest_nice;
        if (!file->good() || id != "cpu")
            error("Error while parsing ", SystemInfoCollector::stat_location);
        UserTime   = (user + nice) * user_hz;
        SystemTime = system * user_hz;
        IrqTime    = (irq + softirq) * user_hz;
        VmTime     = (steal + guest + guest_nice) * user_hz;
    } else {
        error("Error while reading ", SystemInfoCollector::stat_location);
    }
}

void system_info::read_netstat() {
    auto file = open_netstat_file();
    if (file.has_value() && file->good()) {

        std::vector<std::string> lines;
        lines.reserve(20);
        std::string l;
        while (std::getline(*file, l)) lines.push_back(l);

        auto pred = [](std::string const& s) {
            if (s.find("IpExt") != s.npos) { return true; }
            return false;
        };

        auto it = std::find_if(lines.begin(), lines.end(), pred);
        if (it == lines.end()) {
            error("Couldn't  find IpExt in ",
                  SystemInfoCollector::netstat_location);
            return;
        }
        // Find next IpExt row after labels
        it = std::find_if(it + 1, lines.end(), pred);
        if (it != lines.end()) {
            std::string id;
            unsigned long u;  // unused
            unsigned long in_octets  = 0;
            unsigned long out_octets = 0;
            unsigned long in_mcast   = 0;
            unsigned long out_mcast  = 0;

            std::stringstream ss(*it);
            ss >> id >> u >> u >> u >> u >> u >> u >> in_octets >> out_octets
                >> in_mcast >> out_mcast;

            InOctets  = in_octets + in_mcast;
            OutOctets = out_octets + out_mcast;

            if (!ss.good() || id != "IpExt:") {
                error("Error while reading IpExt values");
            }

        } else {
            error("Couldn't find second IpExt row in ",
                  SystemInfoCollector::netstat_location);
        }
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
    if (written != 3) {
        error("Failed to call getloadavg(): returned ", written);
    }
}

std::list<system_info::meminfo_line>
system_info::parse_lines(std::istream& is) {
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

std::vector<thermal_sensor> const& system_info::find_sensors() {
    static std::vector<thermal_sensor> sensors = [this]() {
        std::vector<thermal_sensor> sensors;

        std::filesystem::path path =
            SystemInfoCollector::thremal_sesnsors_root_location;
        for (auto const& entry : std::filesystem::directory_iterator(path)) {
            if (!entry.is_directory()) continue;
            std::string dirname = entry.path().filename().string();
            if (dirname.find("thermal_zone") != dirname.npos) {
                auto& sensor            = sensors.emplace_back();
                sensor.temperature_path = entry.path() / "temp";

                std::ifstream type(entry.path() / "type");
                if (type.good()) {
                    type >> sensor.type;
                } else {
                    error("Failed to read thermal type file ",
                          entry.path() / "type");
                }
            }
        }
        log("Found ", sensors.size(), " sensors");
        for (auto& s : sensors) log(s.type);
        return sensors;
    }();

    return sensors;
}

void sys_info::system_info::read_thermal_sensors() {
    std::vector<thermal_sensor> const& sensors = find_sensors();
    SensorTemps.reserve(sensors.size());
    for (auto const& sensor : sensors) {
        thermal_info info{};
        info.type = sensor.type;

        std::ifstream temperature_file(sensor.temperature_path);
        if (temperature_file.good()) {
            long value;
            temperature_file >> value;
            info.value_celsius = value * thermal_sensor::step_size;
        } else {
            error("Failed to open temperature file ", sensor.temperature_path,
                  " for ", info.type);
        }
        SensorTemps.push_back(info);
    }
}

}  // namespace sys_info
