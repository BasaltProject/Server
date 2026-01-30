#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/BlockInventory.hpp>
#include <basalt/server/runtime/world/Crafters.hpp>

#include <doctest.h>

#include <array>
#include <cstdint>
#include <map>
#include <string>

using namespace basalt::server::runtime::world;
using basalt::server::world::Chunk;
using basalt::server::world::loot::ItemStack;
namespace registry = basalt::server::registry;
namespace nbt = basalt::wire::nbt;

namespace {

[[nodiscard]] basalt::wire::common::Identifier item(const std::string& path) {
    return basalt::wire::common::Identifier::vanilla(path);
}

[[nodiscard]] std::int32_t crafter(const std::string& triggered, const std::string& crafting,
                                   const std::string& orientation = "north_up") {
    const std::optional<std::int32_t> stateId = registry::blockStateDefinitions::stateIdOf(
        item("crafter"), {{"orientation", orientation}, {"triggered", triggered}, {"crafting", crafting}},
        std::nullopt);
    REQUIRE(stateId.has_value());
    return *stateId;
}

[[nodiscard]] WorldState emptyWorld() {
    return WorldState{{}, [](std::int32_t chunkX, std::int32_t chunkZ) { return Chunk{chunkX, chunkZ}; }};
}

struct Loaded {
    WorldState state = emptyWorld();

    Loaded() { state.getOrGenerateChunk(0, 0); }
};

[[nodiscard]] nbt::NbtCompound& dataAt(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    basalt::server::world::Chunk* chunk = world.chunkIfLoaded(x >> 4, z >> 4);
    REQUIRE(chunk != nullptr);
    basalt::server::world::ChunkBlockEntity* entity = chunk->blockEntityAt(x & 15, y, z & 15);
    REQUIRE(entity != nullptr);
    return entity->data();
}

/// A crafter with those slots filled and a block entity to keep them in.
void fill(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, const blockInventory::Slots& slots,
          const std::string& path = "crafter") {
    nbt::NbtCompound data;
    blockInventory::write(data, slots);
    world.chunkIfLoaded(x >> 4, z >> 4)
        ->setBlockEntity(basalt::server::world::ChunkBlockEntity{x & 15, y, z & 15, item(path), std::move(data)});
}

/// Hooks that make one plank out of any grid that has something in it.
[[nodiscard]] crafters::Hooks planks(std::int32_t* asked = nullptr) {
    return crafters::Hooks{
        .resultFor = [asked](const basalt::server::world::crafting::CraftingInput& input) -> std::optional<ItemStack> {
            if (asked != nullptr) {
                ++*asked;
            }
            if (input.ingredientCount() == 0) {
                return std::nullopt;
            }
            return ItemStack{.item = item("oak_planks"), .count = 1};
        },
        .allocateEntityId = [] { return 7; }};
}

} // namespace

TEST_CASE("a rising signal books a turn and a falling one forgets it") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(8, 64, 8, crafter("false", "false")));
    REQUIRE(world.setBlockState(9, 64, 8, registry::blocks::named("redstone_block")->stateId));

    const std::optional<BlockChange> risen = crafters::onNeighbourChange(world, "overworld", 8, 64, 8);
    REQUIRE(risen.has_value());
    CHECK(risen->stateId == crafter("true", "false"));
    CHECK(world.hasScheduledBlockTick(8, 64, 8, crafters::block()));

    // Asked again with the signal still up, nothing changes.
    CHECK_FALSE(crafters::onNeighbourChange(world, "overworld", 8, 64, 8).has_value());

    REQUIRE(world.setBlockState(9, 64, 8, registry::blocks::air().stateId));
    REQUIRE(world.setBlockState(8, 64, 8, crafter("true", "true")));
    const std::optional<BlockChange> fallen = crafters::onNeighbourChange(world, "overworld", 8, 64, 8);
    REQUIRE(fallen.has_value());
    // The animation is forgotten along with the trigger.
    CHECK(fallen->stateId == crafter("false", "false"));
}

TEST_CASE("a crafter puts what it made into the chest it faces") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(8, 64, 8, crafter("true", "false")));
    blockInventory::Slots slots(9);
    slots[0] = ItemStack{.item = item("oak_log"), .count = 2};
    fill(world, 8, 64, 8, slots);
    // The crafter faces north, which is one block back in z.
    REQUIRE(world.setBlockState(8, 64, 7, registry::blocks::named("chest")->stateId));
    fill(world, 8, 64, 7, blockInventory::Slots(27), "chest");

    const std::optional<BlockChange> crafted = crafters::craft(world, 8, 64, 8, planks());

    REQUIRE(crafted.has_value());
    CHECK(crafted->stateId == crafter("true", "true"));
    const blockInventory::Slots inChest = blockInventory::read(dataAt(world, 8, 64, 7), 27);
    REQUIRE(inChest[0].has_value());
    CHECK(inChest[0]->item == item("oak_planks"));
    // Every slot with something in it loses exactly one.
    CHECK(blockInventory::read(dataAt(world, 8, 64, 8), 9)[0]->count == 1);
    CHECK(world.items().empty());
}

