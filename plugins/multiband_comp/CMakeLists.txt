mm_add_plugin(NAME multiband_comp PRODUCT_NAME "Multiband Compressor" CODE Mbcp)
target_sources(multiband_comp PRIVATE MultibandCompProcessor.cpp MultibandCompEditor.cpp)
