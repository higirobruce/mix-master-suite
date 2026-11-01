include(Catch)

add_executable(mm_core_tests
    test_smoke.cpp
    test_math.cpp
    test_smoother.cpp
    test_integer_delay.cpp
    test_onepole.cpp
    test_allpass.cpp
    test_meter_fifo.cpp
    test_meters.cpp)
target_include_directories(mm_core_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(mm_core_tests PRIVATE mm_core Catch2::Catch2WithMain)
mm_set_warnings(mm_core_tests)
mm_enable_sanitizers(mm_core_tests)
catch_discover_tests(mm_core_tests)
