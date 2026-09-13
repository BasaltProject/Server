#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/BlockEntityTicks.hpp>
#include <basalt/server/runtime/world/BlockInventory.hpp>
#include <basalt/server/runtime/world/BrewingStands.hpp>
#include <basalt/server/runtime/world/Campfires.hpp>
#include <basalt/server/runtime/world/DaylightDetectors.hpp>
#include <basalt/server/runtime/world/Furnaces.hpp>

#include <doctest.h>

#include <cstdint>
#include <map>
#include <string>

using namespace basalt::server::runtime::world;
using basalt::server::world::Chunk;
using basalt::server::world::ChunkBlockEntity;
using basalt::wire::common::Identifier;
using basalt::wire::nbt::NbtCompound;
namespace crafting = basalt::server::world::crafting;
namespace loot = basalt::server::world::loot;
namespace registry = basalt::server::registry;

namespace {

[[nodiscard]] loot::ItemStack stack(const std::string& item, std::int32_t count = 1) {
    return loot::ItemStack{.item = Identifier::vanilla(item), .count = count};
}

[[nodiscard]] std::int32_t stateOf(const std::string& path) {
    const registry::BlockState* block = registry::blocks::named(path);
    REQUIRE(block != nullptr);
    return block->stateId;
}

[[nodiscard]] std::int32_t stateOf(const std::string& path, const std::map<std::string, std::string>& properties) {
    const std::optional<std::int32_t> stateId =
        registry::blockStateDefinitions::stateIdOf(Identifier::vanilla(path), properties, stateOf(path));
    REQUIRE(stateId.has_value());
    return *stateId;
}

[[nodiscard]] WorldState emptyWorld() {
    return WorldState{{}, [](std::int32_t chunkX, std::int32_t chunkZ) { return Chunk{chunkX, chunkZ}; }};
}

/// A chunk that is neither the origin nor square, so which coordinate came from where is visible.
constexpr std::int32_t kChunkX = 2;
constexpr std::int32_t kChunkZ = -3;
constexpr std::int32_t kX = (kChunkX * 16) + 4;
constexpr std::int32_t kZ = (kChunkZ * 16) + 6;

struct Loaded {
    WorldState state = emptyWorld();
    WorldClock clock{0, 1.0F, false};
    WorldWeather weather;

    Loaded() { state.getOrGenerateChunk(kChunkX, kChunkZ); }

    /// Puts a block and its block entity down, and answers the entity so a test can fill it in.
    ChunkBlockEntity* place(const std::string& path, std::int32_t stateId, std::int32_t x, std::int32_t y,
                            std::int32_t z) {
        REQUIRE(state.setBlockState(x, y, z, stateId));
        Chunk* chunk = state.chunkIfLoaded(x >> 4, z >> 4);
        REQUIRE(chunk != nullptr);
        chunk->setBlockEntity(ChunkBlockEntity{x & 15, y, z & 15, Identifier::vanilla(path), NbtCompound{}});
        return chunk->blockEntityAt(x & 15, y, z & 15);
    }

    [[nodiscard]] std::vector<BlockChange> tick(const blockEntityTicks::Hooks& hooks = {}) {
        return blockEntityTicks::tick(state, "overworld", clock, daylightDetectors::skyDarken(clock, weather), 0,
                                      {{kChunkX, kChunkZ}}, random, hooks);
    }

    basalt::server::random::RandomSource random = basalt::server::random::RandomSource::legacy(1);
};

/// A recipe book with one smelting recipe and one campfire recipe.
struct Book {
    crafting::Recipe smelted{.kind = crafting::RecipeKind::Cooking,
                             .result = stack("iron_ingot"),
                             .cooker = crafting::Cooker::Smelting,
                             .cookingTime = 200};
    crafting::Recipe grilled{.kind = crafting::RecipeKind::Cooking,
                             .result = stack("cooked_porkchop"),
                             .cooker = crafting::Cooker::Campfire,
                             .cookingTime = 2};

    [[nodiscard]] auto lookup() const {
        return [this](crafting::Cooker cooker, const loot::ItemStack& input) -> const crafting::Recipe* {
            if (cooker == crafting::Cooker::Smelting && input.item == Identifier::vanilla("iron_ore")) {
                return &smelted;
            }
            if (cooker == crafting::Cooker::Campfire && input.item == Identifier::vanilla("porkchop")) {
                return &grilled;
            }
            return nullptr;
        };
    }
};

} // namespace

