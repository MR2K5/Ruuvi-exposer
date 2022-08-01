
set(RV_HEADERS ruuvi.hpp ruuvi_prometheus_exposer.hpp)
list(TRANSFORM RV_HEADERS PREPEND ${CMAKE_CURRENT_SOURCE_DIR}/ruuvi/)

target_sources(ruuvitag PUBLIC FILE_SET HEADERS FILES ${RV_HEADERS})
target_sources(logging PUBLIC FILE_SET HEADERS FILES ${CMAKE_CURRENT_SOURCE_DIR}/logging/logging.hpp)
target_sources(sysinfo PUBLIC FILE_SET HEADERS FILES ${CMAKE_CURRENT_SOURCE_DIR}/sysinfo/system_info_exposer.hpp)
