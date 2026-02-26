#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/DetectorRails.hpp>

#include <doctest.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

using namespace basalt::server::runtime::world;
using basalt::server::runtime::entity::ServerEntity;
using basalt::server::world::Chunk;
using basalt::wire::common::Identifier;
using basalt::wire::common::Uuid;
namespace registry = basalt::server::registry;

namespace {

[[nodiscard]] std::int32_t stateOf(const std::string& path) {
    const registry::BlockState* block = registry::blocks::named(path);
    REQUIRE(block != nullptr);
    return block->stateId;
}

[[nodiscard]] WorldState emptyWorld() {
    return WorldState{{}, [](std::int32_t chunkX, std::int32_t chunkZ) { return Chunk{chunkX, chunkZ}; }};
}

[[nodiscard]] std::string poweredOf(std::int32_t stateId) {
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    const auto found = properties.find("powered");
    return found == properties.end() ? std::string{} : found->second;
}

[[nodiscard]] std::unique_ptr<ServerEntity> cart(const std::string& type, double x, double y, double z) {
    return std::make_unique<ServerEntity>(1, Uuid{1, 1}, Identifier::vanilla(type), 0, x, y, z);
}

} // namespace

TEST_CASE("every kind of minecart presses one, and nothing else does") {
    for (const std::string type : {"minecart", "chest_minecart", "furnace_minecart", "tnt_minecart", "hopper_minecart",
                                   "spawner_minecart", "command_block_minecart"}) {
        CAPTURE(type);
        CHECK(detectorRails::isMinecart(Identifier::vanilla(type)));
    }
    CHECK_FALSE(detectorRails::isMinecart(Identifier::vanilla("boat")));
    CHECK_FALSE(detectorRails::isMinecart(Identifier::vanilla("pig")));
    CHECK_FALSE(detectorRails::isMinecart(Identifier::vanilla("minecart_with_chest")));
}

TEST_CASE("a cart on the rail turns it on and books another look") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(4, 64, 4, stateOf("detector_rail")));

    const std::unique_ptr<ServerEntity> standing = cart("minecart", 4.5, 64.0, 4.5);
    const std::vector<ServerEntity*> entities{standing.get()};

    // parity: `entityInside` is what a cart arriving runs, and it only asks while the rail is off.
    const detectorRails::Checked pressed =
        detectorRails::check(world, 4, 64, 4, entities, detectorRails::Caller::Entity);
    REQUIRE(pressed.change.has_value());
    CHECK(poweredOf(pressed.change->stateId) == "true");
    CHECK(world.blockState(4, 64, 4) == pressed.change->stateId);
    // parity: twenty ticks, and it is the rail's own turn that finds the cart gone later.
    CHECK(world.hasScheduledBlockTick(4, 64, 4, Identifier::vanilla("detector_rail")));

    // parity: with the rail already on, a cart arriving asks nothing at all: `entityInside` is gated
    // on `!POWERED`, and from here on it is the twenty tick turn that keeps looking.
    const detectorRails::Checked again = detectorRails::check(world, 4, 64, 4, entities, detectorRails::Caller::Entity);
    CHECK_FALSE(again.ran);
    CHECK_FALSE(again.change.has_value());

    // The rail's own turn does run, reports nothing, and still counts as a pass: that is what tells
    // the comparators again.
    const detectorRails::Checked held =
        detectorRails::check(world, 4, 64, 4, entities, detectorRails::Caller::ScheduledTick);
    CHECK(held.ran);
    CHECK_FALSE(held.change.has_value());

    const detectorRails::Checked released =
        detectorRails::check(world, 4, 64, 4, {}, detectorRails::Caller::ScheduledTick);
    REQUIRE(released.change.has_value());
    CHECK(poweredOf(released.change->stateId) == "false");
    // parity: and now it is `tick` that is gated off, because the rail is no longer powered.
    CHECK_FALSE(detectorRails::check(world, 4, 64, 4, {}, detectorRails::Caller::ScheduledTick).ran);
}

