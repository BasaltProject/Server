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

TEST_CASE("a fresh map snaps its centre to the grid of its scale") {
    const MapData origin = MapData::createFresh(0, 0, 0, true, false, "minecraft:overworld");
    CHECK(origin.centerX() == 0);
    CHECK(origin.centerZ() == 0);
    // parity: `createFresh`, 256 blocks a side at scale one: floor(164 / 256) and floor(-6 / 256).
    const MapData scaled = MapData::createFresh(100, -70, 1, true, false, "minecraft:overworld");
    CHECK(scaled.centerX() == 64);
    CHECK(scaled.centerZ() == -192);
    CHECK(scaled.scaled().scale() == 2);
}

TEST_CASE("player markers land on the byte grid, leave the edge and point the way vanilla rounds") {
    MapData data = MapData::createFresh(0, 0, 0, true, false, "minecraft:overworld");
    data.addDecoration(types::kPlayer, 0, "Steve", 10.0, 20.0, 90.0, std::nullopt);
    REQUIRE(data.decorations().size() == 1);
    CHECK(data.decorations()[0].second.x == 20);
    CHECK(data.decorations()[0].second.y == 40);
    CHECK(data.decorations()[0].second.rotation == 4);
    data.addDecoration(types::kPlayer, 0, "Steve", 100.0, 0.0, 0.0, std::nullopt);
    CHECK(data.decorations()[0].second.type == types::kPlayerOffMap);
    CHECK(data.decorations()[0].second.x == 127);
    data.addDecoration(types::kPlayer, 0, "Steve", 1000.0, 0.0, 0.0, std::nullopt);
    CHECK(data.decorations().empty());

    // parity: a Nether map's markers spin off the clock: s = 10 gives 10.
    MapData nether = MapData::createFresh(0, 0, 0, true, false, "minecraft:the_nether");
    nether.addDecoration(types::kPlayer, 100, "Alex", 0.0, 0.0, 0.0, std::nullopt);
    CHECK(nether.decorations()[0].second.rotation == 10);
}

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
    // Row by row inside the rectangle.
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
    // Toggling the same banner takes the marker off again.
    MapData again = *loaded;
    CHECK(again.toggleBanner(banner, banner.pos, 0));
    CHECK(again.decorations().empty());
}

TEST_CASE("the colour scan shades land by slope and shallow water bright") {
    namespace colors = basalt::server::registry::mapColors;
    namespace reg = basalt::server::registry;
    basalt::server::world::Chunk chunk{0, 0};
    const std::int32_t stone = reg::blocks::stone().stateId;
    const std::int32_t water = reg::blocks::water().stateId;
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            chunk.setBlockStateId(x, 62, z, stone);
            const bool pond = x >= 4 && x < 8 && z >= 4 && z < 8;
            chunk.setBlockStateId(x, 63, z, pond ? water : stone);
        }
    }
    MapData data = MapData::createFresh(0, 0, 0, true, false, "minecraft:overworld");
    MapData::Holder& holder = data.holder({}, 1, "Steve");
    const basalt::server::runtime::world::maps::ScanWorld scan{
        .chunkAt = [&chunk](std::int32_t x, std::int32_t z) -> const basalt::server::world::Chunk* {
            return x == 0 && z == 0 ? &chunk : nullptr;
        },
        .bannerAt = [](const basalt::server::runtime::world::maps::BlockPos&) { return std::optional<MapBanner>{}; },
        .minY = -64,
        .hasCeiling = false};
    // One column in sixteen each step, so sixteen steps draw the whole chunk.
    for (std::int32_t step = 0; step < 16; ++step) {
        basalt::server::runtime::world::maps::mapRenderer::update(data, holder, scan, 8.0, 8.0);
    }
    const auto at = [&data](std::int32_t x, std::int32_t z) {
        return data.colors()[static_cast<std::size_t>((64 + x) + ((64 + z) * 128))];
    };
    // parity: the first row after an unloaded chunk climbs from a height of zero, so it is bright.
    CHECK(at(0, 0) == colors::packed(colors::kStone, colors::Brightness::High));
    CHECK(at(0, 1) == colors::packed(colors::kStone, colors::Brightness::Normal));
    CHECK(at(10, 10) == colors::packed(colors::kStone, colors::Brightness::Normal));
    // parity: one block of water is shallow on either parity of the dither.
    CHECK(at(4, 4) == colors::packed(colors::kWater, colors::Brightness::High));
    CHECK(at(5, 4) == colors::packed(colors::kWater, colors::Brightness::High));
    CHECK(at(0, -1) == 0);
}
