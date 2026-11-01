# mm_bench_processor: in-process cost of a plugin's processBlock (no host wrapper), for comparison with mm_render --bench.
add_executable(mm_bench_processor bench_processor.cpp)
target_include_directories(mm_bench_processor PRIVATE ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR}/generated/utility)
target_link_libraries(mm_bench_processor PRIVATE utility mm_plugin_base)
mm_set_warnings(mm_bench_processor)
mm_enable_sanitizers(mm_bench_processor)
