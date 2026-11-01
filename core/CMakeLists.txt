# mm_core: shared DSP library, namespace mm. Header-only (INTERFACE); the oversampler wraps juce::dsp, so consumers
# of mm_core compile the JUCE dsp module themselves (JUCE's per-target module model).
add_library(mm_core INTERFACE)
target_include_directories(mm_core INTERFACE ${CMAKE_SOURCE_DIR})
target_compile_features(mm_core INTERFACE cxx_std_20)
target_link_libraries(mm_core INTERFACE juce::juce_dsp)
target_compile_definitions(mm_core INTERFACE
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
