#pragma once

#include "ruuvi.hpp"

#include <memory>

#include <prometheus/collectable.h>

namespace ruuvi {

/**
 * @brief The RuuviExposer class
 */
class RuuviExposer: public prometheus::Collectable {
public:
    ~RuuviExposer();
    RuuviExposer();

    /**
     * @brief update Updates prometheus with values from data, with respect to its mac
     * This is done in thread-safe manner
     * @param data
     */
    void update(ruuvi_data_format_5 const& data);

    virtual std::vector<prometheus::MetricFamily> Collect() const override;

private:
    class Impl;
    std::unique_ptr<Impl> impl;
};

}  // namespace ruuvi
