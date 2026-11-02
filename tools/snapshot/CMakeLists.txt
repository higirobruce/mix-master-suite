# mm_snapshot_<plugin>: renders a plugin editor offscreen to PNG (dark + light, given scale) for visual review and
# M6 layout tests. One executable per plugin (a plugin's JUCE macros cannot coexist with another's in one binary).
function(mm_add_snapshot plugin header class)
    add_executable(mm_snapshot_${plugin} snapshot_editor.cpp)
    target_include_directories(mm_snapshot_${plugin} PRIVATE ${CMAKE_SOURCE_DIR})
    target_compile_definitions(mm_snapshot_${plugin} PRIVATE
        MM_SNAPSHOT_HEADER="${header}" MM_SNAPSHOT_CLASS=${class} MM_SNAPSHOT_NAME="${plugin}")
    target_link_libraries(mm_snapshot_${plugin} PRIVATE ${plugin} mm_gui mm_plugin_base)
    mm_set_warnings(mm_snapshot_${plugin})
    mm_enable_sanitizers(mm_snapshot_${plugin})
endfunction()

mm_add_snapshot(utility plugins/utility/UtilityProcessor.h UtilityProcessor)
mm_add_snapshot(channel_eq plugins/channel_eq/ChannelEqProcessor.h ChannelEqProcessor)
mm_add_snapshot(channel_comp plugins/channel_comp/ChannelCompProcessor.h ChannelCompProcessor)
mm_add_snapshot(gate plugins/gate/GateProcessor.h GateProcessor)
mm_add_snapshot(saturation plugins/saturation/SaturationProcessor.h SaturationProcessor)
mm_add_snapshot(delay plugins/delay/DelayProcessor.h DelayProcessor)
mm_add_snapshot(reverb plugins/reverb/ReverbProcessor.h ReverbProcessor)
