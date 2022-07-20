

find_path(Gattlib_INCLUDE_DIR NAMES gattlib.h REQUIRED)
mark_as_advanced(Gattlib_INCLUDE_DIR)

find_library(Gattlib_LIBRARY NAMES gattlib REQUIRED)
mark_as_advanced(Gattlib_LIBRARY)

set(Gattlib_FOUND TRUE)

if(Gattlib_FOUND)
    set(Gattlib_INCLUDE_DIRS ${Gattlib_INCLUDE_DIR})
    set(Gattlib_LIBRARIES ${Gattlib_LIBRARY})
    if (NOT TARGET Gattlib::Gattlib)
        add_library(Gattlib::Gattlib UNKNOWN IMPORTED)
        set_target_properties(Gattlib::Gattlib PROPERTIES
        IMPORTED_LOCATION "${Gattlib_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${Gattlib_INCLUDE_DIR}")
    endif()
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    target_compile_definitions(Gattlib::Gattlib INTERFACE GATTLIB_LOG_LEVEL=3)
else()
    target_compile_definitions(Gattlib::Gattlib INTERFACE GATTLIB_LOG_LEVEL=1)
endif()
