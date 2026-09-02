#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/Shelves.hpp>

#include <doctest.h>

#include <map>
#include <set>
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

[[nodiscard]] std::int32_t shelf(const std::string& facing, const std::string& powered, const std::string& sideChain) {
    const std::optional<std::int32_t> stateId = registry::blockStateDefinitions::stateIdOf(
        item("oak_shelf"), {{"facing", facing}, {"powered", powered}, {"side_chain", sideChain}}, std::nullopt);
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

[[nodiscard]] const nbt::NbtCompound* dataAt(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const basalt::server::world::Chunk* chunk = world.chunkIfLoaded(x >> 4, z >> 4);
    REQUIRE(chunk != nullptr);
    const basalt::server::world::ChunkBlockEntity* entity = chunk->blockEntityAt(x & 15, y, z & 15);
    return entity == nullptr ? nullptr : &entity->data();
}

[[nodiscard]] std::string soundOf(const std::vector<BlockEffect>& effects) {
    REQUIRE(effects.size() == 1);
    const blockEffect::Sound* sound = std::get_if<blockEffect::Sound>(&effects[0]);
    REQUIRE(sound != nullptr);
    return sound->name;
}

[[nodiscard]] std::string partAt(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    REQUIRE(stateId.has_value());
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(*stateId);
    const auto found = properties.find("side_chain");
    return found == properties.end() ? std::string{} : found->second;
}

} // namespace

TEST_CASE("every shelf is a shelf but the chiseled bookshelf") {
    CHECK(shelves::isShelf("oak_shelf"));
    CHECK(shelves::isShelf("bamboo_shelf"));
    CHECK_FALSE(shelves::isShelf("chiseled_bookshelf"));
    CHECK_FALSE(shelves::isShelf("bookshelf"));
    CHECK_FALSE(shelves::isShelf("shelf"));
    CHECK_FALSE(shelves::isShelf(""));
}

TEST_CASE("a hand and a slot trade whatever each of them holds") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(8, 64, 8, shelf("north", "false", "unconnected")));

    // An empty hand on an empty slot is not a swap at all.
    CHECK_FALSE(shelves::swap(world, 8, 64, 8, 1, std::nullopt).has_value());

    const std::optional<shelves::SwapResult> placed =
        shelves::swap(world, 8, 64, 8, 1, ItemStack{.item = item("stone"), .count = 4});
    REQUIRE(placed.has_value());
    CHECK_FALSE(placed->removed.has_value());
    CHECK(soundOf(placed->effects) == "block.shelf.place_item");

    const std::optional<shelves::SwapResult> exchanged =
        shelves::swap(world, 8, 64, 8, 1, ItemStack{.item = item("dirt"), .count = 1});
    REQUIRE(exchanged.has_value());
    REQUIRE(exchanged->removed.has_value());
    CHECK(exchanged->removed->item == item("stone"));
    CHECK(exchanged->removed->count == 4);
    CHECK(soundOf(exchanged->effects) == "block.shelf.single_swap");

    const std::optional<shelves::SwapResult> taken = shelves::swap(world, 8, 64, 8, 1, std::nullopt);
    REQUIRE(taken.has_value());
    REQUIRE(taken->removed.has_value());
    CHECK(taken->removed->item == item("dirt"));
    CHECK(soundOf(taken->effects) == "block.shelf.take_item");
}

TEST_CASE("a slot the shelf does not have is no slot") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(8, 64, 8, shelf("north", "false", "unconnected")));

    CHECK_FALSE(shelves::swap(world, 8, 64, 8, -1, ItemStack{.item = item("stone"), .count = 1}).has_value());
    CHECK_FALSE(shelves::swap(world, 8, 64, 8, 3, ItemStack{.item = item("stone"), .count = 1}).has_value());
    REQUIRE(world.setBlockState(4, 64, 4, registry::blocks::named("stone")->stateId));
    CHECK_FALSE(shelves::swap(world, 4, 64, 4, 0, ItemStack{.item = item("stone"), .count = 1}).has_value());
}

