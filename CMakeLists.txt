cmake_minimum_required(VERSION 3.23)

project(ruuvitag VERSION 0.1 LANGUAGES CXX)

# use 17, raspberrypi doesn't have c++20
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    message(STATUS "Build type not set, defaulting to Debug")
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "" FORCE)
endif()

find_package(Git QUIET)

if (GIT_FOUND AND EXISTS "${PROJECT_SOURCE_DIR}/.git")
    option(GIT_SUBMODULE "Check submodules during build" ON)

    if (GIT_SUBMODULE)
        message(STATUS "Updating submodules...")
        execute_process(COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive --depth 1
            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR} RESULT_VARIABLE GIT_SUBMODULE_RESULT)
        if (NOT GIT_SUBMODULE_RESULT EQUAL "0")
            message(FATAL_ERROR
                "git submodule update --init --recursive --depth 1 failed with ${GIT_SUBMODULE_ERROR}")
        endif()
    endif()
endif()

if(NOT EXISTS "${PROJECT_SOURCE_DIR}/ext/prometheus-cpp/CMakeLists.txt")
    message(FATAL_ERROR "The submodules were not downloaded!")
endif()

set(OVERRIDE_CXX_STANDARD_FLAGS OFF CACHE BOOL "" FORCE)
add_subdirectory(ext/prometheus-cpp)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
find_package(Gattlib REQUIRED MODULE)

set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE CACHE INTERNAL "Export compile commands")

set(USE_SANITIZER "" CACHE STRING "Select sanitizer used (only works with gcc or clang)")
set_property(CACHE USE_SANITIZER PROPERTY STRINGS "" "thread" "undefined" "address")

add_library(options INTERFACE)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    message(STATUS "Using gcc or clang, adding warnings")
    target_compile_options(options INTERFACE -Wall -Wextra -pedantic $<$<CONFIG:Release>: -march=native>)
    target_link_options(options INTERFACE -Wall -Wextra -pedantic $<$<CONFIG:Release>: -march=native>)
    if(USE_SANITIZER)
        message(STATUS "Using sanitizer ${USE_SANITIZER}")
        target_link_options(options INTERFACE -fsanitize=${USE_SANITIZER})
        target_compile_options(options INTERFACE -fsanitize=${USE_SANITIZER})
    endif()
endif()
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    message(STATUS "Clang: using libstdc++")
    target_compile_options(options INTERFACE -stdlib=libstdc++)
    target_link_options(options INTERFACE -stdlib=libstdc++)
endif()

add_library(sysinfo "")
target_link_libraries(sysinfo PRIVATE options prometheus-cpp::core)

add_library(ruuvitag "")
target_link_libraries(ruuvitag PRIVATE options Gattlib::Gattlib prometheus-cpp::core)

add_executable(ruuvi-exposer "")
target_link_libraries(ruuvi-exposer PRIVATE options sysinfo ruuvitag prometheus-cpp::pull)


add_subdirectory(src)
add_subdirectory(include)

set_target_properties(sysinfo ruuvitag ruuvi-exposer PROPERTIES INSTALL_RPATH ${CMAKE_INSTALL_PREFIX}/lib)
install(TARGETS sysinfo ruuvitag ruuvi-exposer
    LIBRARY
    ARCHIVE
    RUNTIME
    FILE_SET HEADERS
)
