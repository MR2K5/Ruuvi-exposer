
target_include_directories(ruuvi-exposer PRIVATE sysinfo PUBLIC .)
target_sources(ruuvi-exposer PUBLIC FILE_SET HEADERS FILES sysinfo/system_info_exposer.hpp)

target_include_directories(ruuvi-exposer PRIVATE ruuvi PUBLIC .)
target_sources(ruuvi-exposer PUBLIC FILE_SET HEADERS FILES ruuvi/ruuvi.hpp ruuvi/ruuvi_prometheus_exposer.hpp)

target_include_directories(ruuvi-exposer PRIVATE ble)
