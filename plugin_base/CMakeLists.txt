# mm_plugin_base: the JUCE processor base every plugin derives from (ADR-0002).
add_library(mm_plugin_base STATIC
    ParamTable.cpp
    StateIO.cpp
    AbcdSlots.cpp
    PresetManager.cpp
    PluginProcessorBase.cpp)
target_include_directories(mm_plugin_base PUBLIC ${CMAKE_SOURCE_DIR})
target_compile_features(mm_plugin_base PUBLIC cxx_std_20)
target_compile_definitions(mm_plugin_base PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_MODAL_LOOPS_PERMITTED=0)
target_link_libraries(mm_plugin_base
    PUBLIC mm_core juce::juce_audio_processors juce::juce_dsp
    PRIVATE juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)
mm_set_warnings(mm_plugin_base)
mm_enable_sanitizers(mm_plugin_base)
