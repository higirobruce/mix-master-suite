#pragma once
#include "plugin_base/ParamTable.h"
#include <catch2/catch_test_macros.hpp>

namespace mm::test
{
// Every factory preset of a plugin loads, names itself, references only known parameters, and there are at least
// minCount of them (M2: >= 8 per plugin).
template <typename Processor> void checkFactoryPresets(Processor& p, const mm::ParamTable& table, int minCount = 8)
{
    int factory = 0;
    for (const auto& info : p.presets().list())
    {
        if (!info.factory)
            continue;
        ++factory;
        INFO("preset " << info.name);
        REQUIRE(info.name.isNotEmpty());
        REQUIRE(p.presets().load(info));
        REQUIRE(p.presets().currentName() == info.name);
        REQUIRE(info.paramIds.size() == static_cast<int>(table.specs().size()));
        for (const auto& id : info.paramIds)
            REQUIRE(table.find(id.toStdString()) != nullptr);
    }
    REQUIRE(factory >= minCount);
}
} // namespace mm::test
