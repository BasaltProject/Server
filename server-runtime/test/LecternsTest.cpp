#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/Lecterns.hpp>

#include <doctest.h>

#include <cstdint>
#include <map>
#include <string>

using namespace basalt::server::runtime::world;
using basalt::server::world::Chunk;
namespace registry = basalt::server::registry;

namespace {

[[nodiscard]] std::int32_t lectern(const std::string& hasBook, const std::string& powered) {
    const std::optional<std::int32_t> stateId = registry::blockStateDefinitions::stateIdOf(
        basalt::wire::common::Identifier::vanilla("lectern"),
        {{"facing", "north"}, {"has_book", hasBook}, {"powered", powered}}, std::nullopt);
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

/// A lectern with a book of that many pages already on it.
void withBook(WorldState& world, std::int32_t pages) {
    REQUIRE(world.setBlockState(8, 64, 8, lectern("false", "false")));
    REQUIRE(lecterns::placeBook(world, 8, 64, 8, item("written_book"), pages).has_value());
}

} // namespace

TEST_CASE("a lectern takes a book and nothing else") {
    CHECK(lecterns::isBook(item("written_book")));
    CHECK(lecterns::isBook(item("writable_book")));
    CHECK_FALSE(lecterns::isBook(item("book")));
    CHECK_FALSE(lecterns::isBook(item("enchanted_book")));
}

TEST_CASE("a book goes on the stand and comes back off it") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(8, 64, 8, lectern("false", "false")));

    const std::optional<BlockChange> placed = lecterns::placeBook(world, 8, 64, 8, item("written_book"), 5);
    REQUIRE(placed.has_value());
    CHECK(placed->stateId == lectern("true", "false"));
    CHECK(dataAt(world, 8, 64, 8).getInt("Pages") == 5);
    CHECK(dataAt(world, 8, 64, 8).getInt("Page") == 0);

    // A stand that already holds one takes no second book.
    CHECK_FALSE(lecterns::placeBook(world, 8, 64, 8, item("writable_book"), 2).has_value());

    const std::optional<std::pair<BlockChange, basalt::wire::common::Identifier>> taken =
        lecterns::takeBook(world, 8, 64, 8);
    REQUIRE(taken.has_value());
    CHECK(taken->first.stateId == lectern("false", "false"));
    CHECK(taken->second == item("written_book"));
    CHECK(dataAt(world, 8, 64, 8).getString("Book").empty());
    CHECK_FALSE(lecterns::takeBook(world, 8, 64, 8).has_value());
}

TEST_CASE("a book of no pages is still a page long") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(8, 64, 8, lectern("false", "false")));

    REQUIRE(lecterns::placeBook(world, 8, 64, 8, item("writable_book"), 0).has_value());

    CHECK(dataAt(world, 8, 64, 8).getInt("Pages") == 1);
}

TEST_CASE("turning a page pulses the block for two ticks") {
    Loaded loaded;
    WorldState& world = loaded.state;
    withBook(world, 8);

    const std::optional<BlockChange> turned = lecterns::turnPage(world, 8, 64, 8, 3);
    REQUIRE(turned.has_value());
    CHECK(turned->stateId == lectern("true", "true"));
    CHECK(dataAt(world, 8, 64, 8).getInt("Page") == 3);

    CHECK(world.dueBlockTicks().empty());
    CHECK(world.dueBlockTicks().size() == 1);

    const std::optional<BlockChange> quiet = lecterns::endPulse(world, 8, 64, 8);
    REQUIRE(quiet.has_value());
    CHECK(quiet->stateId == lectern("true", "false"));
    // A lectern that is already quiet has no pulse left to end.
    CHECK_FALSE(lecterns::endPulse(world, 8, 64, 8).has_value());
}

TEST_CASE("a page past the covers stops at the last one") {
    Loaded loaded;
    WorldState& world = loaded.state;
    withBook(world, 4);

    REQUIRE(lecterns::turnPage(world, 8, 64, 8, 99).has_value());
    CHECK(dataAt(world, 8, 64, 8).getInt("Page") == 3);

    REQUIRE(lecterns::endPulse(world, 8, 64, 8).has_value());
    REQUIRE(lecterns::turnPage(world, 8, 64, 8, -4).has_value());
    CHECK(dataAt(world, 8, 64, 8).getInt("Page") == 0);
}

