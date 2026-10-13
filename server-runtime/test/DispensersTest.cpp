#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/entity/Projectiles.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/world/BlockInventory.hpp>
#include <basalt/server/runtime/world/Dispensers.hpp>
#include <basalt/server/runtime/world/Fires.hpp>

#include <doctest.h>

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <variant>
#include <vector>

using namespace basalt::server::runtime::world;
using basalt::server::runtime::entity::ServerEntity;
using basalt::server::world::Chunk;
using basalt::server::world::loot::ItemStack;
namespace registry = basalt::server::registry;
namespace nbt = basalt::wire::nbt;
namespace rng = basalt::server::random;

namespace {

[[nodiscard]] basalt::wire::common::Identifier item(const std::string& path) {
    return basalt::wire::common::Identifier::vanilla(path);
}

[[nodiscard]] std::int32_t stateOf(const std::string& path) {
    const registry::BlockState* block = registry::blocks::named(path);
    REQUIRE(block != nullptr);
    return block->stateId;
}

[[nodiscard]] std::int32_t dispenser(const std::string& facing, const std::string& triggered,
                                     const std::string& path = "dispenser") {
    const std::optional<std::int32_t> stateId = registry::blockStateDefinitions::stateIdOf(
        item(path), {{"facing", facing}, {"triggered", triggered}}, std::nullopt);
    REQUIRE(stateId.has_value());
    return *stateId;
}

struct Fixture {
    WorldState world = WorldState{{}, [](std::int32_t chunkX, std::int32_t chunkZ) { return Chunk{chunkX, chunkZ}; }};
    GameRuleValues gameRules;
    rng::RandomSource random = rng::RandomSource::legacy(1);

    Fixture() { world.getOrGenerateChunk(0, 0); }

    [[nodiscard]] fluidFlow::Context context() {
        return fluidFlow::Context{.world = world, .gameRules = gameRules, .random = random, .ultrawarm = false};
    }

    /// A stone floor at y = 63, which is what fire needs under it.
    void floor() {
        for (std::int32_t x = 0; x < 16; ++x) {
            for (std::int32_t z = 0; z < 16; ++z) {
                REQUIRE(world.setBlockState(x, 63, z, stateOf("stone")));
            }
        }
    }
};

[[nodiscard]] nbt::NbtCompound& dataAt(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    Chunk* chunk = world.chunkIfLoaded(x >> 4, z >> 4);
    REQUIRE(chunk != nullptr);
    basalt::server::world::ChunkBlockEntity* entity = chunk->blockEntityAt(x & 15, y, z & 15);
    REQUIRE(entity != nullptr);
    return entity->data();
}

/// A block entity holding those slots, which is where a container keeps its contents.
void fill(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, const blockInventory::Slots& slots,
          const std::string& path = "dispenser") {
    nbt::NbtCompound data;
    blockInventory::write(data, slots);
    world.chunkIfLoaded(x >> 4, z >> 4)
        ->setBlockEntity(basalt::server::world::ChunkBlockEntity{x & 15, y, z & 15, item(path), std::move(data)});
}

/// A dispenser at 8/64/8 with one slot filled, which makes the pick deterministic.
void oneItem(WorldState& world, const std::string& facing, const ItemStack& stack, std::size_t slot = 0,
             const std::string& path = "dispenser") {
    REQUIRE(world.setBlockState(8, 64, 8, dispenser(facing, "true", path)));
    blockInventory::Slots slots(dispensers::kSize);
    slots[slot] = stack;
    fill(world, 8, 64, 8, slots, path);
}

[[nodiscard]] dispensers::Hooks ids(std::int32_t first = 7) {
    return dispensers::Hooks{.allocateEntityId = [first] { return first; }};
}

[[nodiscard]] blockInventory::Slots slotsAt(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                            std::int32_t size) {
    return blockInventory::read(dataAt(world, x, y, z), size);
}

} // namespace

TEST_CASE("a rising signal books a turn and a falling one forgets it") {
    Fixture fixture;
    WorldState& world = fixture.world;
    REQUIRE(world.setBlockState(8, 64, 8, dispenser("north", "false")));
    REQUIRE(world.setBlockState(9, 64, 8, stateOf("redstone_block")));

    const std::optional<BlockChange> risen = dispensers::onNeighbourChange(world, "overworld", 8, 64, 8);
    REQUIRE(risen.has_value());
    CHECK(risen->stateId == dispenser("north", "true"));
    CHECK(risen->x == 8);
    CHECK(risen->y == 64);
    CHECK(risen->z == 8);
    CHECK(world.hasScheduledBlockTick(8, 64, 8, dispensers::tickType()));

    // Asked again with the signal still up, nothing changes.
    CHECK_FALSE(dispensers::onNeighbourChange(world, "overworld", 8, 64, 8).has_value());

    REQUIRE(world.setBlockState(9, 64, 8, registry::blocks::air().stateId));
    const std::optional<BlockChange> fallen = dispensers::onNeighbourChange(world, "overworld", 8, 64, 8);
    REQUIRE(fallen.has_value());
    CHECK(fallen->stateId == dispenser("north", "false"));
}

TEST_CASE("a falling signal books nothing") {
    Fixture fixture;
    WorldState& world = fixture.world;
    REQUIRE(world.setBlockState(8, 64, 8, dispenser("north", "true")));

    REQUIRE(dispensers::onNeighbourChange(world, "overworld", 8, 64, 8).has_value());

    CHECK_FALSE(world.hasScheduledBlockTick(8, 64, 8, dispensers::tickType()));
}

TEST_CASE("the block above is watched as well as the block itself") {
    Fixture fixture;
    WorldState& world = fixture.world;
    REQUIRE(world.setBlockState(8, 64, 8, dispenser("north", "false")));
    // Beside the block above, and not beside the dispenser itself.
    REQUIRE(world.setBlockState(9, 65, 8, stateOf("redstone_block")));

    const std::optional<BlockChange> risen = dispensers::onNeighbourChange(world, "overworld", 8, 64, 8);

    REQUIRE(risen.has_value());
    CHECK(risen->stateId == dispenser("north", "true"));
}

