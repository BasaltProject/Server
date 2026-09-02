#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/Campfires.hpp>

#include <doctest.h>

#include <map>
#include <string>

using namespace basalt::server::runtime::world;
using basalt::wire::common::Identifier;
using basalt::wire::nbt::NbtCompound;
namespace crafting = basalt::server::world::crafting;
namespace loot = basalt::server::world::loot;
namespace registry = basalt::server::registry;

namespace {

[[nodiscard]] loot::ItemStack stack(const std::string& item, std::int32_t count = 1) {
    return loot::ItemStack{.item = Identifier::vanilla(item), .count = count};
}

/// One campfire recipe, kept alive by the caller so a pointer to it stays good.
struct Book {
    crafting::Recipe cooked{.kind = crafting::RecipeKind::Cooking,
                            .result = stack("cooked_porkchop"),
                            .cooker = crafting::Cooker::Campfire,
                            .cookingTime = 600};

    explicit Book(const std::string& result, std::int32_t time) {
        cooked.result = stack(result);
        cooked.cookingTime = time;
    }

    [[nodiscard]] campfires::ResultFor resultFor(const std::string& accepts) const {
        return [this, accepts](const loot::ItemStack& input) -> const crafting::Recipe* {
            return input.item == Identifier::vanilla(accepts) ? &cooked : nullptr;
        };
    }
};

[[nodiscard]] std::int32_t campfireState(const std::string& path, const std::string& lit) {
    const std::optional<std::int32_t> stateId = registry::blockStateDefinitions::stateIdOf(
        Identifier::vanilla(path), {{"lit", lit}}, registry::blocks::named(path)->stateId);
    REQUIRE(stateId.has_value());
    return *stateId;
}

} // namespace

TEST_CASE("a lit campfire cooks what is on it and hands back the result") {
    const Book book{"cooked_porkchop", 600};
    campfires::State state;
    REQUIRE(campfires::placeFood(state, stack("porkchop"), book.resultFor("porkchop")));

    // Nothing comes off until the whole cooking time has gone by.
    for (std::int32_t tick = 0; tick < 599; ++tick) {
        CHECK(campfires::tick(state, true, book.resultFor("porkchop")).empty());
    }
    CHECK(state.progress[0] == 599);

    const std::vector<loot::ItemStack> done = campfires::tick(state, true, book.resultFor("porkchop"));
    REQUIRE(done.size() == 1);
    CHECK(done[0].item == Identifier::vanilla("cooked_porkchop"));
    // And the slot is empty again, with nothing left of its progress.
    CHECK_FALSE(state.items[0].has_value());
    CHECK(state.progress[0] == 0);
    CHECK(state.total[0] == 0);
}

TEST_CASE("food with no campfire recipe of its own comes back as it went on") {
    const Book book{"cooked_porkchop", 2};
    campfires::State state;
    state.items[0] = stack("stone");
    state.total[0] = 2;

    CHECK(campfires::tick(state, true, book.resultFor("porkchop")).empty());
    const std::vector<loot::ItemStack> done = campfires::tick(state, true, book.resultFor("porkchop"));

    REQUIRE(done.size() == 1);
    CHECK(done[0].item == Identifier::vanilla("stone"));
}

TEST_CASE("a fire that goes out loses progress two at a time and never past zero") {
    campfires::State state;
    state.items[0] = stack("porkchop");
    state.progress[0] = 5;
    state.total[0] = 600;

    CHECK(campfires::tick(state, false, {}).empty());
    CHECK(state.progress[0] == 3);
    CHECK(campfires::tick(state, false, {}).empty());
    CHECK(state.progress[0] == 1);
    CHECK(campfires::tick(state, false, {}).empty());
    CHECK(state.progress[0] == 0);
    // And it stays there, rather than going negative.
    CHECK(campfires::tick(state, false, {}).empty());
    CHECK(state.progress[0] == 0);
    // What is on it is still on it: only the progress was lost.
    CHECK(state.items[0].has_value());
}

TEST_CASE("an empty slot on a lit fire is left alone") {
    const Book book{"cooked_porkchop", 2};
    campfires::State state;
    state.total[1] = 2;

    CHECK(campfires::tick(state, true, book.resultFor("porkchop")).empty());
    CHECK(state.progress[1] == 0);
    CHECK(state.total[1] == 2);
}

