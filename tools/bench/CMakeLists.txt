# mm_bench_processor: in-process cost of a plugin's processBlock (no host wrapper), for comparison with mm_render --bench.
add_executable(mm_bench_processor bench_processor.cpp)
target_include_directories(mm_bench_processor PRIVATE ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR}/generated/utility)
target_link_libraries(mm_bench_processor PRIVATE utility mm_plugin_base)
mm_set_warnings(mm_bench_processor)
mm_enable_sanitizers(mm_bench_processor)

# mm_bench_core: ns/sample per core module (M1 Task 17).
add_executable(mm_bench_core bench_core.cpp)
target_include_directories(mm_bench_core PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(mm_bench_core PRIVATE mm_core juce::juce_recommended_config_flags)
target_compile_definitions(mm_bench_core PRIVATE JUCE_STANDALONE_APPLICATION=1)
mm_set_warnings(mm_bench_core)
