#include "diskstat_exposer.hpp"

#include <cassert>
#include <functional>
#include <vector>

#include <prometheus/client_metric.h>
#include <prometheus/metric_family.h>

#ifdef ENABLE_SYSINFO_EXPOSER
#include "diskstat.hpp"
#endif

namespace sys_info {

namespace pr = prometheus;

#ifdef ENABLE_SYSINFO_EXPOSER
namespace {

struct family_with_func {
    std::vector<pr::MetricFamily> family;
    std::vector<std::function<double(Diskstat const&)>> callback;

    void do_collect(std::vector<Diskstat> const& stats) {
        assert(family.size() == callback.size());

        for (size_t i = 0; i < family.size(); ++i) {

            family[i].metric.reserve(stats.size());

            for (auto& stat : stats) {
                prometheus::ClientMetric& cm = family[i].metric.emplace_back();
                cm.label                     = {
                    {"disk", stat.devname}
                };
                cm.gauge.value = callback[i](stat);
            }
        }
    }
};

template<class T> auto convert_to_double(T Diskstat::*val) {
    return
        [val](Diskstat const& stat) { return static_cast<double>(stat.*val); };
}

auto sectors_to_bytes(unsigned long Diskstat::*val) {
    return [val](Diskstat const& stat) {
        return stat.sector_byte_size() * stat.*val;
    };
}

auto convert_time(unsigned Diskstat::*val) {
    return [val](Diskstat const& stat) {
        return Diskstat::time_to_float(stat.*val);
    };
}

family_with_func create_metric_families() {
    family_with_func fms;

    auto cr1 = [&fms](std::string const& name, std::string const& help,
                      auto func) {
        fms.family.push_back({name, help, prometheus::MetricType::Gauge});
        fms.callback.emplace_back(func);
    };

    cr1("sysinfo_disk_reads_completed_blocks_total",
        "Number of successful disk reads in blocks",
        convert_to_double(&Diskstat::ReadCompleted));
    cr1("sysinfo_disk_reads_merged_total",
        "Number of adjacent blocks merged while reading",
        convert_to_double(&Diskstat::ReadMerged));
    cr1("sysinfo_disk_read_bytes_total", "Amount of memory read from disk",
        sectors_to_bytes(&Diskstat::ReadSectors));
    cr1("sysinfo_disk_read_time_seconds_total", "Time spent reading from disk",
        convert_time(&Diskstat::ReadTime));

    cr1("sysinfo_disk_writes_completed_blocks_total",
        "Number of successful disk writes in blocks",
        convert_to_double(&Diskstat::WriteCompleted));
    cr1("sysinfo_disk_writes_merged_total",
        "Number of adjacent blocks merged while writing",
        convert_to_double(&Diskstat::WriteMerged));
    cr1("sysinfo_disk_write_bytes_total", "Amount of memory written to disk",
        sectors_to_bytes(&Diskstat::WriteSectors));
    cr1("sysinfo_disk_write_time_seconds_total", "Time spent writing to disk",
        convert_time(&Diskstat::WriteTime));

    cr1("sysinfo_disk_io_in_progress",
        "Number of disk I/O operation in currently progress",
        convert_to_double(&Diskstat::IOInProgress));
    cr1("sysinfo_disk_io_time_seconds_total", "Time spent on disk I/O",
        convert_time(&Diskstat::IOTime));
    cr1("sysinfo_disk_io_weighted_time_seconds_total",
        "This field is incremented at each I/O start, I/O completion, I/O "
        "merge, or read of these stats by the number of I/Os in progress "
        "[sysinfo_disk_io_in_progress] times the number of milliseconds spent "
        "doing I/O since the "
        "last update of this field.  This can provide an easy measure of both "
        "I/O completion time and the backlog that may be accumulating.",
        convert_time(&Diskstat::WeightedIOTime));

    cr1("sysinfo_disk_discards_completed_blocks_total",
        "Number of successful disk discards in blocks",
        convert_to_double(&Diskstat::DiscardCompleted));
    cr1("sysinfo_disk_discards_merged_total",
        "Number of adjacent blocks merged while discarding",
        convert_to_double(&Diskstat::DiscardMerged));
    cr1("sysinfo_disk_discard_bytes_total",
        "Amount of memory discarded fron disk",
        sectors_to_bytes(&Diskstat::DiscardSectors));
    cr1("sysinfo_disk_discard_time_seconds_total",
        "Time spent discarding from disk",
        convert_time(&Diskstat::DiscardTime));

    cr1("sysinfo_disk_flushes_total", "Number of successful disk flushes",
        convert_to_double(&Diskstat::FlushComplete));
    cr1("sysinfo_disk_flush_time_seconds_total",
        "Amount of time spent flushing disk",
        convert_time(&Diskstat::FlushTime));

    return fms;
}

static auto families = create_metric_families();

}  // namespace

std::vector<prometheus::MetricFamily> DiskstatExposer::Collect() const {
    auto fms   = families;
    auto stats = Diskstat::create();
    fms.do_collect(stats);

    return std::move(fms.family);
}

#else

std::vector<pr::MetricFamily> DiskstatExposer::Collect() const { return {}; }

#endif

}  // namespace sys_info
