
target_sources(Ruuvi PRIVATE ruuvi/ruuvi.cpp ruuvi/ruuvi_prometheus_exposer.cpp)
target_sources(ruuvi-exposer PRIVATE main.cpp)
target_sources(Ble PRIVATE ble/receiver.cpp)

target_sources(Sysinfo PRIVATE ruuvi/system_info_exposer.cpp ruuvi/diskstat_exposer.cpp)

if (${BUILD_SYSINFO_EXPOSER})
    target_sources(Sysinfo PRIVATE ruuvi/raw_gauge.cpp ruuvi/system_info.cpp ruuvi/diskstat.cpp)
endif()
