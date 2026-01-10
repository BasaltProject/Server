#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/BlockInventory.hpp>
#include <basalt/server/runtime/world/Containers.hpp>

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

[[nodiscard]] std::int32_t stateOf(const std::string& path, const std::map<std::string, std::string>& properties = {}) {
    const std::optional<std::int32_t> stateId = registry::blockStateDefinitions::stateIdOf(
        basalt::wire::common::Identifier::vanilla(path), properties, std::nullopt);
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

/// Puts a container down with those slots filled, and answers what a comparator reads off it.
[[nodiscard]] std::optional<std::int32_t> signalOf(WorldState& world, const std::string& path,
                                                   const blockInventory::Slots& slots,
                                                   const containers::Readers& readers = {}) {
    REQUIRE(world.setBlockState(8, 64, 8, stateOf(path)));
    nbt::NbtCompound data;
    blockInventory::write(data, slots);
    world.chunkIfLoaded(0, 0)->setBlockEntity(basalt::server::world::ChunkBlockEntity{
        8, 64, 8, basalt::wire::common::Identifier::vanilla(path), std::move(data)});
    return containers::analogSignal(world, 8, 64, 8, {}, readers);
}

} // namespace

TEST_CASE("every container this build keeps says how many slots it has") {
    CHECK(containers::sizeOf("chest") == 27);
    CHECK(containers::sizeOf("waxed_oxidized_copper_chest") == 27);
    CHECK(containers::sizeOf("barrel") == 27);
    CHECK(containers::sizeOf("shulker_box") == 27);
    CHECK(containers::sizeOf("red_shulker_box") == 27);
    CHECK(containers::sizeOf("hopper") == 5);
    CHECK(containers::sizeOf("furnace") == 3);
    CHECK(containers::sizeOf("blast_furnace") == 3);
    CHECK(containers::sizeOf("smoker") == 3);
    CHECK(containers::sizeOf("dispenser") == 9);
    CHECK(containers::sizeOf("dropper") == 9);
    CHECK(containers::sizeOf("jukebox") == 1);
    CHECK(containers::sizeOf("lectern") == 1);
    CHECK(containers::sizeOf("brewing_stand") == 5);
    CHECK(containers::sizeOf("crafter") == 9);
    CHECK(containers::sizeOf("chiseled_bookshelf") == 6);
    CHECK(containers::sizeOf("decorated_pot") == 1);
    CHECK_FALSE(containers::sizeOf("stone").has_value());
}

TEST_CASE("the plain holders are the ones with no machinery of their own") {
    CHECK(containers::isPlain("chest"));
    CHECK(containers::isPlain("trapped_chest"));
    CHECK(containers::isPlain("barrel"));
    CHECK(containers::isPlain("hopper"));
    CHECK(containers::isPlain("dispenser"));
    CHECK(containers::isPlain("blue_shulker_box"));
    CHECK_FALSE(containers::isPlain("furnace"));
    CHECK_FALSE(containers::isPlain("crafter"));
    CHECK_FALSE(containers::isPlain("stone"));
}

TEST_CASE("a comparator reads how full a chest is") {
    Loaded loaded;
    WorldState& world = loaded.state;

    CHECK(signalOf(world, "chest", blockInventory::Slots(27)) == 0);

    blockInventory::Slots one(27);
    one[0] = ItemStack{.item = basalt::wire::common::Identifier::vanilla("stone"), .count = 1};
    CHECK(signalOf(world, "chest", one) == 1);

    blockInventory::Slots full(27);
    for (std::size_t slot = 0; slot < full.size(); ++slot) {
        full[slot] = ItemStack{.item = basalt::wire::common::Identifier::vanilla("stone"), .count = 64};
    }
    CHECK(signalOf(world, "chest", full) == 15);
}

TEST_CASE("one item in a furnace already reads as one") {
    Loaded loaded;
    WorldState& world = loaded.state;
    blockInventory::Slots one(3);
    one[0] = ItemStack{.item = basalt::wire::common::Identifier::vanilla("coal"), .count = 1};

    // parity: the quirk people build with: three slots make one item a whole fifteenth already.
    CHECK(signalOf(world, "furnace", one) == 1);
}

