#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/BlockInventory.hpp>
#include <basalt/server/runtime/world/Hoppers.hpp>

#include <doctest.h>

#include <cstdint>
#include <string>

using namespace basalt::server::runtime::world;
using basalt::server::world::Chunk;
using basalt::server::world::loot::ItemStack;
namespace registry = basalt::server::registry;
namespace nbt = basalt::wire::nbt;

namespace {

constexpr std::string_view kCooldownKey = "TransferCooldown";

[[nodiscard]] basalt::wire::common::Identifier item(const std::string& path) {
    return basalt::wire::common::Identifier::vanilla(path);
}

[[nodiscard]] std::int32_t hopper(const std::string& facing = "down", const std::string& enabled = "true") {
    const std::optional<std::int32_t> stateId = registry::blockStateDefinitions::stateIdOf(
        item("hopper"), {{"facing", facing}, {"enabled", enabled}}, std::nullopt);
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
    Chunk* chunk = world.chunkIfLoaded(x >> 4, z >> 4);
    REQUIRE(chunk != nullptr);
    basalt::server::world::ChunkBlockEntity* entity = chunk->blockEntityAt(x & 15, y, z & 15);
    REQUIRE(entity != nullptr);
    return entity->data();
}

/// A block entity with those slots in it, which is the only way a container holds anything.
void fill(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, const blockInventory::Slots& slots,
          const std::string& path) {
    nbt::NbtCompound data;
    blockInventory::write(data, slots);
    world.chunkIfLoaded(x >> 4, z >> 4)
        ->setBlockEntity(basalt::server::world::ChunkBlockEntity{x & 15, y, z & 15, item(path), std::move(data)});
}

/// A hopper with a block entity, facing whichever way, at those coordinates.
void placeHopper(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, const blockInventory::Slots& slots,
                 const std::string& facing = "down", const std::string& enabled = "true") {
    REQUIRE(world.setBlockState(x, y, z, hopper(facing, enabled)));
    fill(world, x, y, z, slots, "hopper");
}

void placeContainer(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                    const blockInventory::Slots& slots, const std::string& path) {
    REQUIRE(world.setBlockState(x, y, z, registry::blocks::named(path)->stateId));
    fill(world, x, y, z, slots, path);
}

[[nodiscard]] blockInventory::Slots slotsOf(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                            std::int32_t size) {
    return blockInventory::read(dataAt(world, x, y, z), size);
}

[[nodiscard]] std::int32_t cooldownOf(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    return dataAt(world, x, y, z).getInt(kCooldownKey, 0);
}

[[nodiscard]] blockInventory::Slots empty(std::int32_t size) {
    return blockInventory::Slots(static_cast<std::size_t>(size));
}

[[nodiscard]] blockInventory::Slots oneStack(std::int32_t size, std::size_t slot, const std::string& path,
                                             std::int32_t count) {
    blockInventory::Slots slots(static_cast<std::size_t>(size));
    slots[slot] = ItemStack{.item = item(path), .count = count};
    return slots;
}

} // namespace

TEST_CASE("a hopper on cooldown counts down and moves nothing") {
    Loaded loaded;
    WorldState& world = loaded.state;
    placeHopper(world, 8, 64, 8, oneStack(hoppers::kSize, 0, "stone", 3));
    placeContainer(world, 8, 63, 8, empty(27), "chest");
    dataAt(world, 8, 64, 8).putInt(kCooldownKey, 3);

    CHECK(hoppers::tick(world, 8, 64, 8));

    CHECK(cooldownOf(world, 8, 64, 8) == 2);
    CHECK(slotsOf(world, 8, 64, 8, hoppers::kSize)[0]->count == 3);
    CHECK_FALSE(slotsOf(world, 8, 63, 8, 27)[0].has_value());

    // The last tick of the cooldown is spent counting down, not moving.
    dataAt(world, 8, 64, 8).putInt(kCooldownKey, 1);
    CHECK(hoppers::tick(world, 8, 64, 8));
    CHECK(cooldownOf(world, 8, 64, 8) == 0);
    CHECK_FALSE(slotsOf(world, 8, 63, 8, 27)[0].has_value());
}

