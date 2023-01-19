

add_library(test-options INTERFACE)
target_link_libraries(test-options INTERFACE options GTest::gtest_main)

add_executable(test-Ruuvi "test-ruuvi.cpp")
target_link_libraries(test-Ruuvi PRIVATE test-options Ruuvi)
add_test(NAME "Test Ruuvi data decoding" COMMAND test-Ruuvi)

