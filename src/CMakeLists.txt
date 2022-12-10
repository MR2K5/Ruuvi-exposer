
target_sources(ruuvi-exposer PRIVATE ruuvi/system_info_exposer.cpp)
target_sources(ruuvi-exposer PRIVATE ruuvi/ruuvi.cpp ruuvi/ruuvi_prometheus_exposer.cpp)
target_sources(ruuvi-exposer PRIVATE main.cpp)
target_sources(ruuvi-exposer PRIVATE ble/receiver.cpp)
