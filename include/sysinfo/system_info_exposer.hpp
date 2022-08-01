#pragma once

#include <prometheus/collectable.h>

#include <memory>
#include <vector>

namespace sys_info {

class SystemInfo: public prometheus::Collectable {
    struct init {};

public:
    static constexpr char const* meminfo_location               = "/proc/meminfo";
    static constexpr char const* stat_location                  = "/proc/stat";
    static constexpr char const* netstat_location               = "/proc/net/netstat";
    static constexpr char const* thremal_sesnsors_root_location = "/sys/class/thermal";

    static std::shared_ptr<SystemInfo> create();
    std::vector<prometheus::MetricFamily> Collect() const override;

    SystemInfo(init);
    ~SystemInfo();

private:
    class Impl;
    std::unique_ptr<Impl> impl;
};

}  // namespace sys_info