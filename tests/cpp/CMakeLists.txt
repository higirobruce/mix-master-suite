include(Catch)

add_executable(mm_core_tests
    test_smoke.cpp
    test_math.cpp
    test_fdn.cpp
    test_math_m1.cpp
    test_biquad.cpp
    test_matched.cpp
    test_svf.cpp
    test_onepole_allpass_m1.cpp
    test_fir.cpp
    test_linkwitz_riley.cpp
    test_detector.cpp
    test_envelope.cpp
    test_gain_computer.cpp
    test_lookahead.cpp
    test_sidechain_filter.cpp
    test_fractional_delay.cpp
    test_oversampler.cpp
    test_waveshapers.cpp
    test_adaa.cpp
    test_tape.cpp
    test_tube.cpp
    test_correlation.cpp
    test_spectrum.cpp
    test_smoother.cpp
    test_integer_delay.cpp
    test_onepole.cpp
    test_allpass.cpp
    test_meter_fifo.cpp
    test_meters.cpp)
target_include_directories(mm_core_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(mm_core_tests PRIVATE mm_core Catch2::Catch2WithMain)
target_compile_definitions(mm_core_tests PRIVATE JUCE_STANDALONE_APPLICATION=1)
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
    test_utility_params.cpp
    test_presets.cpp)
target_include_directories(mm_utility_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_BINARY_DIR}/generated/utility)
target_link_libraries(mm_utility_tests PRIVATE utility mm_plugin_base Catch2::Catch2WithMain)
mm_set_warnings(mm_utility_tests)
mm_enable_sanitizers(mm_utility_tests)
catch_discover_tests(mm_utility_tests)

# ---- mm_render CLI smoke tests
add_executable(mm_render_tests test_render_cli.cpp)
target_include_directories(mm_render_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(mm_render_tests PRIVATE mm_plugin_base juce::juce_audio_formats Catch2::Catch2WithMain)
target_compile_definitions(mm_render_tests PRIVATE JUCE_STANDALONE_APPLICATION=1)
mm_set_warnings(mm_render_tests)
mm_enable_sanitizers(mm_render_tests)
target_compile_definitions(mm_render_tests PRIVATE
    MM_RENDER_PATH="$<TARGET_FILE:mm_render>"
    MM_UTILITY_VST3_PATH="$<TARGET_FILE_DIR:utility_VST3>/../.."
    MM_TEST_TMP_PATH="${CMAKE_BINARY_DIR}/tests/tmp")
add_dependencies(mm_render_tests mm_render utility_VST3)
catch_discover_tests(mm_render_tests)

# ---- GUI tests (shared chrome + Utility editor)
add_executable(mm_gui_tests test_gui.cpp)
target_include_directories(mm_gui_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_BINARY_DIR}/generated/utility)
target_link_libraries(mm_gui_tests PRIVATE utility mm_gui mm_plugin_base Catch2::Catch2WithMain)
mm_set_warnings(mm_gui_tests)
mm_enable_sanitizers(mm_gui_tests)
catch_discover_tests(mm_gui_tests)

# ---- channel EQ tests
add_executable(mm_channel_eq_tests test_channel_eq.cpp)
target_include_directories(mm_channel_eq_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_BINARY_DIR}/generated/channel_eq)
target_link_libraries(mm_channel_eq_tests PRIVATE channel_eq mm_plugin_base Catch2::Catch2WithMain)
mm_set_warnings(mm_channel_eq_tests)
mm_enable_sanitizers(mm_channel_eq_tests)
catch_discover_tests(mm_channel_eq_tests)

# ---- channel EQ GUI tests
add_executable(mm_gui_channel_eq_tests test_gui_channel_eq.cpp)
target_include_directories(mm_gui_channel_eq_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_BINARY_DIR}/generated/channel_eq)
target_link_libraries(mm_gui_channel_eq_tests PRIVATE channel_eq mm_gui mm_plugin_base Catch2::Catch2WithMain)
mm_set_warnings(mm_gui_channel_eq_tests)
mm_enable_sanitizers(mm_gui_channel_eq_tests)
catch_discover_tests(mm_gui_channel_eq_tests)

