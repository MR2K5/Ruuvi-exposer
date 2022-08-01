
target_include_directories(sysinfo PRIVATE sysinfo PUBLIC .)
target_sources(sysinfo PUBLIC FILE_SET HEADERS FILES sysinfo/system_info_exposer.hpp)

target_include_directories(ruuvitag PRIVATE ruuvi PUBLIC .)
target_sources(ruuvitag PUBLIC FILE_SET HEADERS FILES ruuvi/ruuvi.hpp ruuvi/ruuvi_prometheus_exposer.hpp)
