
set(RV_HEADERS ruuvi.hpp ruuvi_prometheus_exposer.hpp system_info_exposer.hpp logging.hpp)
list(TRANSFORM RV_HEADERS PREPEND ${CMAKE_CURRENT_SOURCE_DIR}/ruuvi/)

target_sources(ruuvitag PUBLIC FILE_SET HEADERS FILES ${RV_HEADERS})