TEST_CASE("with nowhere to put it, the result is thrown out the front") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(8, 64, 8, crafter("true", "false")));
    blockInventory::Slots slots(9);
    slots[4] = ItemStack{.item = item("oak_log"), .count = 1};
    fill(world, 8, 64, 8, slots);

    REQUIRE(crafters::craft(world, 8, 64, 8, planks()).has_value());

    const std::vector<DroppedItem> thrown = world.items();
    REQUIRE(thrown.size() == 1);
    CHECK(thrown[0].item == item("oak_planks"));
    CHECK(thrown[0].x == doctest::Approx(8.5));
    CHECK(thrown[0].z == doctest::Approx(7.5));
    CHECK(thrown[0].entityId == 7);
    // The last of a stack leaves the slot empty.
    CHECK_FALSE(blockInventory::read(dataAt(world, 8, 64, 8), 9)[4].has_value());
}

TEST_CASE("the result goes out of whichever face the crafter points at") {
    const auto thrownAt = [](const std::string& orientation) {
        Loaded loaded;
        WorldState& world = loaded.state;
        REQUIRE(world.setBlockState(8, 64, 8, crafter("true", "false", orientation)));
        blockInventory::Slots slots(9);
        slots[0] = ItemStack{.item = item("oak_log"), .count = 1};
        fill(world, 8, 64, 8, slots);
        REQUIRE(crafters::craft(world, 8, 64, 8, planks()).has_value());
        const std::vector<DroppedItem> thrown = world.items();
        REQUIRE(thrown.size() == 1);
        return std::array<double, 3>{thrown[0].x, thrown[0].y, thrown[0].z};
    };

    // parity: the first word of `orientation` is the way the front faces.
    CHECK(thrownAt("north_up") == std::array<double, 3>{8.5, 64.5, 7.5});
    CHECK(thrownAt("south_up") == std::array<double, 3>{8.5, 64.5, 9.5});
    CHECK(thrownAt("west_up") == std::array<double, 3>{7.5, 64.5, 8.5});
    CHECK(thrownAt("east_up") == std::array<double, 3>{9.5, 64.5, 8.5});
    CHECK(thrownAt("down_north") == std::array<double, 3>{8.5, 63.5, 8.5});
    CHECK(thrownAt("up_north") == std::array<double, 3>{8.5, 65.5, 8.5});
}

TEST_CASE("the result stacks onto what is already in the chest, until the stack is full") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(8, 64, 8, crafter("true", "false")));
    REQUIRE(world.setBlockState(8, 64, 7, registry::blocks::named("chest")->stateId));
    blockInventory::Slots inChest(27);
    inChest[0] = ItemStack{.item = item("oak_planks"), .count = 64};
    inChest[1] = ItemStack{.item = item("oak_planks"), .count = 3};
    fill(world, 8, 64, 7, inChest, "chest");
    blockInventory::Slots grid(9);
    grid[0] = ItemStack{.item = item("oak_log"), .count = 5};
    fill(world, 8, 64, 8, grid);

    REQUIRE(crafters::craft(world, 8, 64, 8, planks()).has_value());

    const blockInventory::Slots after = blockInventory::read(dataAt(world, 8, 64, 7), 27);
    // The full slot is left alone and the part-full one takes it.
    CHECK(after[0]->count == 64);
    CHECK(after[1]->count == 4);
    CHECK(world.items().empty());
}

TEST_CASE("the crafter books four ticks and animates for six") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(8, 64, 8, crafter("false", "false")));
    REQUIRE(world.setBlockState(9, 64, 8, registry::blocks::named("redstone_block")->stateId));
    REQUIRE(crafters::onNeighbourChange(world, "overworld", 8, 64, 8).has_value());

    for (std::int32_t turn = 0; turn < 3; ++turn) {
        CHECK(world.dueBlockTicks().empty());
    }
    CHECK(world.dueBlockTicks().size() == 1);

    blockInventory::Slots grid(9);
    grid[0] = ItemStack{.item = item("oak_log"), .count = 1};
    fill(world, 8, 64, 8, grid);
    REQUIRE(crafters::craft(world, 8, 64, 8, planks()).has_value());
    CHECK(dataAt(world, 8, 64, 8).getInt("crafting_ticks_remaining") == 6);
}

