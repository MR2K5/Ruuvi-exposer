
set(RUUVITAG_SOURCES ruuvi.cpp ruuvi_prometheus_exposer.cpp)
list(TRANSFORM RUUVITAG_SOURCES PREPEND ${CMAKE_CURRENT_SOURCE_DIR}/ruuvi/)

target_sources(ruuvitag PRIVATE ${RUUVITAG_SOURCES})
target_sources(sysinfo PRIVATE ruuvi/system_info_exposer.cpp)
target_sources(ruuvi_exposer PRIVATE main.cpp)
