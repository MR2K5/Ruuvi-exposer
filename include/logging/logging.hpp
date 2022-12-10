#pragma once

#include <iostream>
#include <mutex>

namespace logging {

template<class... As> void log(As&&... as) noexcept {
    static std::mutex mtx;
    std::lock_guard g(mtx);
    try {
        (std::clog << ... << as) << std::endl;
    } catch (...) {}
}

}  // namespace logging