TEST_CASE("a dropper triggers like a dispenser and anything else is left alone") {
    Fixture fixture;
    WorldState& world = fixture.world;
    REQUIRE(world.setBlockState(8, 64, 8, dispenser("north", "false", "dropper")));
    REQUIRE(world.setBlockState(9, 64, 8, stateOf("redstone_block")));
    REQUIRE(world.setBlockState(4, 64, 4, stateOf("stone")));
    REQUIRE(world.setBlockState(5, 64, 4, stateOf("redstone_block")));

    const std::optional<BlockChange> risen = dispensers::onNeighbourChange(world, "overworld", 8, 64, 8);

    REQUIRE(risen.has_value());
    CHECK(risen->stateId == dispenser("north", "true", "dropper"));
    CHECK_FALSE(dispensers::onNeighbourChange(world, "overworld", 4, 64, 4).has_value());
    // Nothing at all is one more thing that is not a dispenser.
    CHECK_FALSE(dispensers::onNeighbourChange(world, "overworld", 8, 200, 8).has_value());
}

TEST_CASE("the turn is booked four ticks out") {
    Fixture fixture;
    WorldState& world = fixture.world;
    REQUIRE(world.setBlockState(8, 64, 8, dispenser("north", "false")));
    REQUIRE(world.setBlockState(9, 64, 8, stateOf("redstone_block")));
    REQUIRE(dispensers::onNeighbourChange(world, "overworld", 8, 64, 8).has_value());

    for (std::int32_t turn = 0; turn < 3; ++turn) {
        world.openTurn();
        CHECK(world.dueBlockTicks().empty());
    }
    world.openTurn();
    CHECK(world.dueBlockTicks().size() == 1);
}

TEST_CASE("a dispenser away from the first chunk works off its own local coordinates") {
    Fixture fixture;
    WorldState& world = fixture.world;
    world.getOrGenerateChunk(1, 1);
    REQUIRE(world.setBlockState(21, 64, 19, dispenser("east", "true")));
    blockInventory::Slots slots(dispensers::kSize);
    slots[0] = ItemStack{.item = item("stone"), .count = 1};
    fill(world, 21, 64, 19, slots);
    world.chunkIfLoaded(0, 0)->markClean();
    world.chunkIfLoaded(1, 1)->markClean();

    CHECK(dispensers::fire(fixture.context(), 21, 64, 19, ids()).fired);

    CHECK(world.chunkIfLoaded(1, 1)->isDirty());
    CHECK_FALSE(world.chunkIfLoaded(0, 0)->isDirty());

    const std::vector<DroppedItem> thrown = world.items();
    REQUIRE(thrown.size() == 1);
    CHECK(thrown[0].x == 22.2);
    CHECK(thrown[0].y == 64.34375);
    CHECK(thrown[0].z == 19.5);
    CHECK_FALSE(slotsAt(world, 21, 64, 19, dispensers::kSize)[0].has_value());
}

TEST_CASE("a dispenser lights the tnt it holds instead of throwing it") {
    // parity: `DispenseItemBehavior`'s tnt entry builds the primed entity in the square in front and
    // never throws the block. With the `tnt_explodes` rule off it does neither.
    Fixture fixture;
    WorldState& world = fixture.world;
    oneItem(world, "north", ItemStack{.item = item("tnt"), .count = 2});

    const dispensers::Result result = dispensers::fire(fixture.context(), 8, 64, 8, ids());

    CHECK(result.fired);
    CHECK(world.items().empty());
    const std::vector<PrimedTntQueue::Primed> lit = world.takePrimedTnt();
    REQUIRE(lit.size() == 1);
    CHECK(lit[0].x == 8);
    CHECK(lit[0].y == 64);
    CHECK(lit[0].z == 7);
    // parity: the dispenser makes the entity itself, so what a sculk sensor hears is `entity_place`
    // and not the `prime_fuse` of a block being lit.
    CHECK(lit[0].event == "entity_place");
    CHECK(slotsAt(world, 8, 64, 8, dispensers::kSize)[0]->count == 1);
}

TEST_CASE("a dispenser finishes a golem with the pumpkin it holds, or keeps it") {
    // parity: the carved pumpkin entry is an `OptionalDispenseItemBehavior`: it places the pumpkin
    // only when the square in front is empty and `canSpawnGolem` says a figure would stand up, and a
    // refusal keeps the pumpkin in the slot instead of throwing it.
    Fixture builds;
    WorldState& world = builds.world;
    oneItem(world, "north", ItemStack{.item = item("carved_pumpkin"), .count = 1});
    // The dispenser looks north, so the pumpkin lands at z minus one, over two snow blocks.
    REQUIRE(world.setBlockState(8, 63, 7, stateOf("snow_block")));
    REQUIRE(world.setBlockState(8, 62, 7, stateOf("snow_block")));

    const dispensers::Result built = dispensers::fire(builds.context(), 8, 64, 8, ids());

    CHECK(built.fired);
    CHECK(world.items().empty());
    CHECK(world.blockState(8, 64, 7) == stateOf("carved_pumpkin"));
    CHECK(slotsAt(world, 8, 64, 8, dispensers::kSize)[0] == std::nullopt);

    // With nothing under it there is no figure to finish, so the pumpkin stays where it is.
    Fixture keeps;
    WorldState& bare = keeps.world;
    oneItem(bare, "north", ItemStack{.item = item("carved_pumpkin"), .count = 1});

    const dispensers::Result refused = dispensers::fire(keeps.context(), 8, 64, 8, ids());

    CHECK_FALSE(refused.fired);
    CHECK(bare.items().empty());
    CHECK(bare.blockState(8, 64, 7) == registry::blocks::air().stateId);
    CHECK(slotsAt(bare, 8, 64, 8, dispensers::kSize)[0]->count == 1);
}

