# mm_core: shared DSP library, namespace mm. Header-only templates for now (INTERFACE);
# becomes STATIC when the first .cpp lands (M1 oversampler).
add_library(mm_core INTERFACE)
target_include_directories(mm_core INTERFACE ${CMAKE_SOURCE_DIR})
target_compile_features(mm_core INTERFACE cxx_std_20)
