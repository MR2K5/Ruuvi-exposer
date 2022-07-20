#pragma once

#include <prometheus/collectable.h>
#include <prometheus/metric_family.h>

#include <memory>
#include <vector>

namespace sys_info {

class SystemInfo: public prometheus::Collectable {
    struct init {};

public:
    static constexpr char const* meminfo_location = "/proc/meminfo";

    static std::shared_ptr<SystemInfo> create();
    std::vector<prometheus::MetricFamily> Collect() const override;

    SystemInfo(init);
    ~SystemInfo();
private:
    class Impl;
    std::unique_ptr<Impl> impl;
};

unsigned long test();

}  // namespace sys_info
