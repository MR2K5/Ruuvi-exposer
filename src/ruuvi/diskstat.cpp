#include "diskstat.hpp"

#include "diskstat_exposer.hpp"
#include <cassert>
#include <fstream>
#include <functional>
#include <spdlog/spdlog.h>
#include <optional>

#include <prometheus/client_metric.h>
#include <prometheus/metric_family.h>

namespace sys_info {

double Diskstat::time_to_float(ui time) {
    // time is given in milliseconds
    return double(time) / 1000.0;
}

double Diskstat::sector_byte_size() {
    // On linux, always 512
    return 512.0;
}

namespace {
std::optional<std::ifstream> open_diskstats() {
    auto check = [&] {
        std::ifstream ifs(DiskstatExposer::diskstat_location);
        return ifs.good();
    };
    static bool const exists = check();
    if (exists)
        return std::ifstream(DiskstatExposer::diskstat_location);
    else
        return std::nullopt;
}
}  // namespace

std::vector<Diskstat> Diskstat::create() {
    std::vector<Diskstat> stats;
    stats.reserve(10);

    auto f = open_diskstats();
    if (!f.has_value()) {
        spdlog::warn("Failed to open diskstats file");
        return {};
    }
    std::ifstream& ifs = *f;

    std::string line;
    while (ifs.good() && std::getline(ifs, line).good()) {
        Diskstat& c = stats.emplace_back();

        ifs >> c.major >> c.minor >> c.devname >> c.ReadCompleted
            >> c.ReadMerged >> c.ReadSectors >> c.ReadTime >> c.WriteCompleted
            >> c.WriteMerged >> c.WriteSectors >> c.WriteTime >> c.IOInProgress
            >> c.IOTime >> c.WeightedIOTime >> c.DiscardCompleted
            >> c.DiscardMerged >> c.DiscardTime >> c.FlushComplete
            >> c.FlushTime;
    }

    return stats;
}

}  // namespace sys_info