TEST_CASE("a lit furnace cooks and puts what it is doing on the block itself") {
    Loaded loaded;
    const Book book;
    ChunkBlockEntity* entity = loaded.place("furnace", stateOf("furnace", {{"lit", "false"}}), kX, 64, kZ);
    furnaces::State state;
    state.items[0] = stack("iron_ore", 8);
    state.items[1] = stack("coal", 8);
    furnaces::write(state, entity->data());

    const std::vector<BlockChange> changes = loaded.tick({.recipeFor = book.lookup()});

    // The block says it is burning, at its own coordinates.
    REQUIRE(changes.size() == 1);
    CHECK(changes[0].x == kX);
    CHECK(changes[0].y == 64);
    CHECK(changes[0].z == kZ);
    CHECK(registry::blockStateDefinitions::propertiesOf(changes[0].stateId).at("lit") == "true");
    CHECK(loaded.state.blockState(kX, 64, kZ) == changes[0].stateId);
    // And what it has done so far is written back into the block entity.
    CHECK(furnaces::read(entity->data()).lit());
}

TEST_CASE("a furnace with nothing to do says nothing at all") {
    Loaded loaded;
    const Book book;
    ChunkBlockEntity* entity = loaded.place("furnace", stateOf("furnace", {{"lit", "false"}}), kX, 64, kZ);
    furnaces::write(furnaces::State{}, entity->data());

    CHECK(loaded.tick({.recipeFor = book.lookup()}).empty());
    CHECK(registry::blockStateDefinitions::propertiesOf(*loaded.state.blockState(kX, 64, kZ)).at("lit") == "false");
}

TEST_CASE("a plugin that refuses a smelt stops the furnace before it moves") {
    Loaded loaded;
    const Book book;
    ChunkBlockEntity* entity = loaded.place("furnace", stateOf("furnace", {{"lit", "false"}}), kX, 64, kZ);
    furnaces::State state;
    state.items[0] = stack("iron_ore", 8);
    state.items[1] = stack("coal", 8);
    state.items[2] = stack("iron_ingot", 3);
    furnaces::write(state, entity->data());
    std::string sawWorld;
    std::string sawCooker;
    std::optional<Identifier> sawInput;
    std::optional<Identifier> sawResult;

    const std::vector<BlockChange> changes = loaded.tick(
        {.recipeFor = book.lookup(),
         .allowSmelt = [&](std::string_view world, std::int32_t x, std::int32_t y, std::int32_t z,
                           std::string_view cooker, const loot::ItemStack* input, const loot::ItemStack* result) {
             sawWorld = std::string{world};
             sawCooker = std::string{cooker};
             if (input != nullptr) {
                 sawInput = input->item;
             }
             CHECK(x == kX);
             CHECK(y == 64);
             CHECK(z == kZ);
             if (result != nullptr) {
                 sawResult = result->item;
             }
             return false;
         }});

    CHECK(changes.empty());
    CHECK(sawWorld == "overworld");
    CHECK(sawCooker == "smelting");
    REQUIRE(sawInput.has_value());
    CHECK(*sawInput == Identifier::vanilla("iron_ore"));
    REQUIRE(sawResult.has_value());
    CHECK(*sawResult == Identifier::vanilla("iron_ingot"));
    CHECK_FALSE(furnaces::read(entity->data()).lit());
}

TEST_CASE("each of the four cookers reports itself by name") {
    for (const auto& [path, name] : std::vector<std::pair<std::string, std::string>>{
             {"furnace", "smelting"}, {"blast_furnace", "blasting"}, {"smoker", "smoking"}}) {
        Loaded loaded;
        const Book book;
        ChunkBlockEntity* entity = loaded.place(path, stateOf(path, {{"lit", "false"}}), kX, 64, kZ);
        furnaces::State state;
        state.items[0] = stack("iron_ore", 8);
        state.items[1] = stack("coal", 8);
        furnaces::write(state, entity->data());
        std::string sawCooker;

        static_cast<void>(loaded.tick(
            {.recipeFor = book.lookup(),
             .allowSmelt = [&sawCooker](std::string_view, std::int32_t, std::int32_t, std::int32_t,
                                        std::string_view cooker, const loot::ItemStack*, const loot::ItemStack*) {
                 sawCooker = std::string{cooker};
                 return false;
             }}));

        CHECK(sawCooker == name);
    }
}