TEST_CASE("a minecart lands on the rail in front, or on the one a step below") {
    // parity: `MinecartDispenseItemBehavior`, whose four heights are the rail in front (flat or a
    // slope) and the rail below an empty square (again flat or a slope).
    const auto dispensedOnto = [](const std::string& shape, std::int32_t railY) {
        Fixture fixture;
        WorldState& world = fixture.world;
        oneItem(world, "north", ItemStack{.item = item("minecart"), .count = 1});
        const std::optional<std::int32_t> rail = registry::blockStateDefinitions::stateIdOf(
            item("rail"), {{"shape", shape}, {"waterlogged", "false"}}, std::nullopt);
        REQUIRE(rail.has_value());
        REQUIRE(world.setBlockState(8, railY, 7, *rail));
        static_cast<void>(dispensers::fire(fixture.context(), 8, 64, 8, ids()));
        const std::vector<ServerEntity*> spawned = world.entities();
        return spawned.empty() ? std::optional<double>{} : std::optional{spawned.front()->y};
    };

    // On the rail in front: a tenth above the block, and six tenths on a slope.
    CHECK(dispensedOnto("north_south", 64) == doctest::Approx(64.1));
    CHECK(dispensedOnto("ascending_north", 64) == doctest::Approx(64.6));
    // On the rail below an empty square: nine tenths down, and four tenths on a slope.
    CHECK(dispensedOnto("north_south", 63) == doctest::Approx(63.1));
    CHECK(dispensedOnto("ascending_north", 63) == doctest::Approx(63.6));
}

TEST_CASE("a minecart placed on a rail plays the dispense sound and the smoke") {
    Fixture fixture;
    WorldState& world = fixture.world;
    oneItem(world, "north", ItemStack{.item = item("minecart"), .count = 1});
    const std::optional<std::int32_t> rail = registry::blockStateDefinitions::stateIdOf(
        item("rail"), {{"shape", "north_south"}, {"waterlogged", "false"}}, std::nullopt);
    REQUIRE(rail.has_value());
    REQUIRE(world.setBlockState(8, 64, 7, *rail));

    const dispensers::Result result = dispensers::fire(fixture.context(), 8, 64, 8, ids());

    CHECK(result.fired);
    REQUIRE(result.effects.size() == 2);
    CHECK(result.effects[0] ==
          BlockEffect{blockEffect::Effect{.type = blockEffect::Effect::kDispenserDispense, .data = 0}});
    CHECK(result.effects[1] ==
          BlockEffect{blockEffect::Effect{.type = blockEffect::Effect::kDispenserSmoke, .data = 2}});
}

TEST_CASE("an empty dispenser clicks and nothing goes out") {
    Fixture fixture;
    WorldState& world = fixture.world;
    REQUIRE(world.setBlockState(8, 64, 8, dispenser("north", "true")));
    fill(world, 8, 64, 8, blockInventory::Slots(dispensers::kSize));

    const dispensers::Result result = dispensers::fire(fixture.context(), 8, 64, 8, ids());

    CHECK_FALSE(result.fired);
    REQUIRE(result.effects.size() == 1);
    CHECK(result.effects[0] ==
          BlockEffect{blockEffect::Effect{.type = blockEffect::Effect::kDispenserFail, .data = 0}});
    CHECK(world.items().empty());
}

TEST_CASE("a thrown item plays the dispense sound and the smoke out of the facing") {
    Fixture fixture;
    WorldState& world = fixture.world;
    oneItem(world, "north", ItemStack{.item = item("stone"), .count = 2});

    const dispensers::Result result = dispensers::fire(fixture.context(), 8, 64, 8, ids());

    CHECK(result.fired);
    REQUIRE(result.effects.size() == 2);
    CHECK(result.effects[0] ==
          BlockEffect{blockEffect::Effect{.type = blockEffect::Effect::kDispenserDispense, .data = 0}});
    CHECK(result.effects[1] ==
          BlockEffect{blockEffect::Effect{.type = blockEffect::Effect::kDispenserSmoke, .data = 2}});
}

TEST_CASE("a dropper throwing an item plays the same sound and smoke as a dispenser") {
    Fixture fixture;
    WorldState& world = fixture.world;
    oneItem(world, "up", ItemStack{.item = item("stone"), .count = 2}, 0, "dropper");

    const dispensers::Result result = dispensers::fire(fixture.context(), 8, 64, 8, ids());

    CHECK(result.fired);
    REQUIRE(result.effects.size() == 2);
    CHECK(result.effects[0] ==
          BlockEffect{blockEffect::Effect{.type = blockEffect::Effect::kDispenserDispense, .data = 0}});
    CHECK(result.effects[1] ==
          BlockEffect{blockEffect::Effect{.type = blockEffect::Effect::kDispenserSmoke, .data = 1}});
}

TEST_CASE("a shot plays the launch sound of its item and the smoke") {
    const auto effectsOf = [](const std::string& path) {
        Fixture fixture;
        oneItem(fixture.world, "north", ItemStack{.item = item(path), .count = 2});
        return dispensers::fire(fixture.context(), 8, 64, 8, ids()).effects;
    };

    const std::vector<BlockEffect> arrow = effectsOf("arrow");
    REQUIRE(arrow.size() == 2);
    CHECK(arrow[0] == BlockEffect{blockEffect::Effect{.type = blockEffect::Effect::kDispenserShoot, .data = 0}});
    CHECK(arrow[1] == BlockEffect{blockEffect::Effect{.type = blockEffect::Effect::kDispenserSmoke, .data = 2}});

    const std::vector<BlockEffect> fireCharge = effectsOf("fire_charge");
    REQUIRE(fireCharge.size() == 2);
    CHECK(fireCharge[0] == BlockEffect{blockEffect::Effect{.type = blockEffect::Effect::kFireChargeShoot, .data = 0}});

    const std::vector<BlockEffect> windCharge = effectsOf("wind_charge");
    REQUIRE(windCharge.size() == 2);
    CHECK(windCharge[0] == BlockEffect{blockEffect::Effect{.type = blockEffect::Effect::kWindChargeShoot, .data = 0}});
}