TEST_CASE("an unpowered shelf is a chain of nothing") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(8, 64, 8, shelf("north", "false", "unconnected")));

    CHECK(shelves::chainAt(world, 8, 64, 8).empty());
}

TEST_CASE("powering a shelf joins it to the ones beside it, up to three") {
    Loaded loaded;
    WorldState& world = loaded.state;
    // Three shelves in a row, all facing north, so their sides are east and west.
    for (std::int32_t x = 7; x <= 9; ++x) {
        REQUIRE(world.setBlockState(x, 64, 8, shelf("north", "false", "unconnected")));
    }
    REQUIRE(world.setBlockState(8, 65, 8, registry::blocks::named("redstone_block")->stateId));
    std::vector<BlockChange> changes;

    shelves::reactToSignal(world, "overworld", 8, 64, 8, changes);

    REQUIRE_FALSE(changes.empty());
    CHECK(soundOf(changes[0].effects) == "block.shelf.activate");
    CHECK(shelves::chainAt(world, 8, 64, 8).size() == 1);

    // Now the two beside it, which join onto the chain.
    REQUIRE(world.setBlockState(7, 65, 8, registry::blocks::named("redstone_block")->stateId));
    shelves::reactToSignal(world, "overworld", 7, 64, 8, changes);
    REQUIRE(world.setBlockState(9, 65, 8, registry::blocks::named("redstone_block")->stateId));
    shelves::reactToSignal(world, "overworld", 9, 64, 8, changes);

    const std::vector<shelves::Position> chain = shelves::chainAt(world, 8, 64, 8);
    CHECK(chain.size() == 3);
    // parity: the chain reads from the shelf's own left, which is clockwise from its facing, so a
    // shelf looking north has its left to the east.
    CHECK(chain.front() == shelves::Position{9, 64, 8});
    CHECK(chain.back() == shelves::Position{7, 64, 8});
    CHECK(partAt(world, 8, 64, 8) == "center");
}

TEST_CASE("a chain lies along the shelf's own sides, whichever way it looks") {
    const auto chainAlong = [](const std::string& facing, std::int32_t dx, std::int32_t dz) {
        Loaded loaded;
        WorldState& world = loaded.state;
        std::vector<BlockChange> changes;
        for (std::int32_t step = 0; step <= 1; ++step) {
            const std::int32_t x = 8 + (dx * step);
            const std::int32_t z = 8 + (dz * step);
            REQUIRE(world.setBlockState(x, 64, z, shelf(facing, "false", "unconnected")));
            REQUIRE(world.setBlockState(x, 65, z, registry::blocks::named("redstone_block")->stateId));
            shelves::reactToSignal(world, "overworld", x, 64, z, changes);
        }
        const std::vector<shelves::Position> chain = shelves::chainAt(world, 8, 64, 8);
        return chain.size() == 2 && chain.front() == shelves::Position{8 + dx, 64, 8 + dz};
    };

    // The left of a shelf is clockwise from its facing.
    CHECK(chainAlong("north", 1, 0));
    CHECK(chainAlong("east", 0, 1));
    CHECK(chainAlong("south", -1, 0));
    CHECK(chainAlong("west", 0, -1));
}

TEST_CASE("a fourth shelf does not join a chain of three") {
    Loaded loaded;
    WorldState& world = loaded.state;
    std::vector<BlockChange> changes;
    for (std::int32_t x = 6; x <= 9; ++x) {
        REQUIRE(world.setBlockState(x, 64, 8, shelf("north", "false", "unconnected")));
        REQUIRE(world.setBlockState(x, 65, 8, registry::blocks::named("redstone_block")->stateId));
    }
    // Powered from the east end westwards, so the fourth one is the last to ask.
    for (std::int32_t x = 9; x >= 6; --x) {
        shelves::reactToSignal(world, "overworld", x, 64, 8, changes);
    }

    CHECK(shelves::chainAt(world, 9, 64, 8).size() == 3);
    // parity: `getMaxChainLength`, so the fourth stands on its own.
    CHECK(shelves::chainAt(world, 6, 64, 8).size() == 1);
    CHECK(partAt(world, 6, 64, 8) == "unconnected");
}

