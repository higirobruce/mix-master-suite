mm_add_plugin(NAME channel_eq PRODUCT_NAME "Channel EQ" CODE Cheq)
target_sources(channel_eq PRIVATE ChannelEqProcessor.cpp ChannelEqEditor.cpp)
