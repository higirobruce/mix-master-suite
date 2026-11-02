mm_add_plugin(NAME gate PRODUCT_NAME "Gate" CODE Gate)
target_sources(gate PRIVATE GateProcessor.cpp GateEditor.cpp)