TEST_CASE("the blocks that carry the number in their own state need no contents") {
    Loaded loaded;
    WorldState& world = loaded.state;
    const auto readingOf = [&](const std::string& path, const std::map<std::string, std::string>& properties) {
        REQUIRE(world.setBlockState(8, 64, 8, stateOf(path, properties)));
        return containers::analogSignal(world, 8, 64, 8);
    };

    CHECK(readingOf("cake", {{"bites", "0"}}) == 14);
    CHECK(readingOf("cake", {{"bites", "3"}}) == 8);
    CHECK(readingOf("candle_cake", {{"lit", "false"}}) == 14);
    CHECK(readingOf("composter", {{"level", "5"}}) == 5);
    CHECK(readingOf("water_cauldron", {{"level", "2"}}) == 2);
    CHECK(readingOf("lava_cauldron", {}) == 3);
    CHECK(readingOf("respawn_anchor", {{"charges", "0"}}) == 0);
    CHECK(readingOf("respawn_anchor", {{"charges", "2"}}) == 7);
    CHECK(readingOf("respawn_anchor", {{"charges", "4"}}) == 15);
    CHECK(readingOf("end_portal_frame", {{"eye", "true"}, {"facing", "north"}}) == 15);
    CHECK(readingOf("end_portal_frame", {{"eye", "false"}, {"facing", "north"}}) == 0);
    CHECK(readingOf("beehive", {{"facing", "north"}, {"honey_level", "3"}}) == 3);
    CHECK(readingOf("copper_bulb", {{"lit", "true"}, {"powered", "false"}}) == 15);
    CHECK(readingOf("copper_bulb", {{"lit", "false"}, {"powered", "false"}}) == 0);

    // parity: one per pose, in the order the enum declares them.
    const std::array<std::string, 4> poses{"standing", "sitting", "running", "star"};
    for (std::size_t index = 0; index < poses.size(); ++index) {
        CHECK(readingOf("copper_golem_statue",
                        {{"facing", "north"}, {"copper_golem_pose", poses[index]}, {"waterlogged", "false"}}) ==
              static_cast<std::int32_t>(index) + 1);
    }
}

TEST_CASE("how full a slot is counts, not only whether it is taken") {
    Loaded loaded;
    WorldState& world = loaded.state;
    const auto withCount = [&](std::int32_t count) {
        blockInventory::Slots slots(3);
        slots[0] = ItemStack{.item = basalt::wire::common::Identifier::vanilla("coal"), .count = count};
        return signalOf(world, "furnace", slots);
    };

    // parity: the fraction of a full stack in every slot, averaged and walked up to fifteen.
    CHECK(withCount(1) == 1);
    CHECK(withCount(32) == 3);
    CHECK(withCount(64) == 5);
}

TEST_CASE("a container away from the middle chunk is read where it stands") {
    Loaded loaded;
    WorldState& world = loaded.state;
    world.getOrGenerateChunk(1, 1);
    REQUIRE(world.setBlockState(21, 64, 21, stateOf("barrel", {{"facing", "north"}, {"open", "false"}})));
    blockInventory::Slots slots(27);
    for (std::size_t slot = 0; slot < slots.size(); ++slot) {
        slots[slot] = ItemStack{.item = basalt::wire::common::Identifier::vanilla("stone"), .count = 64};
    }
    nbt::NbtCompound data;
    blockInventory::write(data, slots);
    world.chunkIfLoaded(1, 1)->setBlockEntity(basalt::server::world::ChunkBlockEntity{
        5, 64, 5, basalt::wire::common::Identifier::vanilla("barrel"), std::move(data)});

    CHECK(containers::analogSignal(world, 21, 64, 21) == 15);
}

TEST_CASE("a block that is no container at all answers nothing") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(8, 64, 8, stateOf("stone")));

    CHECK_FALSE(containers::analogSignal(world, 8, 64, 8).has_value());
    CHECK_FALSE(containers::analogSignal(world, 300, 64, 300).has_value());
}

TEST_CASE("a container with no block entity yet reads as empty, not as nothing") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(8, 64, 8,
                                stateOf("chest", {{"facing", "north"}, {"type", "single"}, {"waterlogged", "false"}})));

    CHECK(containers::analogSignal(world, 8, 64, 8) == 0);
}

TEST_CASE("the two blocks that read themselves are asked to") {
    Loaded loaded;
    WorldState& world = loaded.state;
    std::int32_t shelfAsks = 0;
    std::int32_t crafterAsks = 0;
    const containers::Readers readers{
        .shelf =
            [&](std::int32_t, std::int32_t, std::int32_t, std::int32_t, std::string_view from) {
                ++shelfAsks;
                return from == "north" ? std::optional{4} : std::nullopt;
            },
        .crafter =
            [&](const nbt::NbtCompound&) {
                ++crafterAsks;
                return 6;
            }};

    CHECK(signalOf(world, "crafter", blockInventory::Slots(9), readers) == 6);
    CHECK(crafterAsks == 1);
    CHECK(shelfAsks == 1);

    // The shelf answers for itself when it is one, whatever the block is called.
    REQUIRE(world.setBlockState(4, 64, 4, stateOf("stone")));
    CHECK(containers::analogSignal(world, 4, 64, 4, "north", readers) == 4);
    CHECK_FALSE(containers::analogSignal(world, 4, 64, 4, "south", readers).has_value());
}

TEST_CASE("a lectern with no book reads nothing, whatever page it remembers") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(
        8, 64, 8, stateOf("lectern", {{"facing", "north"}, {"has_book", "false"}, {"powered", "false"}})));
    nbt::NbtCompound data;
    data.putString("Book", "minecraft:written_book");
    data.putInt("Pages", 4);
    data.putInt("Page", 3);
    world.chunkIfLoaded(0, 0)->setBlockEntity(
        basalt::server::world::ChunkBlockEntity{8, 64, 8, basalt::wire::common::Identifier::vanilla("lectern"), data});

    CHECK(containers::analogSignal(world, 8, 64, 8) == 0);

    REQUIRE(world.setBlockState(8, 64, 8,
                                stateOf("lectern", {{"facing", "north"}, {"has_book", "true"}, {"powered", "false"}})));
    CHECK(containers::analogSignal(world, 8, 64, 8) == 15);
}
