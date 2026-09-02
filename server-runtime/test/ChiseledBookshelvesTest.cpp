#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/BlockInventory.hpp>
#include <basalt/server/runtime/world/ChiseledBookshelves.hpp>

#include <doctest.h>

#include <map>
#include <string>

using namespace basalt::server::runtime::world;
using basalt::server::world::Chunk;
namespace registry = basalt::server::registry;

namespace {

[[nodiscard]] std::int32_t shelf(const std::map<std::string, std::string>& occupied) {
    std::map<std::string, std::string> properties{{"facing", "north"}};
    for (std::int32_t slot = 0; slot < chiseledBookshelves::kSize; ++slot) {
        properties.emplace("slot_" + std::to_string(slot) + "_occupied", "false");
    }
    for (const auto& [key, value] : occupied) {
        properties[key] = value;
    }
    const std::optional<std::int32_t> stateId = registry::blockStateDefinitions::stateIdOf(
        basalt::wire::common::Identifier::vanilla("chiseled_bookshelf"), properties, std::nullopt);
    REQUIRE(stateId.has_value());
    return *stateId;
}

[[nodiscard]] basalt::wire::common::Identifier item(const std::string& path) {
    return basalt::wire::common::Identifier::vanilla(path);
}

[[nodiscard]] WorldState emptyWorld() {
    return WorldState{{}, [](std::int32_t chunkX, std::int32_t chunkZ) { return Chunk{chunkX, chunkZ}; }};
}

struct Loaded {
    WorldState state = emptyWorld();

    Loaded() { state.getOrGenerateChunk(0, 0); }
};

[[nodiscard]] const basalt::wire::nbt::NbtCompound& dataAt(WorldState& world, std::int32_t x, std::int32_t y,
                                                           std::int32_t z) {
    const basalt::server::world::Chunk* chunk = world.chunkIfLoaded(x >> 4, z >> 4);
    REQUIRE(chunk != nullptr);
    const basalt::server::world::ChunkBlockEntity* entity = chunk->blockEntityAt(x & 15, y, z & 15);
    REQUIRE(entity != nullptr);
    return entity->data();
}

[[nodiscard]] std::string soundOf(const BlockChange& change) {
    REQUIRE(change.effects.size() == 1);
    const blockEffect::Sound* sound = std::get_if<blockEffect::Sound>(&change.effects[0]);
    REQUIRE(sound != nullptr);
    return sound->name;
}

} // namespace

TEST_CASE("only the chiseled bookshelf is one") {
    CHECK(chiseledBookshelves::isBookshelf("chiseled_bookshelf"));
    CHECK_FALSE(chiseledBookshelves::isBookshelf("bookshelf"));
    CHECK_FALSE(chiseledBookshelves::isBookshelf(""));
}

TEST_CASE("a book goes into the slot the click landed on and comes back out of it") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(8, 64, 8, shelf({})));

    const std::optional<chiseledBookshelves::Interaction> put =
        chiseledBookshelves::putBook(world, 8, 64, 8, 4, item("book"));
    REQUIRE(put.has_value());
    CHECK_FALSE(put->taken.has_value());
    CHECK(soundOf(put->change) == "block.chiseled_bookshelf.insert");
    CHECK(world.blockState(8, 64, 8) == shelf({{"slot_4_occupied", "true"}}));
    REQUIRE(blockInventory::read(dataAt(world, 8, 64, 8), 6)[4].has_value());
    CHECK(blockInventory::read(dataAt(world, 8, 64, 8), 6)[4]->item == item("book"));
    // A shelf holds one book to a slot, never a stack.
    CHECK(blockInventory::read(dataAt(world, 8, 64, 8), 6)[4]->count == 1);

    // The same slot takes no second book.
    CHECK_FALSE(chiseledBookshelves::putBook(world, 8, 64, 8, 4, item("book")).has_value());

    const std::optional<chiseledBookshelves::Interaction> taken = chiseledBookshelves::takeBook(world, 8, 64, 8, 4);
    REQUIRE(taken.has_value());
    CHECK(taken->taken == item("book"));
    CHECK(soundOf(taken->change) == "block.chiseled_bookshelf.pickup");
    CHECK(world.blockState(8, 64, 8) == shelf({}));
    CHECK_FALSE(blockInventory::read(dataAt(world, 8, 64, 8), 6)[4].has_value());
    CHECK_FALSE(chiseledBookshelves::takeBook(world, 8, 64, 8, 4).has_value());
}

