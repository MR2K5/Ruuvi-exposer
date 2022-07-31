cmake_minimum_required(VERSION 3.23)

project(ruuvitag LANGUAGES CXX)

# use 17, raspberrypi doesn't have c++20
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(RUUVITAG_SANITIZE "" CACHE STRING "Enable a sanitizer")
set_property(CACHE RUUVITAG_SANITIZE PROPERTY STRINGS "" "thread" "undefined")

set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

add_library(ruuvitag SHARED "")
add_executable(ruuvi_exposer "")

add_library(common INTERFACE)

add_custom_target(print COMMAND echo -fsanitize=${RUUVITAG_SANITIZE})

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(common INTERFACE -Wall -Wextra -pedantic -stdlib=libstdc++
        -fsanitize=${RUUVITAG_SANITIZE})
    target_link_options(common INTERFACE -stdlib=libstdc++ -fsanitize=${RUUVITAG_SANITIZE})
endif()
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(common INTERFACE -Wall -Wextra -pedantic)
    target_link_options(common INTERFACE)
endif()

target_compile_definitions(common INTERFACE)

target_link_libraries(ruuvitag PRIVATE common)
target_link_libraries(ruuvi_exposer PRIVATE common)

target_include_directories(ruuvitag PRIVATE ${PROJECT_SOURCE_DIR}/include/ruuvi)
target_include_directories(ruuvi_exposer PRIVATE ${PROJECT_SOURCE_DIR}/include/ruuvi)
target_include_directories(ruuvitag PUBLIC /usr/local/include ${PROJECT_SOURCE_DIR}/include)

find_package(Threads REQUIRED)
find_package(Python3 REQUIRED COMPONENTS Development)

target_link_libraries(ruuvi_exposer PUBLIC ruuvitag Threads::Threads Python3::Python)

#find_package(prometheus-cpp CONFIG REQUIRED)
#target_link_libraries(ruuvi_exposer PRIVATE prometheus-cpp::pull Threads::Threads)
add_subdirectory(ext/prometheus-cpp)
target_link_libraries(ruuvi_exposer PRIVATE prometheus-cpp::pull)

#find_package(Gattlib REQUIRED MODULE)
#set(GATTLIB_BUILD_EXAMPLES CACHE "" OFF)
#add_subdirectory(ext/gattlib)
find_package(Gattlib REQUIRED MODULE)
target_link_libraries(ruuvi_exposer PRIVATE Gattlib::Gattlib)


add_subdirectory(src)
add_subdirectory(include)

set_target_properties(ruuvi_exposer PROPERTIES INSTALL_RPATH ${CMAKE_INSTALL_PREFIX}/lib/)

install(TARGETS ruuvitag ruuvi_exposer
    FILE_SET HEADERS DESTINATION include
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin)
