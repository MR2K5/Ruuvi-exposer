
target_sources(sysinfo PRIVATE ruuvi/system_info_exposer.cpp)
target_sources(ruuvitag PRIVATE ruuvi/ruuvi.cpp ruuvi/ruuvi_prometheus_exposer.cpp)
target_sources(ruuvi-exposer PRIVATE main.cpp)
