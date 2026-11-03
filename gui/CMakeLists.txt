# mm_gui: shared chrome for every plugin (ADR pending for M6 details): theme, look-and-feel, knobs, toggles,
# meters, header, footer, editor base. Vector only; fonts (Inter, OFL) compiled in as binary data.
juce_add_binary_data(mm_gui_fonts NAMESPACE mm_gui_fonts HEADER_NAME mm_gui_fonts.h
    SOURCES fonts/Inter-Regular.ttf fonts/Inter-Medium.ttf)
add_library(mm_gui STATIC
    MMLookAndFeel.cpp
    Knob.cpp
    Toggle.cpp
    Selector.cpp
    MeterBar.cpp
    HeaderBar.cpp
    Footer.cpp
    PluginEditorBase.cpp
    CurveDisplay.cpp
    SpectrumDisplay.cpp
    MultibandDisplay.cpp
    NeedleMeter.cpp
    AdvancedDrawer.cpp
    TransferCurveDisplay.cpp
    HistoryDisplay.cpp
    GainReductionMeter.cpp
    HarmonicDisplay.cpp
    TapDisplay.cpp
    DecayDisplay.cpp
    SibilanceDisplay.cpp
    Goniometer.cpp)
target_include_directories(mm_gui PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(mm_gui
    PUBLIC mm_plugin_base juce::juce_gui_extra juce::juce_audio_utils
    PRIVATE mm_gui_fonts juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)
mm_set_warnings(mm_gui)
mm_enable_sanitizers(mm_gui)