TEST_CASE("an enchanted book sounds different going in and coming out") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(8, 64, 8, shelf({})));

    const std::optional<chiseledBookshelves::Interaction> put =
        chiseledBookshelves::putBook(world, 8, 64, 8, 0, item("enchanted_book"));
    REQUIRE(put.has_value());
    CHECK(soundOf(put->change) == "block.chiseled_bookshelf.insert.enchanted");

    const std::optional<chiseledBookshelves::Interaction> taken = chiseledBookshelves::takeBook(world, 8, 64, 8, 0);
    REQUIRE(taken.has_value());
    CHECK(soundOf(taken->change) == "block.chiseled_bookshelf.pickup.enchanted");
    CHECK(taken->taken == item("enchanted_book"));
}

TEST_CASE("a slot the shelf does not have is no slot at all") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(8, 64, 8, shelf({})));

    CHECK_FALSE(chiseledBookshelves::putBook(world, 8, 64, 8, -1, item("book")).has_value());
    CHECK_FALSE(chiseledBookshelves::putBook(world, 8, 64, 8, 6, item("book")).has_value());
    CHECK_FALSE(chiseledBookshelves::takeBook(world, 8, 64, 8, 6).has_value());

    REQUIRE(world.setBlockState(4, 64, 4, registry::blocks::named("stone")->stateId));
    CHECK_FALSE(chiseledBookshelves::putBook(world, 4, 64, 4, 0, item("book")).has_value());
    CHECK_FALSE(chiseledBookshelves::putBook(world, 300, 64, 300, 0, item("book")).has_value());
}

TEST_CASE("the comparator reads the slot that was touched last") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(8, 64, 8, shelf({})));

    // An untouched shelf reads nothing at all.
    basalt::wire::nbt::NbtCompound untouched;
    CHECK(chiseledBookshelves::comparatorSignal(untouched) == 0);

    for (const std::int32_t slot : {0, 3, 5, 1}) {
        REQUIRE(chiseledBookshelves::putBook(world, 8, 64, 8, slot, item("book")).has_value());
        CHECK(chiseledBookshelves::comparatorSignal(dataAt(world, 8, 64, 8)) == slot + 1);
    }
    // And taking one out counts as touching it too.
    REQUIRE(chiseledBookshelves::takeBook(world, 8, 64, 8, 3).has_value());
    CHECK(chiseledBookshelves::comparatorSignal(dataAt(world, 8, 64, 8)) == 4);
}

TEST_CASE("a shelf away from the middle chunk keeps its own books") {
    Loaded loaded;
    WorldState& world = loaded.state;
    world.getOrGenerateChunk(1, 1);
    REQUIRE(world.setBlockState(21, 64, 21, shelf({})));

    REQUIRE(chiseledBookshelves::putBook(world, 21, 64, 21, 2, item("book")).has_value());
    REQUIRE(chiseledBookshelves::putBook(world, 21, 64, 21, 5, item("enchanted_book")).has_value());

    // The second book has to find the first one's shelf, not a fresh one.
    const blockInventory::Slots slots = blockInventory::read(dataAt(world, 21, 64, 21), 6);
    REQUIRE(slots[2].has_value());
    CHECK(slots[2]->item == item("book"));
    REQUIRE(slots[5].has_value());
    CHECK(slots[5]->item == item("enchanted_book"));
    CHECK(chiseledBookshelves::comparatorSignal(dataAt(world, 21, 64, 21)) == 6);

    const std::optional<chiseledBookshelves::Interaction> taken = chiseledBookshelves::takeBook(world, 21, 64, 21, 2);
    REQUIRE(taken.has_value());
    CHECK(taken->taken == item("book"));
    CHECK(chiseledBookshelves::comparatorSignal(dataAt(world, 21, 64, 21)) == 3);
}

TEST_CASE("the click on the front picks the slot, and one anywhere else picks none") {
    const std::int32_t front = shelf({});

    // Two rows of three, read from the top left of the face it is looking out of.
    CHECK(chiseledBookshelves::slotAt(front, "north", 0.8F, 0.9F, 0.0F) == 0);
    CHECK(chiseledBookshelves::slotAt(front, "north", 0.5F, 0.9F, 0.0F) == 1);
    CHECK(chiseledBookshelves::slotAt(front, "north", 0.1F, 0.1F, 0.0F) == 5);
    CHECK_FALSE(chiseledBookshelves::slotAt(front, "up", 0.5F, 1.0F, 0.5F).has_value());
    CHECK_FALSE(chiseledBookshelves::slotAt(front, "south", 0.5F, 0.5F, 1.0F).has_value());
}