TEST_CASE("a hopper gives one item to what it faces and books eight ticks") {
    Loaded loaded;
    WorldState& world = loaded.state;
    // Slot zero is empty, so the first slot with something in it is the second one.
    placeHopper(world, 8, 64, 8, oneStack(hoppers::kSize, 1, "stone", 3));
    placeContainer(world, 8, 63, 8, empty(27), "chest");

    CHECK(hoppers::tick(world, 8, 64, 8));

    const blockInventory::Slots inChest = slotsOf(world, 8, 63, 8, 27);
    REQUIRE(inChest[0].has_value());
    CHECK(inChest[0]->item == item("stone"));
    CHECK(inChest[0]->count == 1);
    CHECK_FALSE(inChest[1].has_value());
    const blockInventory::Slots inHopper = slotsOf(world, 8, 64, 8, hoppers::kSize);
    CHECK_FALSE(inHopper[0].has_value());
    CHECK(inHopper[1]->count == 2);
    CHECK(cooldownOf(world, 8, 64, 8) == 8);
}

TEST_CASE("the last of a stack leaves the slot empty") {
    Loaded loaded;
    WorldState& world = loaded.state;
    placeHopper(world, 8, 64, 8, oneStack(hoppers::kSize, 0, "stone", 1));
    placeContainer(world, 8, 63, 8, oneStack(27, 0, "stone", 5), "chest");

    CHECK(hoppers::tick(world, 8, 64, 8));

    CHECK_FALSE(slotsOf(world, 8, 64, 8, hoppers::kSize)[0].has_value());
    // It stacks onto what is already there rather than taking the next slot.
    CHECK(slotsOf(world, 8, 63, 8, 27)[0]->count == 6);
    CHECK_FALSE(slotsOf(world, 8, 63, 8, 27)[1].has_value());
}

TEST_CASE("a full slot takes nothing and the next item along goes instead") {
    Loaded loaded;
    WorldState& world = loaded.state;
    blockInventory::Slots inHopper = empty(hoppers::kSize);
    inHopper[0] = ItemStack{.item = item("stone"), .count = 2};
    inHopper[1] = ItemStack{.item = item("dirt"), .count = 2};
    placeHopper(world, 8, 64, 8, inHopper);
    blockInventory::Slots inChest(27);
    for (std::size_t slot = 0; slot < 26; ++slot) {
        inChest[slot] = ItemStack{.item = item("dirt"), .count = 64};
    }
    inChest[26] = ItemStack{.item = item("dirt"), .count = 1};
    placeContainer(world, 8, 63, 8, inChest, "chest");

    CHECK(hoppers::tick(world, 8, 64, 8));

    // The stone fits nowhere, so the dirt behind it moves.
    CHECK(slotsOf(world, 8, 64, 8, hoppers::kSize)[0]->count == 2);
    CHECK(slotsOf(world, 8, 64, 8, hoppers::kSize)[1]->count == 1);
    CHECK(slotsOf(world, 8, 63, 8, 27)[26]->count == 2);
    CHECK(slotsOf(world, 8, 63, 8, 27)[25]->count == 64);
}

TEST_CASE("a chest with no room at all takes nothing") {
    Loaded loaded;
    WorldState& world = loaded.state;
    placeHopper(world, 8, 64, 8, oneStack(hoppers::kSize, 0, "stone", 2));
    blockInventory::Slots inChest(27);
    for (std::size_t slot = 0; slot < 27; ++slot) {
        inChest[slot] = ItemStack{.item = item("dirt"), .count = 64};
    }
    placeContainer(world, 8, 63, 8, inChest, "chest");

    CHECK_FALSE(hoppers::tick(world, 8, 64, 8));

    CHECK(slotsOf(world, 8, 64, 8, hoppers::kSize)[0]->count == 2);
    CHECK(cooldownOf(world, 8, 64, 8) == 0);
}