TEST_CASE("a boat with no water plays the dispense pair twice") {
    Fixture fixture;
    WorldState& world = fixture.world;
    oneItem(world, "north", ItemStack{.item = item("oak_boat"), .count = 1});

    const dispensers::Result result = dispensers::fire(fixture.context(), 8, 64, 8, ids());

    CHECK(result.fired);
    REQUIRE(result.effects.size() == 4);
    for (std::size_t i = 0; i < 2; ++i) {
        CHECK(result.effects[i * 2] ==
              BlockEffect{blockEffect::Effect{.type = blockEffect::Effect::kDispenserDispense, .data = 0}});
        CHECK(result.effects[(i * 2) + 1] ==
              BlockEffect{blockEffect::Effect{.type = blockEffect::Effect::kDispenserSmoke, .data = 2}});
    }
    CHECK(world.items().size() == 1);
}

TEST_CASE("a dispenser with no block entity does nothing at all") {
    Fixture fixture;
    WorldState& world = fixture.world;
    REQUIRE(world.setBlockState(8, 64, 8, dispenser("north", "true")));

    const dispensers::Result result = dispensers::fire(fixture.context(), 8, 64, 8, ids());

    CHECK_FALSE(result.fired);
    CHECK(result.effects.empty());
}

TEST_CASE("the item goes into the container in front, one at a time") {
    Fixture fixture;
    WorldState& world = fixture.world;
    oneItem(world, "north", ItemStack{.item = item("stone"), .count = 5});
    REQUIRE(world.setBlockState(8, 64, 7, stateOf("chest")));
    fill(world, 8, 64, 7, blockInventory::Slots(27), "chest");

    const dispensers::Result result = dispensers::fire(fixture.context(), 8, 64, 8, ids());

    CHECK(result.fired);
    CHECK(result.effects.empty());
    const blockInventory::Slots inChest = slotsAt(world, 8, 64, 7, 27);
    REQUIRE(inChest[0].has_value());
    CHECK(inChest[0]->item == item("stone"));
    CHECK(inChest[0]->count == 1);
    CHECK(slotsAt(world, 8, 64, 8, dispensers::kSize)[0]->count == 4);
    CHECK(world.items().empty());
}

TEST_CASE("the item stacks onto a part-full slot and skips a full one") {
    Fixture fixture;
    WorldState& world = fixture.world;
    oneItem(world, "north", ItemStack{.item = item("stone"), .count = 1});
    REQUIRE(world.setBlockState(8, 64, 7, stateOf("chest")));
    blockInventory::Slots inChest(27);
    inChest[0] = ItemStack{.item = item("stone"), .count = 64};
    inChest[1] = ItemStack{.item = item("dirt"), .count = 1};
    // One short of a full stack, so both sides of the sixty-four are checked.
    inChest[2] = ItemStack{.item = item("stone"), .count = 63};
    fill(world, 8, 64, 7, inChest, "chest");

    CHECK(dispensers::fire(fixture.context(), 8, 64, 8, ids()).fired);

    const blockInventory::Slots after = slotsAt(world, 8, 64, 7, 27);
    CHECK(after[0]->count == 64);
    CHECK(after[1]->item == item("dirt"));
    CHECK(after[1]->count == 1);
    CHECK(after[2]->count == 64);
    CHECK_FALSE(after[3].has_value());
    // The last of the stack leaves the slot empty.
    CHECK_FALSE(slotsAt(world, 8, 64, 8, dispensers::kSize)[0].has_value());
}

TEST_CASE("a container in front with no room takes nothing and the slot keeps its item") {
    Fixture fixture;
    WorldState& world = fixture.world;
    oneItem(world, "north", ItemStack{.item = item("stone"), .count = 5});
    REQUIRE(world.setBlockState(8, 64, 7, stateOf("chest")));
    blockInventory::Slots inChest(27);
    for (std::optional<ItemStack>& slot : inChest) {
        slot = ItemStack{.item = item("dirt"), .count = 64};
    }
    fill(world, 8, 64, 7, inChest, "chest");

    const dispensers::Result result = dispensers::fire(fixture.context(), 8, 64, 8, ids());

    CHECK_FALSE(result.fired);
    CHECK(slotsAt(world, 8, 64, 8, dispensers::kSize)[0]->count == 5);
    CHECK(world.items().empty());
}

TEST_CASE("with nothing in front the item is thrown out of the face, below the middle") {
    const auto thrownAt = [](const std::string& facing) {
        Fixture fixture;
        oneItem(fixture.world, facing, ItemStack{.item = item("stone"), .count = 1});
        REQUIRE(dispensers::fire(fixture.context(), 8, 64, 8, ids()).fired);
        const std::vector<DroppedItem> thrown = fixture.world.items();
        REQUIRE(thrown.size() == 1);
        CHECK(thrown[0].item == item("stone"));
        CHECK(thrown[0].count == 1);
        CHECK(thrown[0].entityId == 7);
        return std::array<double, 3>{thrown[0].x, thrown[0].y, thrown[0].z};
    };

    // parity: 0.7 out of the face, then 0.15625 down sideways and only 0.125 up or down.
    CHECK(thrownAt("north") == std::array<double, 3>{8.5, 64.34375, 7.8});
    CHECK(thrownAt("south") == std::array<double, 3>{8.5, 64.34375, 9.2});
    CHECK(thrownAt("west") == std::array<double, 3>{7.8, 64.34375, 8.5});
    CHECK(thrownAt("east") == std::array<double, 3>{9.2, 64.34375, 8.5});
    CHECK(thrownAt("up") == std::array<double, 3>{8.5, 65.075, 8.5});
    CHECK(thrownAt("down") == std::array<double, 3>{8.5, 63.675, 8.5});
}

