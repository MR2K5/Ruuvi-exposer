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

// struct sysinfo_metric {
//     using F = std::function<std::vector<ClientMetric>(struct sysinfo const&)>;

//    sysinfo_metric(std::string const& name, std::string const& help, MetricType type,
//                   std::vector<std::string> const& labels_, F const& f) {
//        metricfamily.name = name;
//        metricfamily.help = help;
//        metricfamily.type = type;
//        function          = f;
//        labels            = labels_;
//    }

//    MetricFamily get(struct sysinfo const& info) const {
//        auto metrics = function(info);
//        for (auto& m : metrics) { m.label.insert(m.label.cend(), labels.cbegin(), labels.cend());
//        }

//        MetricFamily r = metricfamily;
//        r.metric       = std::move(metrics);
//        return r;
//    }

//    MetricFamily metricfamily;
//    F function;
//    std::vector<ClientMetric::Label> labels;
//};

// class SysinfoCollector: public Collectable {
//     using Label = ClientMetric::Label;
//     using CMV   = std::vector<ClientMetric>;

// public:
//     std::vector<MetricFamily> Collect() const override {
//         std::vector<MetricFamily> collected;
//         auto info = get_info();

//        for (auto& m : metrics) { m.get(info); }
//        return collected;
//    }

// private:
//     static struct sysinfo get_info() {
//         struct sysinfo info;
//         int e = ::sysinfo(&info);
//         if (e) { throw std::system_error(errno, std::generic_category(), "Failed to get
//         sysinfo"); } return info;
//     }

//    void populate_metrics() {
//        metrics.push_back(sysinfo_metric("sysinfo_processes_total",
//                                         "Total number of processes currently running",
//                                         MetricType::Gauge, {}, [](struct sysinfo const& info) {
//                                             ClientMetric m;
//                                             m.gauge.value = info.procs;
//                                             return std::vector{ m };
//                                         }));
//    }

//    std::vector<sysinfo_metric> metrics;
//};

}  // namespace

class RuuviExposer::Impl {
public:
    Impl(std::string const& addr): exposer(addr), registry(std::make_shared<Registry>()) {
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
};

RuuviExposer::RuuviExposer(std::string const& addr): impl(std::make_unique<Impl>(addr)) {}

RuuviExposer::~RuuviExposer() = default;

void RuuviExposer::update(ruuvi_data_format_5 const& data) {
    impl->update_data(data);
}