TEST_CASE("a hopper that is switched off does nothing at all") {
    Loaded loaded;
    WorldState& world = loaded.state;
    placeHopper(world, 8, 64, 8, oneStack(hoppers::kSize, 0, "stone", 3), "down", "false");
    placeContainer(world, 8, 63, 8, empty(27), "chest");

    CHECK_FALSE(hoppers::tick(world, 8, 64, 8));

    CHECK(slotsOf(world, 8, 64, 8, hoppers::kSize)[0]->count == 3);
    CHECK_FALSE(slotsOf(world, 8, 63, 8, 27)[0].has_value());
    CHECK(cooldownOf(world, 8, 64, 8) == 0);
}

TEST_CASE("a hopper faces whichever way its state says") {
    const auto pushesTo = [](const std::string& facing, std::int32_t tx, std::int32_t ty, std::int32_t tz) {
        Loaded loaded;
        WorldState& world = loaded.state;
        placeHopper(world, 8, 64, 8, oneStack(hoppers::kSize, 0, "stone", 1), facing);
        placeContainer(world, tx, ty, tz, empty(27), "chest");
        const bool moved = hoppers::tick(world, 8, 64, 8);
        return moved && slotsOf(world, tx, ty, tz, 27)[0].has_value();
    };

    CHECK(pushesTo("down", 8, 63, 8));
    CHECK(pushesTo("north", 8, 64, 7));
    CHECK(pushesTo("south", 8, 64, 9));
    CHECK(pushesTo("west", 7, 64, 8));
    CHECK(pushesTo("east", 9, 64, 8));
    // A hopper never faces up, and a chest on top of it is pulled from, not pushed into.
    CHECK_FALSE(pushesTo("down", 8, 65, 8));
}

TEST_CASE("a hopper pulls one item out of the container above it") {
    Loaded loaded;
    WorldState& world = loaded.state;
    placeHopper(world, 8, 64, 8, empty(hoppers::kSize));
    placeContainer(world, 8, 65, 8, oneStack(27, 3, "stone", 2), "chest");

    CHECK(hoppers::tick(world, 8, 64, 8));

    CHECK(slotsOf(world, 8, 65, 8, 27)[3]->count == 1);
    const blockInventory::Slots inHopper = slotsOf(world, 8, 64, 8, hoppers::kSize);
    REQUIRE(inHopper[0].has_value());
    CHECK(inHopper[0]->item == item("stone"));
    CHECK(inHopper[0]->count == 1);
    CHECK(cooldownOf(world, 8, 64, 8) == 8);
}

TEST_CASE("the last item out of the container above leaves its slot empty") {
    Loaded loaded;
    WorldState& world = loaded.state;
    blockInventory::Slots inHopper = empty(hoppers::kSize);
    inHopper[0] = ItemStack{.item = item("stone"), .count = 7};
    placeHopper(world, 8, 64, 8, inHopper);
    placeContainer(world, 8, 65, 8, oneStack(27, 0, "stone", 1), "chest");

    CHECK(hoppers::tick(world, 8, 64, 8));

    CHECK_FALSE(slotsOf(world, 8, 65, 8, 27)[0].has_value());
    CHECK(slotsOf(world, 8, 64, 8, hoppers::kSize)[0]->count == 8);
}

