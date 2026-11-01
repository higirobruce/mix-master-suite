#include "Params.h" // generated for utility
#include "helpers/preset_checks.h"
#include "plugins/utility/UtilityProcessor.h"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("every utility factory preset loads and names itself")
{
    UtilityProcessor p;
    mm::test::checkFactoryPresets(p, mm::plugins::utility::table(), 8);
}
