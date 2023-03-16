#pragma once

#include <prometheus/collectable.h>

namespace sys_info {
class DiskstatExposer: public prometheus::Collectable {
public:
    std::vector<prometheus::MetricFamily> Collect() const override;

    static constexpr char const* diskstat_location = "/proc/diskstats";
};
}  // namespace sys_info