TEST_CASE("pushing comes before pulling") {
    Loaded loaded;
    WorldState& world = loaded.state;
    placeHopper(world, 8, 64, 8, oneStack(hoppers::kSize, 0, "stone", 1));
    placeContainer(world, 8, 63, 8, empty(27), "chest");
    placeContainer(world, 8, 65, 8, oneStack(27, 0, "dirt", 4), "chest");

    CHECK(hoppers::tick(world, 8, 64, 8));

    CHECK(slotsOf(world, 8, 63, 8, 27)[0]->count == 1);
    // The chest on top is left alone, because one move a turn is all a hopper gets.
    CHECK(slotsOf(world, 8, 65, 8, 27)[0]->count == 4);
    CHECK_FALSE(slotsOf(world, 8, 64, 8, hoppers::kSize)[0].has_value());
}

TEST_CASE("a hopper whose slots are all taken pulls nothing") {
    Loaded loaded;
    WorldState& world = loaded.state;
    blockInventory::Slots inHopper = empty(hoppers::kSize);
    for (std::size_t slot = 0; slot < 5; ++slot) {
        inHopper[slot] = ItemStack{.item = item("dirt"), .count = 64};
    }
    placeHopper(world, 8, 64, 8, inHopper);
    placeContainer(world, 8, 65, 8, oneStack(27, 0, "stone", 4), "chest");

    CHECK_FALSE(hoppers::tick(world, 8, 64, 8));

    CHECK(slotsOf(world, 8, 65, 8, 27)[0]->count == 4);
    CHECK(cooldownOf(world, 8, 64, 8) == 0);
}

TEST_CASE("a furnace above gives its result and its fuel, never its ingredient") {
    const auto pulledFrom = [](const blockInventory::Slots& inFurnace) {
        Loaded loaded;
        WorldState& world = loaded.state;
        placeHopper(world, 8, 64, 8, empty(hoppers::kSize));
        placeContainer(world, 8, 65, 8, inFurnace, "furnace");
        const bool moved = hoppers::tick(world, 8, 64, 8);
        const blockInventory::Slots after = blockInventory::read(dataAt(world, 8, 64, 8), hoppers::kSize);
        return std::pair{moved, after[0].has_value() ? std::string{after[0]->item.path()} : std::string{}};
    };

    blockInventory::Slots all(3);
    all[0] = ItemStack{.item = item("raw_iron"), .count = 4};
    all[1] = ItemStack{.item = item("coal"), .count = 4};
    all[2] = ItemStack{.item = item("iron_ingot"), .count = 4};
    // parity: `AbstractFurnaceBlockEntity.SLOTS_FOR_DOWN` is {2, 1}: the result first, then the fuel.
    CHECK(pulledFrom(all) == std::pair{true, std::string{"iron_ingot"}});

    blockInventory::Slots noResult(3);
    noResult[0] = ItemStack{.item = item("raw_iron"), .count = 4};
    noResult[1] = ItemStack{.item = item("coal"), .count = 4};
    CHECK(pulledFrom(noResult) == std::pair{true, std::string{"coal"}});

    blockInventory::Slots onlyIngredient(3);
    onlyIngredient[0] = ItemStack{.item = item("raw_iron"), .count = 4};
    CHECK(pulledFrom(onlyIngredient) == std::pair{false, std::string{}});
}

TEST_CASE("a furnace takes ingredients from above and fuel from the side") {
    const auto insertedInto = [](const std::string& facing, std::int32_t hx, std::int32_t hy, std::int32_t hz) {
        Loaded loaded;
        WorldState& world = loaded.state;
        placeHopper(world, hx, hy, hz, oneStack(hoppers::kSize, 0, "coal", 2), facing);
        placeContainer(world, 8, 64, 8, empty(3), "furnace");
        REQUIRE(hoppers::tick(world, hx, hy, hz));
        const blockInventory::Slots after = blockInventory::read(dataAt(world, 8, 64, 8), 3);
        std::int32_t filled = -1;
        for (std::int32_t slot = 0; slot < 3; ++slot) {
            if (after[static_cast<std::size_t>(slot)].has_value()) {
                filled = slot;
            }
        }
        return filled;
    };

    // parity: `SLOTS_FOR_UP` is {0}, the ingredient, and every side is {1}, the fuel.
    CHECK(insertedInto("down", 8, 65, 8) == 0);
    CHECK(insertedInto("north", 8, 64, 9) == 1);
    CHECK(insertedInto("south", 8, 64, 7) == 1);
    CHECK(insertedInto("west", 9, 64, 8) == 1);
    CHECK(insertedInto("east", 7, 64, 8) == 1);
}