TEST_CASE("what is thrown leaves with vanilla's own push and spread") {
    Fixture fixture;
    oneItem(fixture.world, "east", ItemStack{.item = item("stone"), .count = 1});

    // The same rolls in the same order: the slot pick, the push, then one triangle per axis.
    rng::RandomSource oracle = rng::RandomSource::legacy(1);
    CHECK(oracle.nextInt(1) == 0);
    const double push = (oracle.nextDouble() * 0.1) + 0.2;
    const double spread = 0.0172275 * 6.0;
    const double expectedX = oracle.triangle(push, spread);
    const double expectedY = oracle.triangle(0.2, spread);
    const double expectedZ = oracle.triangle(0.0, spread);

    REQUIRE(dispensers::fire(fixture.context(), 8, 64, 8, ids()).fired);

    const std::vector<DroppedItem> thrown = fixture.world.items();
    REQUIRE(thrown.size() == 1);
    CHECK(thrown[0].velocityX == expectedX);
    CHECK(thrown[0].velocityY == expectedY);
    CHECK(thrown[0].velocityZ == expectedZ);
    // The push is a real shove along the face and the spread is small enough not to reverse it.
    CHECK(thrown[0].velocityX > 0.0);
}

TEST_CASE("the slot is picked evenly among the ones with something in") {
    constexpr std::int32_t kTurns = 20;
    // The pick is slot zero unless the second roll, a one in two, moves it to slot eight.
    rng::RandomSource oracle = rng::RandomSource::legacy(1);
    std::int32_t expectedFromEight = 0;
    for (std::int32_t turn = 0; turn < kTurns; ++turn) {
        CHECK(oracle.nextInt(1) == 0);
        if (oracle.nextInt(2) == 0) {
            ++expectedFromEight;
        }
    }
    CHECK(expectedFromEight > 0);
    CHECK(expectedFromEight < kTurns);

    Fixture fixture;
    WorldState& world = fixture.world;
    REQUIRE(world.setBlockState(8, 64, 8, dispenser("north", "true")));
    blockInventory::Slots slots(dispensers::kSize);
    slots[0] = ItemStack{.item = item("stone"), .count = 30};
    slots[8] = ItemStack{.item = item("dirt"), .count = 30};
    fill(world, 8, 64, 8, slots);
    // A chest in front, so the only rolls of the turn are the pick itself.
    REQUIRE(world.setBlockState(8, 64, 7, stateOf("chest")));
    fill(world, 8, 64, 7, blockInventory::Slots(27), "chest");

    for (std::int32_t turn = 0; turn < kTurns; ++turn) {
        REQUIRE(dispensers::fire(fixture.context(), 8, 64, 8, ids()).fired);
    }

    const blockInventory::Slots after = slotsAt(world, 8, 64, 8, dispensers::kSize);
    CHECK(after[8]->count == 30 - expectedFromEight);
    CHECK(after[0]->count == 30 - (kTurns - expectedFromEight));
}

TEST_CASE("an arrow is shot out of the face rather than thrown") {
    Fixture fixture;
    WorldState& world = fixture.world;
    oneItem(world, "east", ItemStack{.item = item("arrow"), .count = 2});

    CHECK(dispensers::fire(fixture.context(), 8, 64, 8, ids(42)).fired);

    const std::vector<ServerEntity*> shot = world.entities();
    REQUIRE(shot.size() == 1);
    CHECK(shot[0]->type() == item("arrow"));
    CHECK(shot[0]->entityId() == 42);
    const registry::EntityTypeEntry* arrow = registry::entityTypes::of(item("arrow"));
    REQUIRE(arrow != nullptr);
    CHECK(shot[0]->protocolType() == arrow->protocolId);
    // parity: 0.7 out of the face, and 0.1 up so it clears the block below.
    CHECK(shot[0]->x == doctest::Approx(9.2));
    CHECK(shot[0]->y == doctest::Approx(64.6));
    CHECK(shot[0]->z == doctest::Approx(8.5));
    CHECK(shot[0]->velocityX > 0.0);
    CHECK(world.items().empty());
    CHECK(slotsAt(world, 8, 64, 8, dispensers::kSize)[0]->count == 1);
}

TEST_CASE("a tipped arrow is shot as a plain one and a stone is not shot at all") {
    Fixture fixture;
    WorldState& world = fixture.world;
    oneItem(world, "up", ItemStack{.item = item("tipped_arrow"), .count = 1});

    CHECK(dispensers::fire(fixture.context(), 8, 64, 8, ids()).fired);

    REQUIRE(world.entities().size() == 1);
    CHECK(world.entities()[0]->type() == item("arrow"));
    CHECK(world.entities()[0]->y == doctest::Approx(65.3));
}

TEST_CASE("a dropper throws what a dispenser would shoot") {
    Fixture fixture;
    WorldState& world = fixture.world;
    // parity: `DropperBlock.getDispenseMethod` answers the default behaviour whatever the item is.
    oneItem(world, "east", ItemStack{.item = item("arrow"), .count = 1}, 0, "dropper");

    CHECK(dispensers::fire(fixture.context(), 8, 64, 8, ids()).fired);

    CHECK(world.entities().empty());
    REQUIRE(world.items().size() == 1);
    CHECK(world.items()[0].item == item("arrow"));
}

TEST_CASE("a projectile with no entity type is not spawned but still leaves the slot") {
    Fixture fixture;
    WorldState& world = fixture.world;
    oneItem(world, "east", ItemStack{.item = item("arrow"), .count = 1});
    dispensers::Hooks hooks = ids();
    hooks.protocolTypeOf = [](const basalt::wire::common::Identifier&) { return std::nullopt; };

    CHECK(dispensers::fire(fixture.context(), 8, 64, 8, hooks).fired);

    CHECK(world.entities().empty());
    CHECK(world.items().empty());
    CHECK_FALSE(slotsAt(world, 8, 64, 8, dispensers::kSize)[0].has_value());
}

