#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/world/terrain/StructureReferenceOrder.hpp>

#include <doctest.h>

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

using namespace basalt::server::world::terrain;

namespace {

using References = std::vector<std::pair<std::int32_t, std::int32_t>>;

[[nodiscard]] std::string describe(const References& references) {
    std::string text;
    for (const auto& [x, z] : vanillaStructureReferenceChunkOrder(references)) {
        if (!text.empty()) {
            text += ';';
        }
        text += std::to_string(x) + "," + std::to_string(z);
    }
    return text;
}

void check(const References& references, std::size_t size, std::string_view order) {
    CHECK(vanillaStructureReferenceChunkOrder(references).size() == size);
    CHECK(describe(references) == order);
}

[[nodiscard]] References grid() {
    References all;
    for (std::int32_t x = -1; x <= 1; ++x) {
        for (std::int32_t z = -1; z <= 1; ++z) {
            all.emplace_back(x, z);
        }
    }
    return all;
}

[[nodiscard]] References row() {
    References all;
    for (std::int32_t index = 0; index < 12; ++index) {
        all.emplace_back(index, 0);
    }
    return all;
}

[[nodiscard]] References negatives() {
    References all;
    for (std::int32_t index = 0; index < 9; ++index) {
        all.emplace_back(-index, index - 4);
    }
    return all;
}

[[nodiscard]] References full() {
    References all;
    for (std::int32_t index = 0; index < 23; ++index) {
        all.emplace_back(index * 7, index * -3);
    }
    return all;
}

} // namespace

TEST_CASE("the structure references come back in the frozen kotlin's order") {
    SUBCASE("empty") { check({}, 0, ""); }

    SUBCASE("one") { check({{3, 5}}, 1, "3,5"); }

    // The origin is the one key the map keeps outside its table, and it comes back first.
    SUBCASE("withOrigin") { check({{3, 5}, {0, 0}, {-7, 2}}, 3, "0,0;3,5;-7,2"); }

    SUBCASE("nine") { check(grid(), 9, "0,0;0,1;0,-1;1,-1;-1,0;-1,1;1,1;1,0;-1,-1"); }

    SUBCASE("row") { check(row(), 12, "0,0;5,0;9,0;6,0;1,0;11,0;10,0;7,0;8,0;3,0;4,0;2,0"); }

    SUBCASE("negatives") { check(negatives(), 9, "-7,3;0,-4;-4,0;-6,2;-8,4;-1,-3;-3,-1;-5,1;-2,-2"); }

    SUBCASE("far") {
        check({{1000, -1000}, {-1000, 1000}, {1000, 1000}, {-1000, -1000}}, 4,
              "-1000,1000;1000,1000;1000,-1000;-1000,-1000");
    }

    SUBCASE("full") {
        check(full(), 23,
              "0,0;112,-48;105,-45;98,-42;63,-27;14,-6;147,-63;133,-57;28,-12;126,-54;77,-33;70,-30;21,-9;91,-39;56,-"
              "24;49,-21;7,-3;154,-66;35,-15;84,-36;119,-51;140,-60;42,-18");
    }
}
