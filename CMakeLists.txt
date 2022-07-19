cmake_minimum_required(VERSION 3.5)

project(ruuvitag LANGUAGES CXX)

# use 17, raspberrypi doesn't have c++20
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(RUUVITAG_SANITIZE "" CACHE STRING "Enable a sanitizer")
set_property(CACHE RUUVITAG_SANITIZE PROPERTY STRINGS "" "thread" "undefined")

add_library(ruuvitag SHARED "")
add_executable(Test "")

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

target_compile_definitions(common INTERFACE GATTLIB_LOG_LEVEL=3)

target_link_libraries(ruuvitag PRIVATE common)
target_link_libraries(Test PRIVATE common)

target_include_directories(ruuvitag PRIVATE /usr/local/include)
target_include_directories(Test PRIVATE src)

find_package(Threads REQUIRED)
find_package(Python3 REQUIRED COMPONENTS Development)

target_link_libraries(Test PUBLIC ruuvitag Threads::Threads gattlib Python3::Python)

find_package(prometheus-cpp CONFIG REQUIRED)

target_link_libraries(Test PRIVATE prometheus-cpp::pull Threads::Threads)

add_subdirectory(src)