TEST_CASE("a bucket of water pours in front and the slot keeps the empty bucket") {
    Fixture fixture;
    WorldState& world = fixture.world;
    fixture.floor();
    oneItem(world, "up", ItemStack{.item = item("water_bucket"), .count = 1});

    CHECK(dispensers::fire(fixture.context(), 8, 64, 8, ids()).fired);

    CHECK(world.blockState(8, 65, 8) == baseStateId(FluidKind::Water));
    const blockInventory::Slots after = slotsAt(world, 8, 64, 8, dispensers::kSize);
    REQUIRE(after[0].has_value());
    CHECK(after[0]->item == item("bucket"));
    CHECK(after[0]->count == 1);
    CHECK(world.items().empty());
}

TEST_CASE("a bucket in a stack of two only loses one and the empty one is not kept") {
    Fixture fixture;
    WorldState& world = fixture.world;
    fixture.floor();
    oneItem(world, "up", ItemStack{.item = item("water_bucket"), .count = 2});

    CHECK(dispensers::fire(fixture.context(), 8, 64, 8, ids()).fired);

    const blockInventory::Slots after = slotsAt(world, 8, 64, 8, dispensers::kSize);
    REQUIRE(after[0].has_value());
    CHECK(after[0]->item == item("water_bucket"));
    CHECK(after[0]->count == 1);
    // The pour is the whole turn: what is left is not also thrown.
    CHECK(world.items().empty());
}

TEST_CASE("an empty bucket picks the source up in front") {
    Fixture fixture;
    WorldState& world = fixture.world;
    fixture.floor();
    REQUIRE(world.setBlockState(8, 65, 8, baseStateId(FluidKind::Water)));
    oneItem(world, "up", ItemStack{.item = item("bucket"), .count = 1});

    CHECK(dispensers::fire(fixture.context(), 8, 64, 8, ids()).fired);

    CHECK(world.blockState(8, 65, 8) == registry::blocks::air().stateId);
    CHECK(slotsAt(world, 8, 64, 8, dispensers::kSize)[0]->item == item("water_bucket"));
}

TEST_CASE("a bucket with nothing to do falls through to being thrown") {
    Fixture fixture;
    WorldState& world = fixture.world;
    fixture.floor();
    // Nothing to pick up out of plain air, so the empty bucket is thrown instead.
    oneItem(world, "up", ItemStack{.item = item("bucket"), .count = 1});

    CHECK(dispensers::fire(fixture.context(), 8, 64, 8, ids()).fired);

    REQUIRE(world.items().size() == 1);
    CHECK(world.items()[0].item == item("bucket"));
    CHECK_FALSE(slotsAt(world, 8, 64, 8, dispensers::kSize)[0].has_value());
}

TEST_CASE("flint and steel lights a fire in front and stays in the slot") {
    Fixture fixture;
    WorldState& world = fixture.world;
    fixture.floor();
    oneItem(world, "east", ItemStack{.item = item("flint_and_steel"), .count = 1});

    CHECK(dispensers::fire(fixture.context(), 8, 64, 8, ids()).fired);

    const std::optional<std::int32_t> lit = world.blockState(9, 64, 8);
    REQUIRE(lit.has_value());
    CHECK(fires::isFire(*lit));
    // parity: the tool is not spent, it comes back into the slot as it was.
    const blockInventory::Slots after = slotsAt(world, 8, 64, 8, dispensers::kSize);
    REQUIRE(after[0].has_value());
    CHECK(after[0]->item == item("flint_and_steel"));
    CHECK(after[0]->count == 1);
    CHECK(world.items().empty());
}

TEST_CASE("flint and steel with nowhere to light still wears, and is never thrown") {
    Fixture fixture;
    WorldState& world = fixture.world;
    fixture.floor();
    // Facing into stone, where no fire fits.
    REQUIRE(world.setBlockState(9, 64, 8, stateOf("stone")));
    oneItem(world, "east", ItemStack{.item = item("flint_and_steel"), .count = 1});

    CHECK(dispensers::fire(fixture.context(), 8, 64, 8, ids()).fired);

    // parity: `FlintAndSteelDispenseItemBehavior` says success for everything but a tnt block it
    // could not prime, so aiming it at a wall costs a point of durability and nothing else.
    CHECK(world.items().empty());
    const std::optional<ItemStack> left = slotsAt(world, 8, 64, 8, dispensers::kSize)[0];
    REQUIRE(left.has_value());
    CHECK(left->item == item("flint_and_steel"));
}

TEST_CASE("a dropper pours nothing and lights nothing") {
    Fixture fixture;
    WorldState& world = fixture.world;
    fixture.floor();
    oneItem(world, "up", ItemStack{.item = item("water_bucket"), .count = 1}, 0, "dropper");

    CHECK(dispensers::fire(fixture.context(), 8, 64, 8, ids()).fired);

    CHECK(world.blockState(8, 65, 8) == registry::blocks::air().stateId);
    REQUIRE(world.items().size() == 1);
    CHECK(world.items()[0].item == item("water_bucket"));
}

TEST_CASE("the hooks decide what counts as a container in front") {
    Fixture fixture;
    WorldState& world = fixture.world;
    oneItem(world, "north", ItemStack{.item = item("stone"), .count = 1});
    REQUIRE(world.setBlockState(8, 64, 7, stateOf("chest")));
    fill(world, 8, 64, 7, blockInventory::Slots(27), "chest");
    dispensers::Hooks hooks = ids();
    hooks.containerSize = [](std::string_view) { return std::optional<std::int32_t>{}; };

    CHECK(dispensers::fire(fixture.context(), 8, 64, 8, hooks).fired);

    // Told there is no container, it throws past the chest instead of filling it.
    CHECK(world.items().size() == 1);
    CHECK(slotsAt(world, 8, 64, 7, 27)[0].has_value() == false);
}