# ---- channel compressor tests
add_executable(mm_channel_comp_tests test_channel_comp.cpp)
target_include_directories(mm_channel_comp_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_BINARY_DIR}/generated/channel_comp)
target_link_libraries(mm_channel_comp_tests PRIVATE channel_comp mm_plugin_base Catch2::Catch2WithMain)
mm_set_warnings(mm_channel_comp_tests)
mm_enable_sanitizers(mm_channel_comp_tests)
catch_discover_tests(mm_channel_comp_tests)

# ---- channel compressor GUI tests
add_executable(mm_gui_channel_comp_tests test_gui_channel_comp.cpp)
target_include_directories(mm_gui_channel_comp_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_BINARY_DIR}/generated/channel_comp)
target_link_libraries(mm_gui_channel_comp_tests PRIVATE channel_comp mm_gui mm_plugin_base Catch2::Catch2WithMain)
mm_set_warnings(mm_gui_channel_comp_tests)
mm_enable_sanitizers(mm_gui_channel_comp_tests)
catch_discover_tests(mm_gui_channel_comp_tests)

# ---- gate
add_executable(mm_gate_tests test_gate.cpp)
target_include_directories(mm_gate_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_BINARY_DIR}/generated/gate)
target_link_libraries(mm_gate_tests PRIVATE gate mm_plugin_base Catch2::Catch2WithMain)
mm_set_warnings(mm_gate_tests)
mm_enable_sanitizers(mm_gate_tests)
catch_discover_tests(mm_gate_tests)

# ---- gate GUI tests
add_executable(mm_gui_gate_tests test_gui_gate.cpp)
target_include_directories(mm_gui_gate_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_BINARY_DIR}/generated/gate)
target_link_libraries(mm_gui_gate_tests PRIVATE gate mm_gui mm_plugin_base Catch2::Catch2WithMain)
mm_set_warnings(mm_gui_gate_tests)
mm_enable_sanitizers(mm_gui_gate_tests)
catch_discover_tests(mm_gui_gate_tests)

# ---- saturation
add_executable(mm_saturation_tests test_saturation.cpp)
target_include_directories(mm_saturation_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_BINARY_DIR}/generated/saturation)
target_link_libraries(mm_saturation_tests PRIVATE saturation mm_plugin_base Catch2::Catch2WithMain)
mm_set_warnings(mm_saturation_tests)
mm_enable_sanitizers(mm_saturation_tests)
catch_discover_tests(mm_saturation_tests)

# ---- saturation GUI tests
add_executable(mm_gui_saturation_tests test_gui_saturation.cpp)
target_include_directories(mm_gui_saturation_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_BINARY_DIR}/generated/saturation)
target_link_libraries(mm_gui_saturation_tests PRIVATE saturation mm_gui mm_plugin_base Catch2::Catch2WithMain)
mm_set_warnings(mm_gui_saturation_tests)
mm_enable_sanitizers(mm_gui_saturation_tests)
catch_discover_tests(mm_gui_saturation_tests)

# ---- delay
add_executable(mm_delay_tests test_delay.cpp)
target_include_directories(mm_delay_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_BINARY_DIR}/generated/delay)
target_link_libraries(mm_delay_tests PRIVATE delay mm_plugin_base Catch2::Catch2WithMain)
mm_set_warnings(mm_delay_tests)
mm_enable_sanitizers(mm_delay_tests)
catch_discover_tests(mm_delay_tests)

# ---- delay GUI tests
add_executable(mm_gui_delay_tests test_gui_delay.cpp)
target_include_directories(mm_gui_delay_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_BINARY_DIR}/generated/delay)
target_link_libraries(mm_gui_delay_tests PRIVATE delay mm_gui mm_plugin_base Catch2::Catch2WithMain)
mm_set_warnings(mm_gui_delay_tests)
mm_enable_sanitizers(mm_gui_delay_tests)
catch_discover_tests(mm_gui_delay_tests)

# ---- reverb
add_executable(mm_reverb_tests test_reverb.cpp)
target_include_directories(mm_reverb_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_BINARY_DIR}/generated/reverb)
target_link_libraries(mm_reverb_tests PRIVATE reverb mm_plugin_base Catch2::Catch2WithMain)
mm_set_warnings(mm_reverb_tests)
mm_enable_sanitizers(mm_reverb_tests)
catch_discover_tests(mm_reverb_tests)
