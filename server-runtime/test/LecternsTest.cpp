#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/BlockInventory.hpp>
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

/// A book with that many pages, written the way vanilla writes one: the pages are a component of
/// the item, not a field of the lectern.
[[nodiscard]] basalt::server::world::loot::ItemStack book(const std::string& path, std::int32_t pages) {
    basalt::wire::nbt::NbtList written{basalt::wire::nbt::NbtType::String};
    for (std::int32_t page = 0; page < pages; ++page) {
        written.push(basalt::wire::nbt::NbtTag::ofString("page"));
    }
    basalt::wire::nbt::NbtCompound content;
    content.set("pages", basalt::wire::nbt::NbtTag::ofList(std::move(written)));
    basalt::server::world::loot::ItemStack stack{.item = item(path), .count = 1};
    const std::string key =
        path == "written_book" ? "minecraft:written_book_content" : "minecraft:writable_book_content";
    stack.components.set(key, basalt::wire::nbt::NbtTag::ofCompound(std::move(content)));
    return stack;
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
    REQUIRE(lecterns::placeBook(world, 8, 64, 8, book("written_book", pages)).has_value());
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

    const std::optional<BlockChange> placed = lecterns::placeBook(world, 8, 64, 8, book("written_book", 5));
    REQUIRE(placed.has_value());
    CHECK(placed->stateId == lectern("true", "false"));
    CHECK(dataAt(world, 8, 64, 8).getInt("Page") == 0);
    // parity: `saveAdditional` stores the book with `ItemStack.CODEC`, components and all, which is
    // where the pages live.
    const basalt::wire::nbt::NbtCompound* stored = dataAt(world, 8, 64, 8).getCompound("Book");
    REQUIRE(stored != nullptr);
    CHECK(stored->getString("id") == "minecraft:written_book");
    CHECK(stored->getCompound("components") != nullptr);

    // A stand that already holds one takes no second book.
    CHECK_FALSE(lecterns::placeBook(world, 8, 64, 8, book("writable_book", 2)).has_value());

    const std::optional<std::pair<BlockChange, basalt::server::world::loot::ItemStack>> taken =
        lecterns::takeBook(world, 8, 64, 8);
    REQUIRE(taken.has_value());
    CHECK(taken->first.stateId == lectern("false", "false"));
    CHECK(taken->second.item == item("written_book"));
    // The book comes back off the stand with its pages still on it.
    CHECK(taken->second.components.getCompound("minecraft:written_book_content") != nullptr);
    CHECK(dataAt(world, 8, 64, 8).getCompound("Book") == nullptr);
    CHECK_FALSE(lecterns::takeBook(world, 8, 64, 8).has_value());
}

TEST_CASE("a book with no pages reads as fully read") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(8, 64, 8, lectern("false", "false")));

    REQUIRE(lecterns::placeBook(world, 8, 64, 8, book("writable_book", 0)).has_value());

    // parity: `getPageCount` answers zero, and `getRedstoneSignal` reads anything under two pages as
    // the whole book.
    CHECK(lecterns::comparatorSignal(dataAt(world, 8, 64, 8)) == 15);
}

TEST_CASE("turning a page pulses the block for two ticks") {
    Loaded loaded;
    WorldState& world = loaded.state;
    withBook(world, 8);

    const std::optional<BlockChange> turned = lecterns::turnPage(world, 8, 64, 8, 3);
    REQUIRE(turned.has_value());
    CHECK(turned->stateId == lectern("true", "true"));
    CHECK(dataAt(world, 8, 64, 8).getInt("Page") == 3);

    world.openTurn();
    CHECK(world.dueBlockTicks().empty());
    world.openTurn();
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
    CHECK_FALSE(lecterns::placeBook(world, 4, 64, 4, book("written_book", 1)).has_value());
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

    REQUIRE(lecterns::placeBook(world, 21, 64, 21, book("written_book", 3)).has_value());
    CHECK(dataAt(world, 21, 64, 21).getCompound("Book") != nullptr);
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
    const auto stand = [](std::int32_t pages, std::optional<std::int32_t> page) {
        basalt::wire::nbt::NbtCompound data;
        data.set("Book",
                 basalt::wire::nbt::NbtTag::ofCompound(blockInventory::writeStack(book("written_book", pages))));
        if (page.has_value()) {
            data.putInt("Page", *page);
        }
        return data;
    };
    const basalt::wire::nbt::NbtCompound empty;
    CHECK(lecterns::comparatorSignal(empty) == 14);

    CHECK(lecterns::comparatorSignal(stand(1, 0)) == 15);

    // Two pages is the shortest book with a first half and a second.
    CHECK(lecterns::comparatorSignal(stand(2, 0)) == 1);
    CHECK(lecterns::comparatorSignal(stand(2, 1)) == 15);

    // A book that never said which page it was open at is open at the first.
    CHECK(lecterns::comparatorSignal(stand(5, std::nullopt)) == 1);
}
