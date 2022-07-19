#include "ruuvi_prometheus_exposer.hpp"

#include <future>
#include <prometheus/collectable.h>
#include <prometheus/counter.h>
#include <prometheus/exposer.h>
#include <prometheus/family.h>
#include <prometheus/gauge.h>
#include <prometheus/registry.h>

#include <atomic>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <thread>

extern "C" {
#include <sys/sysinfo.h>
}

using namespace ble;
using namespace prometheus;

namespace {

class MetricCollector {
public:
    MetricCollector(Family<Gauge>& m, std::function<double(ruuvi_data_format_5 const&)> const& c,
                    std::map<std::string, std::string> const& l = {})
        : metric(&m), collector(c), labels(l) {}

    void update(ruuvi_data_format_5 const& d) {
        auto tmp = labels;
        tmp.insert({ "mac", d.mac });
        metric->Add(tmp).Set(collector(d));
    }

private:
    Family<Gauge>* metric;
    const std::function<double(ruuvi_data_format_5 const&)> collector;
    const std::map<std::string, std::string> labels;
};
//

struct sysinfo_metric {
    using F = std::function<std::vector<ClientMetric>(struct sysinfo const&)>;

    MetricFamily get(struct sysinfo const& info) const {
        auto metrics = function(info);

        MetricFamily r = metricfamily;
        r.metric       = std::move(metrics);
        return r;
    }

    MetricFamily metricfamily;
    F function;
};

class sysinfo_builder {
public:
    explicit sysinfo_builder() { metric.metricfamily.type = MetricType::Gauge; }

    sysinfo_builder& Name(std::string const& name) & {
        metric.metricfamily.name = name;
        return *this;
    }
    sysinfo_builder& Help(std::string const& s) & {
        metric.metricfamily.help = s;
        return *this;
    }
    sysinfo_builder& Type(MetricType t) & {
        metric.metricfamily.type = t;
        return *this;
    }
    sysinfo_builder& Callback(sysinfo_metric::F const& f) & {
        metric.function = f;
        return *this;
    }

    sysinfo_builder&& Name(std::string const& name) && {
        metric.metricfamily.name = name;
        return std::move(*this);
    }
    sysinfo_builder&& Help(std::string const& s) && {
        metric.metricfamily.help = s;
        return std::move(*this);
    }
    sysinfo_builder&& Type(MetricType t) && {
        metric.metricfamily.type = t;
        return std::move(*this);
    }
    sysinfo_builder&& Callback(sysinfo_metric::F const& f) && {
        metric.function = f;
        return std::move(*this);
    }

    operator sysinfo_metric() && { return std::move(metric); }

private:
    sysinfo_metric metric;
};

sysinfo_builder BuildSysinfo() {
    return sysinfo_builder();
}

struct single_value_cb {
    template<class F> single_value_cb(F&& f): f(std::forward<F>(f)) {}
    std::function<double(struct sysinfo const&)> f;

    std::vector<ClientMetric> operator()(struct sysinfo const& info) {
        ClientMetric m;
        m.gauge.value = f(info);
        return std::vector{ m };
    }
};

template<class F> single_value_cb multiply_by_memunit(F&& f) {
    return single_value_cb([f = std::forward<F>(f)](struct sysinfo const& i) {
        return static_cast<double>(std::invoke(f, i)) * i.mem_unit;
    });
}

class SysinfoCollector: public Collectable {
    using Label     = ClientMetric::Label;
    using CMV       = std::vector<ClientMetric>;
    using sysinfo_t = struct sysinfo;

public:
    SysinfoCollector() { populate_metrics(); }

    SysinfoCollector(SysinfoCollector const&)            = delete;
    SysinfoCollector& operator=(SysinfoCollector const&) = delete;

    std::vector<MetricFamily> Collect() const override {
        const auto info = get_info();
        std::vector<MetricFamily> collected;
        collected.reserve(metrics.size());

        for (auto& m : metrics) { collected.push_back(m.get(info)); }
        return collected;
    }

private:
    static struct sysinfo get_info() {
        struct sysinfo info;
        int e = ::sysinfo(&info);
        if (e) { throw std::system_error(errno, std::generic_category(), "Failed to getsysinfo"); }
        return info;
    }

    void populate_metrics() {
        metrics.push_back(BuildSysinfo()
                              .Name("sysinfo_running_processes")
                              .Help("Number of currently running processes")
                              .Callback(single_value_cb(&sysinfo_t::procs)));
        metrics.push_back(BuildSysinfo()
                              .Name("sysinfo_uptime")
                              .Help("Uptime since last reboot")
                              .Callback(single_value_cb(&sysinfo_t::uptime)));
        metrics.push_back(BuildSysinfo()
                              .Name("sysinfo_memory_size_bytes")
                              .Help("Total amount of RAM")
                              .Callback(multiply_by_memunit(&sysinfo_t::totalram)));
        metrics.push_back(BuildSysinfo()
                              .Name("sysinfo_memory_free_bytes")
                              .Help("Amount of free RAM")
                              .Callback(multiply_by_memunit(&sysinfo_t::freeram)));
        metrics.push_back(BuildSysinfo()
                              .Name("sysinfo_memory_shared_bytes")
                              .Help("Amount of shared RAM")
                              .Callback(multiply_by_memunit(&sysinfo_t::sharedram)));
        metrics.push_back(BuildSysinfo()
                              .Name("sysinfo_memory_buffered_bytes")
                              .Help("Amount of buffered RAM")
                              .Callback(multiply_by_memunit(&sysinfo_t::bufferram)));
        metrics.push_back(BuildSysinfo()
                              .Name("sysinfo_swap_size_bytes")
                              .Help("Total amount of swap space")
                              .Callback(multiply_by_memunit(&sysinfo_t::totalswap)));
        metrics.push_back(BuildSysinfo()
                              .Name("sysinfo_swap_free_bytes")
                              .Help("Amount of free swap space")
                              .Callback(multiply_by_memunit(&sysinfo_t::freeswap)));
        metrics.push_back(BuildSysinfo()
                              .Name("sysinfo_high_memory_free_bytes")
                              .Help("Amount of free high (userspace) memory")
                              .Callback(multiply_by_memunit(&sysinfo_t::freehigh)));
        metrics.push_back(BuildSysinfo()
                              .Name("sysinfo_high_memory_size_bytes")
                              .Help("Total amount of high (userspace) memory")
                              .Callback(multiply_by_memunit(&sysinfo_t::totalhigh)));
    }