TEST_CASE("an empty lectern turns no pages") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(8, 64, 8, lectern("false", "false")));

    CHECK_FALSE(lecterns::turnPage(world, 8, 64, 8, 1).has_value());
    REQUIRE(world.setBlockState(4, 64, 4, registry::blocks::named("stone")->stateId));
    CHECK_FALSE(lecterns::placeBook(world, 4, 64, 4, item("written_book"), 1).has_value());
    CHECK_FALSE(lecterns::takeBook(world, 4, 64, 4).has_value());
    CHECK_FALSE(lecterns::endPulse(world, 4, 64, 4).has_value());
}

TEST_CASE("the comparator reads how far through the book the reader is") {
    Loaded loaded;
    WorldState& world = loaded.state;
    withBook(world, 15);

    // parity: the first page is one and the last is fifteen, with the fourteen steps between.
    const std::array<std::int32_t, 4> pages{0, 7, 13, 14};
    const std::array<std::int32_t, 4> wanted{1, 8, 14, 15};
    for (std::size_t index = 0; index < pages.size(); ++index) {
        // The pulse from the turn before has to end before the next one can start.
        static_cast<void>(lecterns::endPulse(world, 8, 64, 8));
        REQUIRE(lecterns::turnPage(world, 8, 64, 8, pages[index]).has_value());
        CHECK(lecterns::comparatorSignal(dataAt(world, 8, 64, 8)) == wanted[index]);
    }
}

TEST_CASE("a lectern away from the middle chunk keeps its own book") {
    Loaded loaded;
    WorldState& world = loaded.state;
    world.getOrGenerateChunk(1, 1);
    // Twenty-one is in the next chunk over and its low bits are odd, so both halves of the split
    // have to be right for the book to land on the block.
    REQUIRE(world.setBlockState(21, 64, 21, lectern("false", "false")));

    REQUIRE(lecterns::placeBook(world, 21, 64, 21, item("written_book"), 3).has_value());
    CHECK(dataAt(world, 21, 64, 21).getInt("Pages") == 3);
    REQUIRE(lecterns::turnPage(world, 21, 64, 21, 2).has_value());
    CHECK(dataAt(world, 21, 64, 21).getInt("Page") == 2);
    CHECK(lecterns::takeBook(world, 21, 64, 21).has_value());
}

TEST_CASE("a book of one page has only the page it is open at") {
    Loaded loaded;
    WorldState& world = loaded.state;
    withBook(world, 1);

    REQUIRE(lecterns::turnPage(world, 8, 64, 8, 1).has_value());

    CHECK(dataAt(world, 8, 64, 8).getInt("Page") == 0);
}

TEST_CASE("a stand with no book reads as nothing, and a single page reads as full") {
    basalt::wire::nbt::NbtCompound empty;
    CHECK(lecterns::comparatorSignal(empty) == 14);

    basalt::wire::nbt::NbtCompound single;
    single.putString("Book", "minecraft:written_book");
    single.putInt("Pages", 1);
    single.putInt("Page", 0);
    CHECK(lecterns::comparatorSignal(single) == 15);

    // Two pages is the shortest book with a first half and a second.
    basalt::wire::nbt::NbtCompound pair;
    pair.putString("Book", "minecraft:written_book");
    pair.putInt("Pages", 2);
    pair.putInt("Page", 0);
    CHECK(lecterns::comparatorSignal(pair) == 1);
    pair.putInt("Page", 1);
    CHECK(lecterns::comparatorSignal(pair) == 15);

    // A book that never said which page it was open at is open at the first.
    basalt::wire::nbt::NbtCompound unopened;
    unopened.putString("Book", "minecraft:written_book");
    unopened.putInt("Pages", 5);
    CHECK(lecterns::comparatorSignal(unopened) == 1);
}
