# mm_snapshot: renders a plugin editor offscreen to PNG (dark + light, given scale) for visual review and M6 layout tests.
add_executable(mm_snapshot snapshot_editor.cpp)
target_include_directories(mm_snapshot PRIVATE ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR}/generated/utility)
target_link_libraries(mm_snapshot PRIVATE utility mm_gui mm_plugin_base)
mm_set_warnings(mm_snapshot)
mm_enable_sanitizers(mm_snapshot)
