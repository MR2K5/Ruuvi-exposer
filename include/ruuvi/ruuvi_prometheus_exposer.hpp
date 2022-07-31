#pragma once

#include "ruuvi.hpp"

#include <memory>

namespace ruuvi {

/**
 * @brief The RuuviExposer class
 */
class RuuviExposer {
public:
    ~RuuviExposer();
    RuuviExposer(std::string const& addr);

    /**
     * @brief update Updates prometheus with values from data, with respect to its mac
     * This is done in thread-safe manner
     * @param data
     */
    void update(ruuvi_data_format_5 const& data);

private:
    class Impl;
    std::unique_ptr<Impl> impl;
    friend struct D;
};

struct D {
    void operator()(RuuviExposer::Impl* p);
    static std::unique_ptr<RuuviExposer::Impl, D> test();
};

}  // namespace ruuvi
