mm_add_plugin(NAME channel_comp PRODUCT_NAME "Channel Compressor" CODE Chcp)
target_sources(channel_comp PRIVATE ChannelCompProcessor.cpp ChannelCompEditor.cpp)