TEST_CASE("the four slots cook side by side and each finishes on its own") {
    const Book quick{"cooked_porkchop", 2};
    campfires::State state;
    for (std::size_t slot = 0; slot < 4; ++slot) {
        state.items[slot] = stack("porkchop");
        state.total[slot] = static_cast<std::int32_t>(slot) + 1;
    }

    const std::vector<loot::ItemStack> first = campfires::tick(state, true, quick.resultFor("porkchop"));
    REQUIRE(first.size() == 1);
    CHECK_FALSE(state.items[0].has_value());
    CHECK(state.items[1].has_value());

    const std::vector<loot::ItemStack> second = campfires::tick(state, true, quick.resultFor("porkchop"));
    REQUIRE(second.size() == 1);
    CHECK_FALSE(state.items[1].has_value());
    CHECK(state.progress[2] == 2);
    CHECK(state.progress[3] == 2);
}

TEST_CASE("food goes into the first free slot, one piece at a time") {
    const Book book{"cooked_porkchop", 600};
    campfires::State state;
    state.items[0] = stack("beef");

    CHECK(campfires::placeFood(state, stack("porkchop", 64), book.resultFor("porkchop")));

    CHECK(state.items[0]->item == Identifier::vanilla("beef"));
    REQUIRE(state.items[1].has_value());
    CHECK(state.items[1]->item == Identifier::vanilla("porkchop"));
    // parity: one piece off the stack, and the recipe's own time.
    CHECK(state.items[1]->count == 1);
    CHECK(state.total[1] == 600);
    CHECK(state.progress[1] == 0);
}

TEST_CASE("food a campfire cannot cook is refused rather than left sitting there") {
    const Book book{"cooked_porkchop", 600};
    campfires::State state;

    CHECK_FALSE(campfires::placeFood(state, stack("stone"), book.resultFor("porkchop")));
    CHECK_FALSE(state.items[0].has_value());

    // And a fire with all four sides taken has nowhere to put it.
    campfires::State full;
    for (std::size_t slot = 0; slot < 4; ++slot) {
        full.items[slot] = stack("beef");
    }
    CHECK_FALSE(campfires::placeFood(full, stack("porkchop"), book.resultFor("porkchop")));
}

TEST_CASE("both campfires are campfires, and only a lit one cooks") {
    CHECK(campfires::isCampfire("campfire"));
    CHECK(campfires::isCampfire("soul_campfire"));
    CHECK_FALSE(campfires::isCampfire("furnace"));
    CHECK_FALSE(campfires::isCampfire(""));

    CHECK(campfires::isLitCampfire(campfireState("campfire", "true")));
    CHECK(campfires::isLitCampfire(campfireState("soul_campfire", "true")));
    CHECK_FALSE(campfires::isLitCampfire(campfireState("campfire", "false")));
    CHECK_FALSE(campfires::isLitCampfire(registry::blocks::named("furnace")->stateId));
    CHECK_FALSE(campfires::isLitCampfire(registry::blocks::air().stateId));
}

TEST_CASE("what is on the fire survives being written down and read back") {
    campfires::State state;
    state.items[0] = stack("porkchop");
    state.items[2] = stack("beef");
    state.progress = {5, 0, 300, 0};
    state.total = {600, 0, 600, 0};
    NbtCompound data;

    campfires::write(state, data);
    const campfires::State read = campfires::read(data);

    REQUIRE(read.items[0].has_value());
    CHECK(read.items[0]->item == Identifier::vanilla("porkchop"));
    CHECK_FALSE(read.items[1].has_value());
    REQUIRE(read.items[2].has_value());
    CHECK(read.items[2]->item == Identifier::vanilla("beef"));
    CHECK(read.progress == std::array<std::int32_t, 4>{5, 0, 300, 0});
    CHECK(read.total == std::array<std::int32_t, 4>{600, 0, 600, 0});
}

TEST_CASE("a fire with nothing written down reads as an empty one") {
    const NbtCompound empty;

    const campfires::State state = campfires::read(empty);

    CHECK(state.items.size() == 4);
    for (std::size_t slot = 0; slot < 4; ++slot) {
        CHECK_FALSE(state.items[slot].has_value());
        CHECK(state.progress[slot] == 0);
        CHECK(state.total[slot] == 0);
    }
}

TEST_CASE("a fire written down with more slots than it has keeps only its own four") {
    NbtCompound data;
    data.putIntArray("CookingTimes", {1, 2, 3, 4, 5, 6});
    data.putIntArray("CookingTotalTimes", {10, 20, 30, 40, 50});

    const campfires::State state = campfires::read(data);

    CHECK(state.progress == std::array<std::int32_t, 4>{1, 2, 3, 4});
    CHECK(state.total == std::array<std::int32_t, 4>{10, 20, 30, 40});
}