TEST_CASE("joining a chain says which blocks changed") {
    Loaded loaded;
    WorldState& world = loaded.state;
    std::vector<BlockChange> changes;
    for (std::int32_t x = 7; x <= 8; ++x) {
        REQUIRE(world.setBlockState(x, 64, 8, shelf("north", "false", "unconnected")));
        REQUIRE(world.setBlockState(x, 65, 8, registry::blocks::named("redstone_block")->stateId));
    }
    shelves::reactToSignal(world, "overworld", 7, 64, 8, changes);
    changes.clear();

    shelves::reactToSignal(world, "overworld", 8, 64, 8, changes);

    // The shelf itself, and the one it joined onto.
    std::set<std::pair<std::int32_t, std::int32_t>> touched;
    for (const BlockChange& change : changes) {
        touched.emplace(change.x, change.z);
    }
    CHECK(touched == std::set<std::pair<std::int32_t, std::int32_t>>{{7, 8}, {8, 8}});
    // parity: `SideChainPart`'s names read from the chain, not from the shelf: the one that joined
    // on its west side is the chain's left end.
    CHECK(partAt(world, 8, 64, 8) == "left");
    CHECK(partAt(world, 7, 64, 8) == "right");
}

TEST_CASE("a shelf that loses its signal leaves the chain") {
    Loaded loaded;
    WorldState& world = loaded.state;
    for (std::int32_t x = 7; x <= 8; ++x) {
        REQUIRE(world.setBlockState(x, 64, 8, shelf("north", "false", "unconnected")));
        REQUIRE(world.setBlockState(x, 65, 8, registry::blocks::named("redstone_block")->stateId));
    }
    std::vector<BlockChange> changes;
    shelves::reactToSignal(world, "overworld", 7, 64, 8, changes);
    shelves::reactToSignal(world, "overworld", 8, 64, 8, changes);
    REQUIRE(shelves::chainAt(world, 8, 64, 8).size() == 2);

    REQUIRE(world.setBlockState(8, 65, 8, registry::blocks::air().stateId));
    changes.clear();
    shelves::reactToSignal(world, "overworld", 8, 64, 8, changes);

    REQUIRE_FALSE(changes.empty());
    CHECK(soundOf(changes[0].effects) == "block.shelf.deactivate");
    CHECK(shelves::chainAt(world, 8, 64, 8).empty());
    CHECK(partAt(world, 8, 64, 8) == "unconnected");
    CHECK(partAt(world, 7, 64, 8) == "unconnected");
}

TEST_CASE("a chain swaps the hotbar slots that line up with it") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(8, 64, 8, shelf("north", "true", "unconnected")));
    REQUIRE(shelves::swap(world, 8, 64, 8, 0, ItemStack{.item = item("stone"), .count = 1}).has_value());
    blockInventory::Slots hotbar(9);
    hotbar[6] = ItemStack{.item = item("dirt"), .count = 2};

    CHECK(shelves::swapHotbar(world, 8, 64, 8, hotbar));

    // A chain of one uses the last three hotbar slots.
    REQUIRE(hotbar[6].has_value());
    CHECK(hotbar[6]->item == item("stone"));
    const blockInventory::Slots onShelf = blockInventory::read(*dataAt(world, 8, 64, 8), 3);
    REQUIRE(onShelf[0].has_value());
    CHECK(onShelf[0]->item == item("dirt"));
    CHECK(onShelf[0]->count == 2);

    // A shelf with nothing to trade moves nothing.
    blockInventory::Slots empty(9);
    REQUIRE(shelves::swap(world, 8, 64, 8, 0, std::nullopt).has_value());
    CHECK_FALSE(shelves::swapHotbar(world, 8, 64, 8, empty));
}

TEST_CASE("an unpowered shelf swaps no hotbar") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(8, 64, 8, shelf("north", "false", "unconnected")));
    REQUIRE(shelves::swap(world, 8, 64, 8, 0, ItemStack{.item = item("stone"), .count = 1}).has_value());
    blockInventory::Slots hotbar(9);

    CHECK_FALSE(shelves::swapHotbar(world, 8, 64, 8, hotbar));
}