TEST_CASE("a hook can name the protocol type the projectile spawns with") {
    Fixture fixture;
    WorldState& world = fixture.world;
    oneItem(world, "east", ItemStack{.item = item("snowball"), .count = 1});
    dispensers::Hooks hooks = ids();
    hooks.protocolTypeOf = [](const basalt::wire::common::Identifier& type) {
        return type == item("snowball") ? std::optional<std::int32_t>{123} : std::nullopt;
    };

    CHECK(dispensers::fire(fixture.context(), 8, 64, 8, hooks).fired);

    REQUIRE(world.entities().size() == 1);
    CHECK(world.entities()[0]->protocolType() == 123);
}

TEST_CASE("with no hook for it the thrown item takes entity id zero") {
    Fixture fixture;
    oneItem(fixture.world, "north", ItemStack{.item = item("stone"), .count = 1});

    CHECK(dispensers::fire(fixture.context(), 8, 64, 8, {}).fired);

    REQUIRE(fixture.world.items().size() == 1);
    CHECK(fixture.world.items()[0].entityId == 0);
}

TEST_CASE("a container in front with no block entity takes nothing") {
    Fixture fixture;
    WorldState& world = fixture.world;
    oneItem(world, "north", ItemStack{.item = item("stone"), .count = 1});
    // A chest that was never given its contents, which is what a half-built world looks like.
    REQUIRE(world.setBlockState(8, 64, 7, stateOf("chest")));

    const dispensers::Result result = dispensers::fire(fixture.context(), 8, 64, 8, ids());

    CHECK_FALSE(result.fired);
    CHECK(world.items().empty());
    CHECK(slotsAt(world, 8, 64, 8, dispensers::kSize)[0]->count == 1);
}

TEST_CASE("the container in front may be in another chunk") {
    Fixture fixture;
    WorldState& world = fixture.world;
    world.getOrGenerateChunk(1, 1);
    REQUIRE(world.setBlockState(22, 64, 19, dispenser("west", "true")));
    blockInventory::Slots slots(dispensers::kSize);
    slots[0] = ItemStack{.item = item("stone"), .count = 1};
    fill(world, 22, 64, 19, slots);
    REQUIRE(world.setBlockState(21, 64, 19, stateOf("chest")));
    fill(world, 21, 64, 19, blockInventory::Slots(27), "chest");
    world.chunkIfLoaded(0, 0)->markClean();
    world.chunkIfLoaded(1, 1)->markClean();

    CHECK(dispensers::fire(fixture.context(), 22, 64, 19, ids()).fired);

    CHECK(slotsAt(world, 21, 64, 19, 27)[0]->count == 1);
    CHECK(world.chunkIfLoaded(1, 1)->isDirty());
    CHECK_FALSE(world.chunkIfLoaded(0, 0)->isDirty());
}

TEST_CASE("a signal of one is a signal, beside the block and beside the block above") {
    const std::optional<std::int32_t> weak = registry::blockStateDefinitions::stateIdOf(
        item("light_weighted_pressure_plate"), {{"power", "1"}}, std::nullopt);
    REQUIRE(weak.has_value());

    const auto triggers = [&weak](std::int32_t sourceY) {
        Fixture fixture;
        WorldState& world = fixture.world;
        REQUIRE(world.setBlockState(8, 64, 8, dispenser("north", "false")));
        REQUIRE(world.setBlockState(9, sourceY, 8, *weak));
        return dispensers::onNeighbourChange(world, "overworld", 8, 64, 8).has_value();
    };

    CHECK(triggers(64));
    CHECK(triggers(65));
}

TEST_CASE("another block that has a triggered state of its own is still not a dispenser") {
    Fixture fixture;
    WorldState& world = fixture.world;
    // A crafter carries `triggered` too, so only the block's own name tells the two apart.
    const std::optional<std::int32_t> crafter = registry::blockStateDefinitions::stateIdOf(
        item("crafter"), {{"orientation", "north_up"}, {"triggered", "false"}, {"crafting", "false"}}, std::nullopt);
    REQUIRE(crafter.has_value());
    REQUIRE(world.setBlockState(8, 64, 8, *crafter));
    REQUIRE(world.setBlockState(9, 64, 8, stateOf("redstone_block")));

    CHECK_FALSE(dispensers::onNeighbourChange(world, "overworld", 8, 64, 8).has_value());
}

TEST_CASE("another container that faces somewhere is still not a dispenser") {
    Fixture fixture;
    WorldState& world = fixture.world;
    // A chest has both `facing` and contents, so only the block's own name tells the two apart.
    REQUIRE(world.setBlockState(8, 64, 8, stateOf("chest")));
    blockInventory::Slots slots(27);
    slots[0] = ItemStack{.item = item("stone"), .count = 1};
    fill(world, 8, 64, 8, slots, "chest");

    CHECK_FALSE(dispensers::fire(fixture.context(), 8, 64, 8, ids()).fired);

    CHECK(world.items().empty());
    CHECK(slotsAt(world, 8, 64, 8, 27)[0]->count == 1);
}

