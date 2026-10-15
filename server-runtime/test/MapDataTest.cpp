#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/MapColors.hpp>
#include <basalt/server/runtime/world/maps/MapData.hpp>
#include <basalt/server/runtime/world/maps/MapDecorationTypes.hpp>
#include <basalt/server/runtime/world/maps/MapRenderer.hpp>
#include <basalt/server/world/Chunk.hpp>

#include <doctest.h>

using basalt::server::runtime::world::maps::MapBanner;
using basalt::server::runtime::world::maps::MapData;
namespace types = basalt::server::runtime::world::maps::decorationTypes;

TEST_CASE("a holder is sent only the dirty rectangle, then nothing") {
    MapData data = MapData::createFresh(0, 0, 0, true, false, "minecraft:overworld");
    MapData::Holder& holder = data.holder({}, 1, "Steve");
    auto first = data.nextUpdate(3, holder);
    REQUIRE(first.has_value());
    CHECK(first->patch->width == 128);
    CHECK(first->decorations.has_value());
    CHECK_FALSE(data.nextUpdate(3, holder).has_value());
    CHECK(data.updateColor(5, 6, 0x2A));
    CHECK_FALSE(data.updateColor(5, 6, 0x2A));
    CHECK(data.updateColor(7, 9, 0x10));
    const auto second = data.nextUpdate(3, holder);
    REQUIRE(second.has_value());
    CHECK(second->patch->startX == 5);
    CHECK(second->patch->startY == 6);
    CHECK(second->patch->width == 3);
    CHECK(second->patch->height == 4);
    CHECK(second->patch->colors[0] == 0x2A);
    CHECK(second->patch->colors[2 + (3 * 3)] == 0x10);
}

TEST_CASE("the saved compound comes back as the same map") {
    MapData data = MapData::createFresh(300, 300, 2, true, true, "minecraft:overworld");
    data.setColor(1, 2, 0x55);
    const MapBanner banner{.pos = {310, 64, 290}, .color = "red"};
    CHECK(data.toggleBanner(banner, banner.pos, 0));
    const std::optional<MapData> loaded = MapData::load(data.save());
    REQUIRE(loaded.has_value());
    CHECK(loaded->centerX() == data.centerX());
    CHECK(loaded->scale() == 2);
    CHECK(loaded->unlimitedTracking());
    CHECK(loaded->colors()[1 + (2 * 128)] == 0x55);
    REQUIRE(loaded->decorations().size() == 1);
    CHECK(loaded->decorations()[0].first == "banner-310,64,290");
    CHECK(loaded->decorations()[0].second.type == *types::bannerOf("red"));
    MapData again = *loaded;
    CHECK(again.toggleBanner(banner, banner.pos, 0));
    CHECK(again.decorations().empty());
}