TEST_CASE("a lit campfire cooks and drops what it finished beside itself") {
    Loaded loaded;
    const Book book;
    ChunkBlockEntity* entity = loaded.place("campfire", stateOf("campfire", {{"lit", "true"}}), kX, 64, kZ);
    campfires::State fire;
    fire.items[0] = stack("porkchop");
    fire.total[0] = 2;
    campfires::write(fire, entity->data());
    std::vector<blockEntityTicks::Dropped> dropped;

    static_cast<void>(
        loaded.tick({.recipeFor = book.lookup(),
                     .onDropped = [&dropped](const blockEntityTicks::Dropped& one) { dropped.push_back(one); }}));
    CHECK(dropped.empty());
    static_cast<void>(
        loaded.tick({.recipeFor = book.lookup(),
                     .onDropped = [&dropped](const blockEntityTicks::Dropped& one) { dropped.push_back(one); }}));

    REQUIRE(dropped.size() == 1);
    CHECK(dropped[0].x == kX);
    CHECK(dropped[0].y == 64);
    CHECK(dropped[0].z == kZ);
    CHECK(dropped[0].stack.item == Identifier::vanilla("cooked_porkchop"));
    CHECK_FALSE(campfires::read(entity->data()).items[0].has_value());
}

TEST_CASE("a campfire that is not lit loses progress instead of cooking") {
    Loaded loaded;
    const Book book;
    ChunkBlockEntity* entity = loaded.place("campfire", stateOf("campfire", {{"lit", "false"}}), kX, 64, kZ);
    campfires::State fire;
    fire.items[0] = stack("porkchop");
    fire.progress[0] = 5;
    fire.total[0] = 600;
    campfires::write(fire, entity->data());

    static_cast<void>(loaded.tick({.recipeFor = book.lookup()}));

    CHECK(campfires::read(entity->data()).progress[0] == 3);
}

TEST_CASE("a brewing stand shows the bottles standing in it") {
    Loaded loaded;
    ChunkBlockEntity* entity = loaded.place(
        "brewing_stand",
        stateOf("brewing_stand", {{"has_bottle_0", "false"}, {"has_bottle_1", "false"}, {"has_bottle_2", "false"}}), kX,
        64, kZ);
    // The bottles only reach the block on a turn that moved something, so give it a brew to run.
    brewingStands::State stand;
    stand.items[0] = stack("potion");
    stand.potions[0] = "water";
    stand.items[2] = stack("potion");
    stand.potions[2] = "water";
    stand.items[brewingStands::kIngredientSlot] = stack("nether_wart");
    stand.fuel = 20;
    brewingStands::write(stand, entity->data());

    const std::vector<BlockChange> changes = loaded.tick();

    REQUIRE(changes.size() == 1);
    const std::map<std::string, std::string> shown = registry::blockStateDefinitions::propertiesOf(changes[0].stateId);
    CHECK(shown.at("has_bottle_0") == "true");
    CHECK(shown.at("has_bottle_1") == "false");
    CHECK(shown.at("has_bottle_2") == "true");
}

TEST_CASE("a hopper asks a plugin before it moves anything") {
    Loaded loaded;
    REQUIRE(loaded.state.setBlockState(kX, 65, kZ, stateOf("chest")));
    Chunk* chunk = loaded.state.chunkIfLoaded(kChunkX, kChunkZ);
    REQUIRE(chunk != nullptr);
    NbtCompound above;
    blockInventory::write(above, blockInventory::Slots{stack("stone", 4)});
    chunk->setBlockEntity(ChunkBlockEntity{kX & 15, 65, kZ & 15, Identifier::vanilla("chest"), std::move(above)});
    ChunkBlockEntity* hopper =
        loaded.place("hopper", stateOf("hopper", {{"facing", "down"}, {"enabled", "true"}}), kX, 64, kZ);
    blockInventory::write(hopper->data(), blockInventory::Slots(5));
    std::int32_t asked = 0;

    static_cast<void>(loaded.tick(
        {.allowHopperMove = [&asked](std::string_view world, std::int32_t x, std::int32_t y, std::int32_t z) {
            ++asked;
            CHECK(world == "overworld");
            CHECK(x == kX);
            CHECK(y == 64);
            CHECK(z == kZ);
            return false;
        }}));

    CHECK(asked == 1);
    // Refused, so the hopper is still empty.
    CHECK_FALSE(blockInventory::read(hopper->data(), 5)[0].has_value());

    static_cast<void>(loaded.tick());
    CHECK(blockInventory::read(hopper->data(), 5)[0].has_value());
}