TEST_CASE("bone meal grows the crop in front and is spent doing it") {
    Fixture fixture;
    WorldState& world = fixture.world;
    oneItem(world, "north", ItemStack{.item = item("bone_meal"), .count = 3});
    const std::optional<std::int32_t> nearlyGrown =
        registry::blockStateDefinitions::stateIdOf(item("wheat"), {{"age", "6"}}, std::nullopt);
    REQUIRE(nearlyGrown.has_value());
    REQUIRE(world.setBlockState(8, 64, 7, *nearlyGrown));

    const dispensers::Result result = dispensers::fire(fixture.context(), 8, 64, 8, ids());

    // Two ages at least, so a crop this close to the end saturates whatever the roll says.
    const std::optional<std::int32_t> grown =
        registry::blockStateDefinitions::stateIdOf(item("wheat"), {{"age", "7"}}, std::nullopt);
    CHECK(result.fired);
    CHECK(world.blockState(8, 64, 7) == grown);
    CHECK(slotsAt(world, 8, 64, 8, dispensers::kSize)[0]->count == 2);
    CHECK(world.items().empty());
    // parity: `DefaultDispenseItemBehavior.dispense`, the sound and the smoke after a successful
    // behaviour just the same as after a throw.
    REQUIRE(result.effects.size() == 2);
    CHECK(result.effects[0] ==
          BlockEffect{blockEffect::Effect{.type = blockEffect::Effect::kDispenserDispense, .data = 0}});
    CHECK(result.effects[1] ==
          BlockEffect{blockEffect::Effect{.type = blockEffect::Effect::kDispenserSmoke, .data = 2}});
    REQUIRE(result.changes.size() == 1);
    CHECK(result.changes[0].x == 8);
    CHECK(result.changes[0].z == 7);
    REQUIRE(result.changes[0].effects.size() == 1);
    CHECK(std::get<blockEffect::Effect>(result.changes[0].effects[0]).type == blockEffect::Effect::kPlantGrowth);

    // A grown crop is not a target at all, so the second one is refused rather than thrown.
    const dispensers::Result again = dispensers::fire(fixture.context(), 8, 64, 8, ids());
    CHECK_FALSE(again.fired);
    CHECK(slotsAt(world, 8, 64, 8, dispensers::kSize)[0]->count == 2);
    CHECK(world.items().empty());
    REQUIRE(again.effects.size() == 2);
    CHECK(std::get<blockEffect::Effect>(again.effects[0]).type == blockEffect::Effect::kDispenserFail);
    CHECK(std::get<blockEffect::Effect>(again.effects[1]) ==
          blockEffect::Effect{.type = blockEffect::Effect::kDispenserSmoke, .data = 2});
}

TEST_CASE("a honeycomb waxes the copper chest in front without emptying it") {
    Fixture fixture;
    WorldState& world = fixture.world;
    oneItem(world, "north", ItemStack{.item = item("honeycomb"), .count = 2});
    REQUIRE(world.setBlockState(8, 64, 7, stateOf("exposed_copper_chest")));
    blockInventory::Slots inChest(27);
    inChest[0] = ItemStack{.item = item("diamond"), .count = 5};
    fill(world, 8, 64, 7, inChest, "exposed_copper_chest");

    const dispensers::Result result = dispensers::fire(fixture.context(), 8, 64, 8, ids());

    CHECK(result.fired);
    CHECK(world.blockState(8, 64, 7) == stateOf("waxed_exposed_copper_chest"));
    CHECK(slotsAt(world, 8, 64, 8, dispensers::kSize)[0]->count == 1);
    // parity: `shouldChangedStateKeepBlockEntity`, so the chest keeps what it held and nothing
    // spills on the floor.
    CHECK(world.takeSpilledContents().empty());
    const blockInventory::Slots after = slotsAt(world, 8, 64, 7, 27);
    REQUIRE(after[0].has_value());
    CHECK(after[0]->item == item("diamond"));
    CHECK(after[0]->count == 5);
}

TEST_CASE("glowstone charges the respawn anchor in front until the fourth one") {
    Fixture fixture;
    WorldState& world = fixture.world;
    oneItem(world, "north", ItemStack{.item = item("glowstone"), .count = 6});
    const auto anchorAt = [](const std::string& charges) {
        const std::optional<std::int32_t> stateId =
            registry::blockStateDefinitions::stateIdOf(item("respawn_anchor"), {{"charges", charges}}, std::nullopt);
        REQUIRE(stateId.has_value());
        return *stateId;
    };
    REQUIRE(world.setBlockState(8, 64, 7, anchorAt("3")));

    const dispensers::Result charged = dispensers::fire(fixture.context(), 8, 64, 8, ids());
    CHECK(charged.fired);
    CHECK(world.blockState(8, 64, 7) == anchorAt("4"));
    CHECK(slotsAt(world, 8, 64, 8, dispensers::kSize)[0]->count == 5);

    // A full anchor refuses: the piece stays in the slot and nothing is thrown.
    const dispensers::Result full = dispensers::fire(fixture.context(), 8, 64, 8, ids());
    CHECK_FALSE(full.fired);
    CHECK(slotsAt(world, 8, 64, 8, dispensers::kSize)[0]->count == 5);
    CHECK(world.items().empty());

    // Aimed at anything else it is thrown like any other item.
    REQUIRE(world.setBlockState(8, 64, 7, stateOf("air")));
    CHECK(dispensers::fire(fixture.context(), 8, 64, 8, ids()).fired);
    CHECK(world.items().size() == 1);
}

TEST_CASE("shears empty the full beehive in front and wear one point doing it") {
    Fixture fixture;
    WorldState& world = fixture.world;
    oneItem(world, "north", ItemStack{.item = item("shears"), .count = 1});
    const auto hiveAt = [](const std::string& honey) {
        const std::optional<std::int32_t> stateId = registry::blockStateDefinitions::stateIdOf(
            item("beehive"), {{"honey_level", honey}, {"facing", "north"}}, std::nullopt);
        REQUIRE(stateId.has_value());
        return *stateId;
    };
    REQUIRE(world.setBlockState(8, 64, 7, hiveAt("5")));

    const dispensers::Result sheared = dispensers::fire(fixture.context(), 8, 64, 8, ids());

    CHECK(sheared.fired);
    CHECK(world.blockState(8, 64, 7) == hiveAt("0"));
    REQUIRE(world.items().size() == 1);
    CHECK(world.items()[0].item == item("honeycomb"));
    CHECK(world.items()[0].count == 3);
    // parity: `hurtAndBreak(1, ...)`, which is a damage component and not a smaller stack.
    const std::optional<ItemStack> left = slotsAt(world, 8, 64, 8, dispensers::kSize)[0];
    REQUIRE(left.has_value());
    CHECK(left->item == item("shears"));
    CHECK(left->count == 1);

    // An empty hive is a refusal: the shears stay, unworn, and nothing is thrown.
    const dispensers::Result again = dispensers::fire(fixture.context(), 8, 64, 8, ids());
    CHECK_FALSE(again.fired);
    CHECK(world.items().size() == 1);
    CHECK(slotsAt(world, 8, 64, 8, dispensers::kSize)[0].has_value());
}
