#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/NetherPortals.hpp>

#include <doctest.h>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

using basalt::server::runtime::world::LoadedWorld;
using basalt::server::runtime::world::PortalExit;
using basalt::server::runtime::world::WorldMetadata;
using basalt::server::world::portal::PortalAxis;
using basalt::server::world::portal::PortalShape;
using basalt::wire::common::Identifier;
namespace blocks = basalt::server::registry::blocks;
namespace config = basalt::server::runtime::config;
namespace portals = basalt::server::runtime::world::netherPortals;

namespace {

[[nodiscard]] std::string scratch(std::string_view name) {
    const std::filesystem::path directory = std::filesystem::path{BASALT_TEST_SCRATCH_DIR} / name;
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return directory.string();
}

[[nodiscard]] std::unique_ptr<LoadedWorld> worldIn(const std::string& root, config::WorldDimension dimension) {
    auto world = std::make_unique<LoadedWorld>(
        config::WorldDeclaration{
            .name = "world", .dimension = dimension, .type = config::WorldType::Flat, .isDefault = true},
        config::WorldSettings{}, WorldMetadata{}, root, basalt::wire::common::Logger{"Test"});
    for (std::int32_t chunkX = -1; chunkX <= 1; chunkX++) {
        for (std::int32_t chunkZ = -1; chunkZ <= 1; chunkZ++) {
            world->state().getOrGenerateChunk(chunkX, chunkZ);
        }
    }
    return world;
}

/// A four-by-five obsidian frame around a two-by-three opening, standing on the X axis.
void buildFrame(LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::int32_t obsidian = blocks::named("obsidian")->stateId;
    for (std::int32_t column = -1; column < 3; column++) {
        for (std::int32_t row = -1; row < 4; row++) {
            if (column == -1 || column == 2 || row == -1 || row == 3) {
                REQUIRE(world.state().setBlockState(x - column, y + row, z, obsidian));
            }
        }
    }
}

} // namespace

TEST_CASE("lighting a complete frame fills it, and an incomplete one is left alone") {
    std::unique_ptr<LoadedWorld> world = worldIn(scratch("portal-light"), config::WorldDimension::Overworld);
    buildFrame(*world, 4, 64, 4);

    const std::optional<PortalShape> lit = portals::light(*world, 4, 64, 4);
    REQUIRE(lit.has_value());
    // The two by three opening is now portal, and the frame around it is still obsidian.
    CHECK(PortalShape::isPortal(*world->state().blockState(4, 64, 4)));
    CHECK(PortalShape::isPortal(*world->state().blockState(3, 66, 4)));
    CHECK_FALSE(PortalShape::isPortal(*world->state().blockState(5, 64, 4)));

    // A frame with a corner missing is not one, and lighting it answers nothing.
    std::unique_ptr<LoadedWorld> broken = worldIn(scratch("portal-broken"), config::WorldDimension::Overworld);
    buildFrame(*broken, 4, 64, 4);
    REQUIRE(broken->state().setBlockState(4, 63, 4, blocks::air().stateId));
    CHECK_FALSE(portals::light(*broken, 4, 64, 4).has_value());
}

TEST_CASE("breaking the frame puts the whole portal out at once") {
    std::unique_ptr<LoadedWorld> world = worldIn(scratch("portal-extinguish"), config::WorldDimension::Overworld);
    buildFrame(*world, 4, 64, 4);
    REQUIRE(portals::light(*world, 4, 64, 4).has_value());

    REQUIRE(world->state().setBlockState(4, 63, 4, blocks::air().stateId));
    const std::vector<std::array<std::int32_t, 3>> cleared = portals::extinguishAround(*world, 4, 63, 4);
    // All six blocks of the opening go, not just the one next to the hole.
    CHECK(cleared.size() == 6);
    CHECK_FALSE(PortalShape::isPortal(*world->state().blockState(3, 66, 4)));
}

TEST_CASE("the index finds a lit portal, and agrees with a walk of the loaded chunks") {
    std::unique_ptr<LoadedWorld> world = worldIn(scratch("portal-index"), config::WorldDimension::Overworld);
    buildFrame(*world, 4, 64, 4);
    REQUIRE(portals::light(*world, 4, 64, 4).has_value());

    const std::optional<std::array<std::int32_t, 3>> indexed = portals::findClosestPortal(*world, 4, 64, 4, 32);
    const std::optional<std::array<std::int32_t, 3>> walked = portals::findClosestLoadedPortal(*world, 4, 64, 4, 32);
    REQUIRE(indexed.has_value());
    REQUIRE(walked.has_value());
    CHECK(*indexed == *walked);

    // Nothing within a tight radius of somewhere far away.
    CHECK_FALSE(portals::findClosestPortal(*world, 500, 64, 500, 16).has_value());
}

TEST_CASE("an exit scales horizontally by the dimension ratio and builds a portal where there is none") {
    std::unique_ptr<LoadedWorld> overworld = worldIn(scratch("portal-from"), config::WorldDimension::Overworld);
    std::unique_ptr<LoadedWorld> nether = worldIn(scratch("portal-to"), config::WorldDimension::Nether);

    // The Nether floor of a flat world is at its own minimum, so the traveller enters low.
    const std::optional<PortalExit> exit = portals::exitFor(*overworld, *nether, 80.0, 80.0, 80.0, PortalAxis::X);
    REQUIRE(exit.has_value());
    CHECK(exit->world == nether.get());
    // parity: the 1:8, so eighty blocks out in the overworld is ten blocks out in the Nether. The
    // search radius is sixteen, so the built portal lands within that of the scaled position.
    CHECK(std::abs(exit->x - 10.5) <= 32.0);
    CHECK(std::abs(exit->z - 10.5) <= 32.0);

    // A second traveller through the same place finds the portal that is already there.
    const std::optional<PortalExit> again = portals::exitFor(*overworld, *nether, 80.0, 80.0, 80.0, PortalAxis::X);
    REQUIRE(again.has_value());
    CHECK(std::abs(again->x - exit->x) <= 32.0);
}
