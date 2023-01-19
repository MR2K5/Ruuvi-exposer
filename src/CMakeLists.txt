
target_sources(Ruuvi PRIVATE ruuvi/ruuvi.cpp ruuvi/ruuvi_prometheus_exposer.cpp)
target_sources(ruuvi-exposer PRIVATE main.cpp)
target_sources(Ble PRIVATE ble/receiver.cpp)

target_sources(Sysinfo PRIVATE ruuvi/system_info_exposer.cpp)

