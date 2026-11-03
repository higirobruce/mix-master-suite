mm_add_plugin(NAME deesser PRODUCT_NAME "De-esser" CODE Dees)
target_sources(deesser PRIVATE DeEsserProcessor.cpp DeEsserEditor.cpp)
