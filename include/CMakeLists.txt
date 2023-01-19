
target_include_directories(Sysinfo PRIVATE sysinfo PUBLIC .)
target_sources(Sysinfo PUBLIC FILE_SET HEADERS FILES sysinfo/system_info_exposer.hpp)


target_include_directories(Ruuvi PRIVATE ruuvi PUBLIC .)
target_sources(Ruuvi PUBLIC FILE_SET HEADERS FILES ruuvi/ruuvi.hpp ruuvi/ruuvi_prometheus_exposer.hpp)

target_include_directories(Ble PRIVATE ble PUBLIC .)
target_sources(Ble PUBLIC FILE_SET HEADERS FILES ble/receiver.hpp)