TEST_CASE("a chain of three swaps the whole hotbar, in the order it lies") {
    Loaded loaded;
    WorldState& world = loaded.state;
    std::vector<BlockChange> changes;
    for (std::int32_t x = 7; x <= 9; ++x) {
        REQUIRE(world.setBlockState(x, 64, 8, shelf("north", "false", "unconnected")));
        REQUIRE(world.setBlockState(x, 65, 8, registry::blocks::named("redstone_block")->stateId));
    }
    for (std::int32_t x = 9; x >= 7; --x) {
        shelves::reactToSignal(world, "overworld", x, 64, 8, changes);
    }
    REQUIRE(shelves::chainAt(world, 8, 64, 8).size() == 3);
    blockInventory::Slots hotbar(9);
    for (std::size_t slot = 0; slot < hotbar.size(); ++slot) {
        hotbar[slot] = ItemStack{.item = item("stone"), .count = static_cast<std::int32_t>(slot) + 1};
    }

    CHECK(shelves::swapHotbar(world, 8, 64, 8, hotbar));

    // Nine hotbar slots against three shelves of three, in chain order.
    for (std::size_t slot = 0; slot < hotbar.size(); ++slot) {
        CHECK_FALSE(hotbar[slot].has_value());
    }
    const std::vector<shelves::Position> chain = shelves::chainAt(world, 8, 64, 8);
    for (std::size_t index = 0; index < chain.size(); ++index) {
        const blockInventory::Slots onShelf =
            blockInventory::read(*dataAt(world, chain[index][0], chain[index][1], chain[index][2]), 3);
        for (std::size_t slot = 0; slot < onShelf.size(); ++slot) {
            REQUIRE(onShelf[slot].has_value());
            CHECK(onShelf[slot]->count == static_cast<std::int32_t>((index * 3) + slot) + 1);
        }
    }
}

TEST_CASE("a shelf away from the middle chunk keeps its own slots") {
    Loaded loaded;
    WorldState& world = loaded.state;
    world.getOrGenerateChunk(1, 1);
    REQUIRE(world.setBlockState(21, 64, 21, shelf("north", "false", "unconnected")));

    REQUIRE(shelves::swap(world, 21, 64, 21, 0, ItemStack{.item = item("stone"), .count = 1}).has_value());
    const std::optional<shelves::SwapResult> back = shelves::swap(world, 21, 64, 21, 0, std::nullopt);

    REQUIRE(back.has_value());
    REQUIRE(back->removed.has_value());
    CHECK(back->removed->item == item("stone"));
}

TEST_CASE("the comparator reads three bits, and only from behind") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(8, 64, 8, shelf("north", "false", "unconnected")));
    REQUIRE(shelves::swap(world, 8, 64, 8, 0, ItemStack{.item = item("stone"), .count = 1}).has_value());
    REQUIRE(shelves::swap(world, 8, 64, 8, 2, ItemStack{.item = item("dirt"), .count = 1}).has_value());

    // A shelf facing north is read from the south.
    CHECK(shelves::comparatorSignal(dataAt(world, 8, 64, 8), "north", "south") == 5);
    CHECK(shelves::comparatorSignal(dataAt(world, 8, 64, 8), "north", "north") == 0);
    CHECK(shelves::comparatorSignal(dataAt(world, 8, 64, 8), "north", "east") == 0);
    CHECK(shelves::comparatorSignal(nullptr, "north", "south") == 0);
    CHECK(shelves::comparatorSignal(dataAt(world, 8, 64, 8), "", "south") == 0);
    CHECK(shelves::comparatorSignal(dataAt(world, 8, 64, 8), "north", "") == 0);
}

TEST_CASE("the click on the front picks the slot") {
    const std::int32_t front = shelf("north", "false", "unconnected");

    CHECK(shelves::slotAt(front, "north", 0.8F, 0.5F, 0.0F) == 0);
    CHECK(shelves::slotAt(front, "north", 0.1F, 0.5F, 0.0F) == 2);
    CHECK_FALSE(shelves::slotAt(front, "up", 0.5F, 1.0F, 0.5F).has_value());
}