TEST_CASE("the box a cart has to be in") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(4, 64, 4, stateOf("detector_rail")));

    const auto pressesFrom = [&world](double x, double y, double z) {
        const std::unique_ptr<ServerEntity> at = cart("minecart", x, y, z);
        const std::vector<ServerEntity*> entities{at.get()};
        const detectorRails::Checked checked =
            detectorRails::check(world, 4, 64, 4, entities, detectorRails::Caller::Entity);
        const bool pressed = checked.change.has_value() && poweredOf(checked.change->stateId) == "true";
        if (pressed) {
            // Put it back so each question is asked of the same rail.
            static_cast<void>(world.setBlockState(4, 64, 4, stateOf("detector_rail")));
        }
        return pressed;
    };

    // parity: `getSearchBB` is the rail's square shrunk a fifth of a block on each horizontal side
    // and off the top, and what has to touch it is the **cart's own box** (0.98 wide, 0.7 tall), not
    // its middle. So the reach is x and z in (3.71, 5.29) and y in (63.3, 64.8). Medido contra el
    // jar con una vagoneta lanzada: dispara con el centro en 4.84 para un rail en 5, y yendo por el
    // centro disparaba en 5.12.
    CHECK(pressesFrom(4.5, 64.0, 4.5));
    CHECK(pressesFrom(3.75, 64.0, 4.5));
    CHECK(pressesFrom(5.25, 64.0, 4.5));
    CHECK_FALSE(pressesFrom(3.7, 64.0, 4.5));
    CHECK_FALSE(pressesFrom(5.3, 64.0, 4.5));
    CHECK_FALSE(pressesFrom(4.5, 64.0, 3.7));
    CHECK_FALSE(pressesFrom(4.5, 64.0, 5.3));
    // Un poco por debajo si llega, porque la vagoneta mide setenta centesimas de alto.
    CHECK(pressesFrom(4.5, 63.5, 4.5));
    CHECK_FALSE(pressesFrom(4.5, 63.2, 4.5));
    CHECK_FALSE(pressesFrom(4.5, 64.9, 4.5));
}

TEST_CASE("what does not press a rail") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(4, 64, 4, stateOf("detector_rail")));

    // A pig standing on it is not a cart.
    const std::unique_ptr<ServerEntity> pig = cart("pig", 4.5, 64.0, 4.5);
    CHECK_FALSE(
        detectorRails::check(world, 4, 64, 4, std::vector<ServerEntity*>{pig.get()}, detectorRails::Caller::Entity)
            .change.has_value());

    // A cart that has been removed is not there any more, whatever its coordinates still say.
    const std::unique_ptr<ServerEntity> gone = cart("minecart", 4.5, 64.0, 4.5);
    gone->removed = true;
    CHECK_FALSE(
        detectorRails::check(world, 4, 64, 4, std::vector<ServerEntity*>{gone.get()}, detectorRails::Caller::Entity)
            .change.has_value());

    // A plain rail is not a detector rail, and neither is an empty position.
    REQUIRE(world.setBlockState(5, 64, 5, stateOf("rail")));
    const std::unique_ptr<ServerEntity> onRail = cart("minecart", 5.5, 64.0, 5.5);
    CHECK_FALSE(
        detectorRails::check(world, 5, 64, 5, std::vector<ServerEntity*>{onRail.get()}, detectorRails::Caller::Entity)
            .change.has_value());

    // Nor is a powered rail, which does have a `powered` of its own: a cart rolling over one must
    // not switch it on, that is what the redstone under it decides.
    REQUIRE(world.setBlockState(6, 64, 6, stateOf("powered_rail")));
    const std::unique_ptr<ServerEntity> onPowered = cart("minecart", 6.5, 64.0, 6.5);
    CHECK_FALSE(detectorRails::check(world, 6, 64, 6, std::vector<ServerEntity*>{onPowered.get()},
                                     detectorRails::Caller::Entity)
                    .change.has_value());
    CHECK(poweredOf(*world.blockState(6, 64, 6)) == "false");
    CHECK_FALSE(detectorRails::check(world, 500, 64, 500, {}, detectorRails::Caller::Entity).change.has_value());
}
