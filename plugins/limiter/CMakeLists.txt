mm_add_plugin(NAME limiter PRODUCT_NAME "True-peak Limiter" CODE Tplm)
target_sources(limiter PRIVATE LimiterProcessor.cpp LimiterEditor.cpp)
