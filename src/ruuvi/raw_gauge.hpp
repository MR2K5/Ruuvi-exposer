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

template<class...>
class raw_gauge_builder;

template<class... As> class raw_gauge {
public:
    using func = std::vector<pr::ClientMetric>(As const&... as);

    pr::MetricFamily Collect(As const&... as) const {
        auto m   = gauge;
        m.metric = callback(as...);

        m.metric.reserve(m.metric.size() + labels.size());
        for (auto& [name, value] : labels) {
            for (auto& n : m.metric) { n.label.push_back({name, value}); }
        }
        return m;
    }

    template<class...>
    friend class raw_gauge_builder;

private:
    std::function<func> callback;
    pr::MetricFamily gauge;
    std::map<std::string, std::string> labels;
    raw_gauge() = default;
};

template<class... As>
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

    raw_gauge_builder&& Type(prometheus::MetricType t) && {
        type = t;
        return std::move(*this);
    }

    raw_gauge_builder&& Labels(std::map<std::string, std::string> const& n) && {
        labels = n;
        return std::move(*this);
    }

    template<class F>
    auto Callback(F&& f) && {
        raw_gauge<As...> g;
        g.callback = std::forward<F>(f);
        g.gauge    = pr::MetricFamily{std::move(name), std::move(help), type, {}};
        g.labels   = std::move(labels);
        return g;
    }

private:
    std::string name;
    std::string help;
    pr::MetricType type;
    std::map<std::string, std::string> labels;
};

template<class... As>
raw_gauge_builder<As...> BuildRawGauge() { return raw_gauge_builder<As...>{}; }

}  // namespace sys_info
