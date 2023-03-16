#include "raw_gauge.hpp"

namespace sys_info {

prometheus::MetricFamily raw_gauge::Collect(system_info const& info) const {
    auto m   = gauge;
    m.metric = callback(info);

    m.metric.reserve(m.metric.size() + labels.size());
    for (auto& [name, value] : labels) {
        for (auto& n : m.metric) { n.label.push_back({name, value}); }
    }
    return m;
}

raw_gauge_builder&& raw_gauge_builder::Name(std::string const& n) && {
    name = n;
    return std::move(*this);
}

raw_gauge_builder&& raw_gauge_builder::Help(std::string const& n) && {
    help = n;
    return std::move(*this);
}

raw_gauge_builder&& raw_gauge_builder::Type(prometheus::MetricType t) && {
    type = t;
    return std::move(*this);
}

raw_gauge_builder&&
raw_gauge_builder::Labels(std::map<std::string, std::string> const& n) && {
    labels = n;
    return std::move(*this);
}

raw_gauge_builder&&
raw_gauge_builder::Callback(std::function<cb_func> const& n) && {
    callback = n;
    return std::move(*this);
}

raw_gauge_builder::operator raw_gauge() && {
    raw_gauge rg;
    rg.gauge.name = std::move(name);
    rg.gauge.help = std::move(help);
    rg.gauge.type = type;
    rg.labels     = std::move(labels);
    rg.callback   = std::move(callback);
    return rg;
}

raw_gauge_builder BuildRawGauge() { return raw_gauge_builder(); }

}  // namespace sys_info
