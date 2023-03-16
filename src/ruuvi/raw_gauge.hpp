#pragma once

#include <functional>
#include <map>
#include <prometheus/client_metric.h>
#include <prometheus/metric_family.h>
#include <string>

namespace sys_info {
namespace pr = prometheus;
class system_info;

using cb_func = std::vector<pr::ClientMetric>(system_info const&);

class raw_gauge_builder;

class raw_gauge {
public:
    pr::MetricFamily Collect(system_info const& info) const;

    friend class raw_gauge_builder;

private:
    std::function<cb_func> callback;
    pr::MetricFamily gauge;
    std::map<std::string, std::string> labels;
    raw_gauge() = default;
};

class raw_gauge_builder {
public:
    raw_gauge_builder() = default;
    raw_gauge_builder&& Name(std::string const& n) &&;
    raw_gauge_builder&& Help(std::string const& n) &&;
    raw_gauge_builder&& Type(pr::MetricType t) &&;
    raw_gauge_builder&& Labels(std::map<std::string, std::string> const& n) &&;
    raw_gauge_builder&& Callback(std::function<cb_func> const& n) &&;

    operator raw_gauge() &&;

private:
    std::string name;
    std::string help;
    pr::MetricType type;
    std::map<std::string, std::string> labels;
    std::function<cb_func> callback;
};

raw_gauge_builder BuildRawGauge();

}  // namespace sys_info