TEST_CASE("a hopper eats a whole stack lying above it") {
    Loaded loaded;
    WorldState& world = loaded.state;
    placeHopper(world, 8, 64, 8, empty(hoppers::kSize));
    world.spawnItem(DroppedItem{.entityId = 11, .item = item("stone"), .count = 3, .x = 8.5, .y = 64.75, .z = 8.5});

    CHECK(hoppers::tick(world, 8, 64, 8));

    const blockInventory::Slots inHopper = slotsOf(world, 8, 64, 8, hoppers::kSize);
    REQUIRE(inHopper[0].has_value());
    CHECK(inHopper[0]->item == item("stone"));
    CHECK(inHopper[0]->count == 3);
    CHECK(world.items().empty());
    CHECK(cooldownOf(world, 8, 64, 8) == 8);
}

TEST_CASE("what a hopper eats goes into the first slot that will take it") {
    Loaded loaded;
    WorldState& world = loaded.state;
    blockInventory::Slots inHopper = empty(hoppers::kSize);
    inHopper[1] = ItemStack{.item = item("stone"), .count = 5};
    placeHopper(world, 8, 64, 8, inHopper);
    world.spawnItem(DroppedItem{.entityId = 11, .item = item("stone"), .count = 2, .x = 8.5, .y = 64.75, .z = 8.5});

    CHECK(hoppers::tick(world, 8, 64, 8));

    // parity: `addItem` walks the slots in order, so an empty one before a matching stack wins.
    const blockInventory::Slots after = slotsOf(world, 8, 64, 8, hoppers::kSize);
    REQUIRE(after[0].has_value());
    CHECK(after[0]->count == 2);
    CHECK(after[1]->count == 5);
}

TEST_CASE("a hopper takes only what fits and leaves the rest lying") {
    Loaded loaded;
    WorldState& world = loaded.state;
    blockInventory::Slots inHopper = empty(hoppers::kSize);
    for (std::size_t slot = 0; slot < 4; ++slot) {
        inHopper[slot] = ItemStack{.item = item("dirt"), .count = 64};
    }
    inHopper[4] = ItemStack{.item = item("stone"), .count = 63};
    placeHopper(world, 8, 64, 8, inHopper);
    world.spawnItem(DroppedItem{.entityId = 11, .item = item("stone"), .count = 5, .x = 8.5, .y = 64.75, .z = 8.5});

    CHECK(hoppers::tick(world, 8, 64, 8));

    CHECK(slotsOf(world, 8, 64, 8, hoppers::kSize)[4]->count == 64);
    const std::vector<DroppedItem> left = world.items();
    REQUIRE(left.size() == 1);
    CHECK(left[0].entityId == 11);
    CHECK(left[0].count == 4);
    CHECK(left[0].countChanged);
}

TEST_CASE("a hopper with no room takes nothing off the floor") {
    Loaded loaded;
    WorldState& world = loaded.state;
    blockInventory::Slots inHopper = empty(hoppers::kSize);
    for (std::size_t slot = 0; slot < 5; ++slot) {
        inHopper[slot] = ItemStack{.item = item("dirt"), .count = 64};
    }
    placeHopper(world, 8, 64, 8, inHopper);
    world.spawnItem(DroppedItem{.entityId = 11, .item = item("stone"), .count = 5, .x = 8.5, .y = 64.75, .z = 8.5});

    CHECK_FALSE(hoppers::tick(world, 8, 64, 8));

    REQUIRE(world.items().size() == 1);
    CHECK(world.items()[0].count == 5);
    CHECK_FALSE(world.items()[0].countChanged);
    CHECK(cooldownOf(world, 8, 64, 8) == 0);
}

