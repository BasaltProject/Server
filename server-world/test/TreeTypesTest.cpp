#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <basalt/server/world/terrain/decoration/tree/TreeTypes.hpp>

#include <doctest.h>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

using namespace basalt::server::world::terrain::decoration::tree;

namespace {

constexpr std::array<std::int32_t, 4> kTreeHeights = {4, 7, 12, 20};

void appendBands(std::string& out, const FeatureSize& size) {
    for (const std::int32_t treeHeight : kTreeHeights) {
        for (std::int32_t yo = 0; yo <= treeHeight; ++yo) {
            out.append(std::to_string(size.sizeAtHeight(treeHeight, yo)));
        }
        out.push_back('|');
    }
    out.append("clip=");
    out.append(size.minClippedHeight().has_value() ? std::to_string(*size.minClippedHeight()) : "null");
    out.push_back(';');
}

} // namespace

TEST_CASE("the two-layer feature sizes match the frozen kotlin") {
    std::string rendered;
    for (const FeatureSize& size :
         std::vector<FeatureSize>{FeatureSize::twoLayers(1, 0, 1), FeatureSize::twoLayers(2, 0, 2),
                                  FeatureSize::twoLayers(1, 0, 2, 4), FeatureSize::twoLayers(3, 0, 2)}) {
        appendBands(rendered, size);
    }
    CHECK(rendered == "01111|01111111|0111111111111|011111111111111111111|clip=null;00222|00222222|0022222222222|"
                      "002222222222222222222|clip=null;02222|02222222|0222222222222|022222222222222222222|clip=4;00022|"
                      "00022222|0002222222222|000222222222222222222|clip=null;");
}

TEST_CASE("the three-layer feature sizes match the frozen kotlin") {
    // The upper band is measured from the top down, so the same shape gives different answers at
    // different tree heights: that is what the four heights per shape are for.
    std::string rendered;
    for (const FeatureSize& size :
         std::vector<FeatureSize>{FeatureSize::threeLayers(1, 1, 0, 1, 2), FeatureSize::threeLayers(0, 0, 1, 2, 1),
                                  FeatureSize::threeLayers(2, 3, 0, 2, 1)}) {
        appendBands(rendered, size);
    }
    CHECK(rendered == "01122|01111122|0111111111122|011111111111111111122|clip=null;22221|22222221|2222222222221|"
                      "222222222222222222221|clip=null;00111|00221111|0022222221111|002222222222222221111|clip=null;");
}
