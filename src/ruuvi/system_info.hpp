#pragma once

#include <atomic>
#include <filesystem>
#include <list>
#include <logging/logging.hpp>
#include <optional>
#include <string>
#include <vector>

#include "diskstat.hpp"

namespace sys_info {

struct thermal_info {
    std::string type;
    double value_celsius;
};

struct thermal_sensor {
    static constexpr double step_size = 1.0 / 1000;
    std::filesystem::path temperature_path;
    std::string type;
};

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

    // From /proc/net/netstat
    ul InOctets;
    ul OutOctets;

    // From /sys/class/thermal
    std::vector<thermal_info> SensorTemps;

    // From /proc/diskstat
    std::vector<Diskstat> DiskStats;

    bool has_errors;
    std::string errors;
    static std::atomic_llong errors_count;
    double get_errors_count() const { return errors_count; }

    static std::unique_ptr<system_info const> create();

private:
    bool test_file(std::string const& name);
    std::optional<std::ifstream> open_file(std::string const& name);
    std::optional<std::ifstream> open_stat_file();
    std::optional<std::ifstream> open_meminfo_file();
    std::optional<std::ifstream> open_netstat_file();

    std::vector<thermal_sensor> const& find_sensors();

    double get_unit(std::string const& u);
    double get_clock_hz();
    int get_sectorsize();

    system_info() = default;

    void read_meminfo();
    void read_stat();
    void read_netstat();
    void get_sysinfo();
    void get_loadavg();
    void read_thermal_sensors();

    struct meminfo_line {
        std::string name;
        unsigned long value;
    };
    std::list<meminfo_line> parse_lines(std::istream& is);

    template<class... As> void error(As&&... as) {
        // Increment only first time
        if (has_errors == false) {
            has_errors   = true;
            errors_count += 1;
        }

        if constexpr (sizeof...(As) >= 1) {
            std::stringstream ss;
            (ss << ... << as);
            errors += "\n" + ss.str();
            spdlog::warn(ss.rdbuf()->str());
        }
    }
};

}  // namespace sys_info
