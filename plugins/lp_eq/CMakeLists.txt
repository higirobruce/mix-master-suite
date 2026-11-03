mm_add_plugin(NAME lp_eq PRODUCT_NAME "Linear-phase EQ" CODE Lpeq)
target_sources(lp_eq PRIVATE LpEqProcessor.cpp LpEqEditor.cpp)
