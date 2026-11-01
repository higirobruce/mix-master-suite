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

# ---- plugin base tests (JUCE)
add_executable(mm_plugin_base_tests
    test_plugin_base.cpp)
target_include_directories(mm_plugin_base_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(mm_plugin_base_tests PRIVATE mm_core mm_plugin_base juce::juce_audio_processors juce::juce_dsp
    Catch2::Catch2WithMain)
target_compile_definitions(mm_plugin_base_tests PRIVATE JUCE_STANDALONE_APPLICATION=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
    JUCE_MODAL_LOOPS_PERMITTED=0)
mm_set_warnings(mm_plugin_base_tests)
mm_enable_sanitizers(mm_plugin_base_tests)
catch_discover_tests(mm_plugin_base_tests)

# ---- utility plugin tests (link the plugin's static sources via the shared-code target JUCE creates)
add_executable(mm_utility_tests
    test_utility_params.cpp)
target_include_directories(mm_utility_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_BINARY_DIR}/generated/utility)
target_link_libraries(mm_utility_tests PRIVATE utility mm_plugin_base Catch2::Catch2WithMain)
mm_set_warnings(mm_utility_tests)
mm_enable_sanitizers(mm_utility_tests)
catch_discover_tests(mm_utility_tests)
