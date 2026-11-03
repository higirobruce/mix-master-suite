mm_add_plugin(NAME bus_comp PRODUCT_NAME "Bus Compressor" CODE Bscp)
target_sources(bus_comp PRIVATE BusCompProcessor.cpp BusCompEditor.cpp)
