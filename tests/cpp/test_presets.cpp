#include "Params.h" // generated for utility
#include "plugins/utility/UtilityProcessor.h"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("every factory preset loads and names itself")
{
    UtilityProcessor p;
    int factory = 0;
    for (const auto& info : p.presets().list())
    {
        if (!info.factory)
            continue;
        ++factory;
        REQUIRE(p.presets().load(info));
        REQUIRE(p.presets().currentName() == info.name);
        for (const auto& id : info.paramIds)
            REQUIRE(mm::plugins::utility::table().find(id.toStdString()) != nullptr);
    }
    REQUIRE(factory >= 6);
    REQUIRE(p.presets().list().size() >= 6);
}
