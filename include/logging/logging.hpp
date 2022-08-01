#pragma once

#include <iostream>

namespace logging {

template<class... As> void log(As&&... as) {
    (std::clog << ... << as) << std::endl;
}

}  // namespace logging
