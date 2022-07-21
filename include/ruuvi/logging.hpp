#pragma once

#include <iostream>

namespace sys_info {

template<class... As> void log(As&&... as) {
    (std::clog << ... << as) << std::endl;
}

}  // namespace sys_info
