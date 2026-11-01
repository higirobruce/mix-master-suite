# mm_render: offline host for plugins (VST3/AU) and core modules, driven by a JSON job. Used by the Python
# measurement suite, the CPU benchmark and ctest.
juce_add_console_app(mm_render PRODUCT_NAME "mm_render")
target_sources(mm_render PRIVATE
    Main.cpp
    Job.cpp
    Signals.cpp
    ModuleRegistry.cpp
    PluginTarget.cpp
    Bench.cpp)
target_include_directories(mm_render PRIVATE ${CMAKE_SOURCE_DIR})
target_compile_definitions(mm_render PRIVATE
    JUCE_PLUGINHOST_VST3=1
    JUCE_PLUGINHOST_AU=$<IF:$<PLATFORM_ID:Darwin>,1,0>
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=0)
target_link_libraries(mm_render PRIVATE
    mm_core
    juce::juce_audio_processors juce::juce_audio_formats juce::juce_audio_utils juce::juce_dsp
    juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)
mm_set_warnings(mm_render)
mm_enable_sanitizers(mm_render)