TEST_CASE("the box a hopper eats from is its own width, from eleven sixteenths up two blocks") {
    const auto sucks = [](double x, double y, double z) {
        Loaded loaded;
        WorldState& world = loaded.state;
        placeHopper(world, 8, 64, 8, empty(hoppers::kSize));
        world.spawnItem(DroppedItem{.entityId = 11, .item = item("stone"), .count = 1, .x = x, .y = y, .z = z});
        return hoppers::tick(world, 8, 64, 8);
    };

    // parity: `Hopper.SUCK_AABB`, from 11/16 of a block up to two whole blocks.
    CHECK(sucks(8.5, 64.0 + 11.0 / 16.0, 8.5));
    CHECK_FALSE(sucks(8.5, 64.0 + 10.0 / 16.0, 8.5));
    CHECK(sucks(8.5, 66.0, 8.5));
    CHECK_FALSE(sucks(8.5, 66.5, 8.5));

    // And its own width, which is closed at both ends.
    CHECK(sucks(8.0, 64.75, 8.5));
    CHECK_FALSE(sucks(7.75, 64.75, 8.5));
    CHECK(sucks(9.0, 64.75, 8.5));
    CHECK_FALSE(sucks(9.25, 64.75, 8.5));
    CHECK(sucks(8.5, 64.75, 8.0));
    CHECK_FALSE(sucks(8.5, 64.75, 7.75));
    CHECK(sucks(8.5, 64.75, 9.0));
    CHECK_FALSE(sucks(8.5, 64.75, 9.25));
}

TEST_CASE("a hopper eats every stack in its box in the one turn") {
    Loaded loaded;
    WorldState& world = loaded.state;
    placeHopper(world, 8, 64, 8, empty(hoppers::kSize));
    world.spawnItem(DroppedItem{.entityId = 11, .item = item("stone"), .count = 2, .x = 8.2, .y = 64.75, .z = 8.5});
    world.spawnItem(DroppedItem{.entityId = 12, .item = item("dirt"), .count = 1, .x = 8.8, .y = 65.5, .z = 8.5});
    // Out of the box, so it stays where it is.
    world.spawnItem(DroppedItem{.entityId = 13, .item = item("sand"), .count = 1, .x = 8.5, .y = 64.0, .z = 8.5});

    CHECK(hoppers::tick(world, 8, 64, 8));

    const blockInventory::Slots after = slotsOf(world, 8, 64, 8, hoppers::kSize);
    CHECK(after[0]->count == 2);
    CHECK(after[0]->item == item("stone"));
    CHECK(after[1]->count == 1);
    CHECK(after[1]->item == item("dirt"));
    const std::vector<DroppedItem> left = world.items();
    REQUIRE(left.size() == 1);
    CHECK(left[0].entityId == 13);
}

TEST_CASE("a hopper away from the middle chunk works in its own") {
    Loaded loaded;
    WorldState& world = loaded.state;
    world.getOrGenerateChunk(1, 1);
    placeHopper(world, 21, 64, 21, oneStack(hoppers::kSize, 0, "stone", 2));
    placeContainer(world, 21, 63, 21, empty(27), "chest");
    world.chunkIfLoaded(1, 1)->markClean();
    world.chunkIfLoaded(0, 0)->markClean();

    CHECK(hoppers::tick(world, 21, 64, 21));

    CHECK(slotsOf(world, 21, 63, 21, 27)[0]->count == 1);
    CHECK(slotsOf(world, 21, 64, 21, hoppers::kSize)[0]->count == 1);
    // The chunk the items landed in is the one that has to be written back.
    CHECK(world.chunkIfLoaded(1, 1)->isDirty());
    CHECK_FALSE(world.chunkIfLoaded(0, 0)->isDirty());
}