TEST_CASE("a result of nothing at all is no result") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(8, 64, 8, crafter("true", "false")));
    blockInventory::Slots grid(9);
    grid[0] = ItemStack{.item = item("oak_log"), .count = 1};
    fill(world, 8, 64, 8, grid);
    crafters::Hooks nothing = planks();
    nothing.resultFor = [](const basalt::server::world::crafting::CraftingInput&) {
        return std::optional{ItemStack{.item = item("oak_planks"), .count = 0}};
    };

    // parity: `dispenseFrom` raises `levelEvent(1050)` and nothing else: the block stays as it was and
    // the click is the whole of what the crafter does.
    const std::optional<BlockChange> clicked = crafters::craft(world, 8, 64, 8, nothing);
    REQUIRE(clicked.has_value());
    CHECK(clicked->stateId == crafter("true", "false"));
    REQUIRE(clicked->effects.size() == 1);
    const auto* click = std::get_if<blockEffect::Effect>(&clicked->effects.front());
    REQUIRE(click != nullptr);
    CHECK(click->type == blockEffect::Effect::kCrafterFail);
    CHECK(world.items().empty());
}

TEST_CASE("a tag that never started an animation has none to finish") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(8, 64, 8, crafter("true", "true")));
    nbt::NbtCompound data;

    CHECK_FALSE(crafters::tick(world, 8, 64, 8, data).has_value());
    CHECK(world.blockState(8, 64, 8) == crafter("true", "true"));
}

TEST_CASE("a grid that makes nothing crafts nothing") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(8, 64, 8, crafter("true", "false")));
    fill(world, 8, 64, 8, blockInventory::Slots(9));
    std::int32_t asked = 0;

    const std::optional<BlockChange> clicked = crafters::craft(world, 8, 64, 8, planks(&asked));
    REQUIRE(clicked.has_value());
    CHECK(std::get_if<blockEffect::Effect>(&clicked->effects.front()) != nullptr);

    CHECK(asked == 1);
    CHECK(world.blockState(8, 64, 8) == crafter("true", "false"));
    CHECK(world.items().empty());
}

TEST_CASE("the animation lasts six ticks and then the block goes still") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(8, 64, 8, crafter("true", "true")));
    nbt::NbtCompound data;
    data.putInt("crafting_ticks_remaining", 6);

    for (std::int32_t turn = 0; turn < 5; ++turn) {
        CHECK_FALSE(crafters::tick(world, 8, 64, 8, data).has_value());
    }
    const std::optional<BlockChange> still = crafters::tick(world, 8, 64, 8, data);
    REQUIRE(still.has_value());
    CHECK(still->stateId == crafter("true", "false"));

    // Once still there is nothing left to count down.
    CHECK_FALSE(crafters::tick(world, 8, 64, 8, data).has_value());
}

TEST_CASE("the comparator counts slots, not items") {
    nbt::NbtCompound data;
    blockInventory::Slots slots(9);
    slots[0] = ItemStack{.item = item("stone"), .count = 1};
    slots[1] = ItemStack{.item = item("stone"), .count = 64};
    blockInventory::write(data, slots);

    // A slot with one item reads the same as a full one.
    CHECK(crafters::comparatorSignal(data) == 2);

    // And a slot switched off reads as taken.
    data.putIntArray("disabled_slots", {5, 6});
    CHECK(crafters::comparatorSignal(data) == 4);
    CHECK(crafters::disabledSlots(data) == std::set<std::int32_t>{5, 6});
    CHECK_FALSE(crafters::accepts(data, 5));
    CHECK(crafters::accepts(data, 0));
    CHECK(crafters::accepts(data, 8));
}

TEST_CASE("a crafter away from the middle chunk crafts into its own neighbour") {
    Loaded loaded;
    WorldState& world = loaded.state;
    world.getOrGenerateChunk(1, 1);
    REQUIRE(world.setBlockState(21, 64, 21, crafter("true", "false")));
    blockInventory::Slots grid(9);
    grid[0] = ItemStack{.item = item("oak_log"), .count = 2};
    fill(world, 21, 64, 21, grid);
    // North of it, still in the same chunk.
    REQUIRE(world.setBlockState(21, 64, 20, registry::blocks::named("chest")->stateId));
    fill(world, 21, 64, 20, blockInventory::Slots(27), "chest");

    REQUIRE(crafters::craft(world, 21, 64, 21, planks()).has_value());

    const blockInventory::Slots inChest = blockInventory::read(dataAt(world, 21, 64, 20), 27);
    REQUIRE(inChest[0].has_value());
    CHECK(inChest[0]->item == item("oak_planks"));
    CHECK(blockInventory::read(dataAt(world, 21, 64, 21), 9)[0]->count == 1);
    CHECK(world.items().empty());
}

TEST_CASE("anything that is not a crafter is left alone") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(8, 64, 8, registry::blocks::named("stone")->stateId));

    CHECK_FALSE(crafters::onNeighbourChange(world, "overworld", 8, 64, 8).has_value());
    CHECK_FALSE(crafters::craft(world, 8, 64, 8, planks()).has_value());
    CHECK_FALSE(crafters::onNeighbourChange(world, "overworld", 300, 64, 300).has_value());
}