    std::vector<sysinfo_metric> metrics;
};

}  // namespace

class RuuviExposer::Impl {
public:
    Impl(std::string const& addr)
        : exposer(addr), registry(std::make_shared<Registry>()),
          sysinfo_collector(std::make_shared<SysinfoCollector>()) {
        collectors.push_back({ BuildGauge()
                                   .Name("ruuvi_temperature_celsius")
                                   .Help("Ruuvitag temperature in Celsius")
                                   .Register(*registry),
                               &ruuvi_data_format_5::temperature });

        collectors.push_back({ BuildGauge()
                                   .Name("ruuvi_relative_humidity_ratio")
                                   .Help("Ruuvitag relative humidity 0-100%")
                                   .Register(*registry),
                               &ruuvi_data_format_5::humidity });

        collectors.push_back({ BuildGauge()
                                   .Name("ruuvi_pressure_pascals")
                                   .Help("Ruuvitag pressure in Pascal")
                                   .Register(*registry),
                               &ruuvi_data_format_5::pressure });

        collectors.push_back({ BuildGauge()
                                   .Name("ruuvi_acceleration_gs")
                                   .Help("Ruuvitag acceleration in Gs")
                                   .Register(*registry),
                               [](ruuvi_data_format_5 const& p) { return p.acceleration[0]; },
                               { { "axis", "x" } } });

        collectors.push_back({ BuildGauge()
                                   .Name("ruuvi_acceleration_gs")
                                   .Help("Ruuvitag acceleration in Gs")
                                   .Register(*registry),
                               [](ruuvi_data_format_5 const& p) { return p.acceleration[1]; },
                               { { "axis", "y" } } });

        collectors.push_back({ BuildGauge()
                                   .Name("ruuvi_acceleration_gs")
                                   .Help("Ruuvitag acceleration in Gs")
                                   .Register(*registry),
                               [](ruuvi_data_format_5 const& p) { return p.acceleration[2]; },
                               { { "axis", "z" } } });

        collectors.push_back({ BuildGauge()
                                   .Name("ruuvi_battery_volts")
                                   .Help("Ruuvitag battery voltage")
                                   .Register(*registry),
                               &ruuvi_data_format_5::battery_voltage });

        collectors.push_back({ BuildGauge()
                                   .Name("ruuvi_movement_count")
                                   .Help("Ruuvitag movement counter")
                                   .Register(*registry),
                               &ruuvi_data_format_5::movement_counter });

        collectors.push_back({ BuildGauge()
                                   .Name("ruuvi_tx_power_dbm")
                                   .Help("Ruuvitag transmit power")
                                   .Register(*registry),
                               &ruuvi_data_format_5::tx_power });

        collectors.push_back({ BuildGauge()
                                   .Name("ruuvi_measurement_count")
                                   .Help("Ruuvitag packet measurement sequence number [0-65335]")
                                   .Register(*registry),
                               &ruuvi_data_format_5::measurement_sequence });

        collectors.push_back({ BuildGauge()
                                   .Name("ruuvi_rssi_dbm")
                                   .Help("Ruuvitag received signal strength rssi")
                                   .Register(*registry),
                               &ruuvi_data_format_5::signal_strength });

        collectors.push_back({ BuildGauge()
                                   .Name("ruuvi_accelerayion_gs_total")
                                   .Help("Total acceleration of ruuvitag, hypot(x, y, z")
                                   .Register(*registry),
                               &ruuvi_data_format_5::acceleration_total });

        errors_counter =
            &BuildCounter().Name("ruuvi_errors_total").Help("Number of errors").Register(*registry);

        measurements_total = &BuildCounter()
                                  .Name("ruuvi_received_measurements_total")
                                  .Help("Total count of received measurements")
                                  .Register(*registry);

        exposer.RegisterCollectable(registry);
        exposer.RegisterCollectable(sysinfo_collector);
    }

    void update_data(ruuvi_data_format_5 const& new_data) {
        std::lock_guard grd(mtx);
        for (auto& c : collectors) { c.update(new_data); }
        measurements_total->Add({ { "mac", new_data.mac } }).Increment();
        auto& e = errors_counter->Add({ { "mac", new_data.mac } });
        if (new_data.contains_errors) e.Increment();
    }

private:
    Exposer exposer;
    const std::shared_ptr<Registry> registry;
    std::vector<MetricCollector> collectors;
    Family<Counter>* errors_counter;
    Family<Counter>* measurements_total;
    std::mutex mtx;

    // Internal measurements
    std::shared_ptr<SysinfoCollector> sysinfo_collector;
};

RuuviExposer::RuuviExposer(std::string const& addr): impl(std::make_unique<Impl>(addr)) {}

RuuviExposer::~RuuviExposer() = default;

void RuuviExposer::update(ruuvi_data_format_5 const& data) {
    impl->update_data(data);
}