TEST_CASE("a daylight detector is only looked at once every twenty ticks") {
    Loaded loaded;
    static_cast<void>(loaded.place("daylight_detector",
                                   stateOf("daylight_detector", {{"inverted", "false"}, {"power", "15"}}), kX, 64, kZ));

    // parity: tick zero is a multiple of twenty, so the first look is immediate.
    CHECK(loaded.tick().size() == 1);
    CHECK(registry::blockStateDefinitions::propertiesOf(*loaded.state.blockState(kX, 64, kZ)).at("power") != "15");

    loaded.clock = WorldClock{1, 1.0F, false};
    CHECK(loaded.tick().empty());
    loaded.clock = WorldClock{19, 1.0F, false};
    CHECK(loaded.tick().empty());
}

TEST_CASE("a chunk that is not loaded, and a block entity with no ticker, are both skipped") {
    Loaded loaded;
    const Book book;
    static_cast<void>(loaded.place("chest", stateOf("chest"), kX, 64, kZ));

    CHECK(loaded.tick({.recipeFor = book.lookup()}).empty());
    CHECK(blockEntityTicks::tick(loaded.state, "overworld", loaded.clock,
                                 daylightDetectors::skyDarken(loaded.clock, loaded.weather), 0, {{9, 9}}, loaded.random)
              .empty());
    CHECK(blockEntityTicks::tick(loaded.state, "overworld", loaded.clock,
                                 daylightDetectors::skyDarken(loaded.clock, loaded.weather), 0, {}, loaded.random)
              .empty());
}

TEST_CASE("a furnace that keeps burning says nothing, and one that goes out says so") {
    Loaded stillBurning;
    const Book book;
    ChunkBlockEntity* burning = stillBurning.place("furnace", stateOf("furnace", {{"lit", "true"}}), kX, 64, kZ);
    furnaces::State lit;
    lit.litTimeRemaining = 100;
    lit.litTotalTime = 200;
    lit.items[0] = stack("iron_ore", 8);
    furnaces::write(lit, burning->data());

    // The counters moved, so it is written back, but the block already says what it should.
    CHECK(stillBurning.tick({.recipeFor = book.lookup()}).empty());
    CHECK(furnaces::read(burning->data()).litTimeRemaining == 99);

    Loaded goingOut;
    ChunkBlockEntity* dying = goingOut.place("furnace", stateOf("furnace", {{"lit", "true"}}), kX, 64, kZ);
    furnaces::State last;
    last.litTimeRemaining = 1;
    last.litTotalTime = 200;
    furnaces::write(last, dying->data());

    const std::vector<BlockChange> changes = goingOut.tick({.recipeFor = book.lookup()});

    REQUIRE(changes.size() == 1);
    CHECK(registry::blockStateDefinitions::propertiesOf(changes[0].stateId).at("lit") == "false");
}

TEST_CASE("a brewing stand whose block already shows its bottles says nothing") {
    Loaded loaded;
    ChunkBlockEntity* entity = loaded.place(
        "brewing_stand",
        stateOf("brewing_stand", {{"has_bottle_0", "true"}, {"has_bottle_1", "false"}, {"has_bottle_2", "true"}}), kX,
        64, kZ);
    brewingStands::State stand;
    stand.items[0] = stack("potion");
    stand.potions[0] = "water";
    stand.items[2] = stack("potion");
    stand.potions[2] = "water";
    stand.items[brewingStands::kIngredientSlot] = stack("nether_wart");
    stand.fuel = 20;
    brewingStands::write(stand, entity->data());

    // The brew moved on, so it is written back, but the three bits were already right.
    CHECK(loaded.tick().empty());
    CHECK(brewingStands::read(entity->data()).brewTime > 0);
}