TEST_CASE("a hopper with nothing around it and nothing in it does nothing") {
    Loaded loaded;
    WorldState& world = loaded.state;
    placeHopper(world, 8, 64, 8, empty(hoppers::kSize));

    CHECK_FALSE(hoppers::tick(world, 8, 64, 8));

    CHECK(cooldownOf(world, 8, 64, 8) == 0);
}

TEST_CASE("anything that is not a hopper with a block entity is left alone") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(8, 64, 8, registry::blocks::named("stone")->stateId));
    CHECK_FALSE(hoppers::tick(world, 8, 64, 8));

    // A hopper whose block entity never got written.
    REQUIRE(world.setBlockState(4, 64, 4, hopper()));
    CHECK_FALSE(hoppers::tick(world, 4, 64, 4));

    // And a chunk nobody loaded.
    CHECK_FALSE(hoppers::tick(world, 300, 64, 300));
}

TEST_CASE("a hopper pushes into another hopper, which is five slots and not a chest") {
    Loaded loaded;
    WorldState& world = loaded.state;
    placeHopper(world, 8, 64, 8, oneStack(hoppers::kSize, 0, "stone", 2));
    placeHopper(world, 8, 63, 8, empty(hoppers::kSize));

    CHECK(hoppers::tick(world, 8, 64, 8));

    CHECK(slotsOf(world, 8, 63, 8, hoppers::kSize)[0]->count == 1);
    CHECK(slotsOf(world, 8, 64, 8, hoppers::kSize)[0]->count == 1);
}

TEST_CASE("a full slot is skipped over even when it holds the right item") {
    Loaded loaded;
    WorldState& world = loaded.state;
    blockInventory::Slots inHopper = empty(hoppers::kSize);
    inHopper[0] = ItemStack{.item = item("stone"), .count = 64};
    placeHopper(world, 8, 64, 8, inHopper);
    world.spawnItem(DroppedItem{.entityId = 11, .item = item("stone"), .count = 1, .x = 8.5, .y = 64.75, .z = 8.5});

    CHECK(hoppers::tick(world, 8, 64, 8));

    const blockInventory::Slots after = slotsOf(world, 8, 64, 8, hoppers::kSize);
    CHECK(after[0]->count == 64);
    REQUIRE(after[1].has_value());
    CHECK(after[1]->count == 1);
}

TEST_CASE("a single item left over stays on the floor") {
    Loaded loaded;
    WorldState& world = loaded.state;
    blockInventory::Slots inHopper = empty(hoppers::kSize);
    for (std::size_t slot = 0; slot < 4; ++slot) {
        inHopper[slot] = ItemStack{.item = item("dirt"), .count = 64};
    }
    inHopper[4] = ItemStack{.item = item("stone"), .count = 63};
    placeHopper(world, 8, 64, 8, inHopper);
    world.spawnItem(DroppedItem{.entityId = 11, .item = item("stone"), .count = 2, .x = 8.5, .y = 64.75, .z = 8.5});

    CHECK(hoppers::tick(world, 8, 64, 8));

    CHECK(slotsOf(world, 8, 64, 8, hoppers::kSize)[4]->count == 64);
    REQUIRE(world.items().size() == 1);
    CHECK(world.items()[0].count == 1);
}

TEST_CASE("a container that is not a hopper never takes a hopper's turn") {
    Loaded loaded;
    WorldState& world = loaded.state;
    // A chest has a facing and a block entity, which is everything a hopper's turn reads.
    placeContainer(world, 8, 64, 8, oneStack(27, 0, "stone", 4), "chest");
    placeContainer(world, 8, 64, 7, empty(27), "chest");

    CHECK_FALSE(hoppers::tick(world, 8, 64, 8));

    CHECK(slotsOf(world, 8, 64, 8, 27)[0]->count == 4);
    CHECK_FALSE(slotsOf(world, 8, 64, 7, 27)[0].has_value());
}
