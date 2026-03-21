#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/world/Furnaces.hpp>

#include <doctest.h>

#include <cstdint>
#include <string>
#include <vector>

using namespace basalt::server::runtime::world;
using basalt::server::world::crafting::Cooker;
using basalt::server::world::crafting::Recipe;
using basalt::server::world::crafting::RecipeKind;
using basalt::server::world::loot::ItemStack;
namespace nbt = basalt::wire::nbt;

namespace {

[[nodiscard]] basalt::wire::common::Identifier item(const std::string& path) {
    return basalt::wire::common::Identifier::vanilla(path);
}

[[nodiscard]] ItemStack stack(const std::string& path, std::int32_t count = 1) {
    return ItemStack{.item = item(path), .count = count};
}

[[nodiscard]] Recipe cooking(const std::string& result, std::int32_t count = 1, std::int32_t time = 200) {
    return Recipe{
        .kind = RecipeKind::Cooking, .result = stack(result, count), .cooker = Cooker::Smelting, .cookingTime = time};
}

/// A book of one recipe, which the input has to match by name for anything to cook.
[[nodiscard]] furnaces::RecipeFor bookOf(const Recipe& recipe, const std::string& input) {
    return [&recipe, input](const ItemStack& given) -> const Recipe* {
        return given.item.path() == input ? &recipe : nullptr;
    };
}

/// A furnace with beef in it and coal under it, one tick short of nothing in particular.
[[nodiscard]] furnaces::State beefAndCoal() {
    furnaces::State state;
    state.items[furnaces::kInputSlot] = stack("beef");
    state.items[furnaces::kFuelSlot] = stack("coal");
    state.cookingTotalTime = 200;
    return state;
}

constexpr std::int32_t kCoalTicks = 1600;
constexpr std::int32_t kLavaTicks = 20000;

} // namespace

TEST_CASE("only the three block entities cook") {
    CHECK(furnaces::cookerFor("furnace") == Cooker::Smelting);
    CHECK(furnaces::cookerFor("blast_furnace") == Cooker::Blasting);
    CHECK(furnaces::cookerFor("smoker") == Cooker::Smoking);
    CHECK_FALSE(furnaces::cookerFor("campfire").has_value());
    CHECK_FALSE(furnaces::cookerFor("").has_value());
}

TEST_CASE("an idle furnace answers that nothing moved") {
    furnaces::State state;
    const Recipe recipe = cooking("cooked_beef");

    CHECK_FALSE(furnaces::tick(state, bookOf(recipe, "beef")).moved);
    CHECK(state.litTimeRemaining == 0);
    CHECK(state.cookingTimer == 0);
    CHECK_FALSE(state.lit());
}

TEST_CASE("the fire burns down whether or not anything is cooking") {
    furnaces::State state;
    state.litTimeRemaining = 2;
    state.litTotalTime = 2;
    const Recipe recipe = cooking("cooked_beef");

    CHECK(furnaces::tick(state, bookOf(recipe, "beef")).moved);
    CHECK(state.litTimeRemaining == 1);
    CHECK(state.lit());

    CHECK(furnaces::tick(state, bookOf(recipe, "beef")).moved);
    CHECK(state.litTimeRemaining == 0);
    CHECK_FALSE(state.lit());
    // The total stays put: it is what the burning bar is drawn against.
    CHECK(state.litTotalTime == 2);
}

TEST_CASE("the tick that puts the fire out is the first one that does not cook") {
    const Recipe recipe = cooking("cooked_beef");
    furnaces::State state;
    state.litTimeRemaining = 2;
    state.cookingTimer = 5;
    state.cookingTotalTime = 200;
    state.items[furnaces::kInputSlot] = stack("beef");

    // Two ticks of fire left, so this one still counts as lit and the progress goes up.
    CHECK(furnaces::tick(state, bookOf(recipe, "beef")).moved);
    CHECK(state.litTimeRemaining == 1);
    CHECK(state.cookingTimer == 6);

    // The one that takes it to zero is not lit any more, and with no fuel under it the progress
    // cools instead of climbing.
    CHECK(furnaces::tick(state, bookOf(recipe, "beef")).moved);
    CHECK(state.litTimeRemaining == 0);
    CHECK(state.cookingTimer == 4);
}

TEST_CASE("fuel with nothing to cook still lets the progress cool") {
    const Recipe recipe = cooking("cooked_beef");
    furnaces::State state;
    state.items[furnaces::kFuelSlot] = stack("coal");
    state.cookingTimer = 5;
    state.cookingTotalTime = 200;

    CHECK(furnaces::tick(state, bookOf(recipe, "beef")).moved);
    CHECK(state.cookingTimer == 3);
    CHECK(state.items[furnaces::kFuelSlot]->count == 1);
}

TEST_CASE("fuel is only eaten when there is something to cook") {
    furnaces::State state;
    state.items[furnaces::kFuelSlot] = stack("coal", 2);
    const Recipe recipe = cooking("cooked_beef");

    CHECK_FALSE(furnaces::tick(state, bookOf(recipe, "beef")).moved);
    CHECK(state.items[furnaces::kFuelSlot]->count == 2);
    CHECK(state.litTimeRemaining == 0);
}

TEST_CASE("the fuel lights the furnace and the first tick of progress goes on") {
    furnaces::State state = beefAndCoal();
    const Recipe recipe = cooking("cooked_beef");

    CHECK(furnaces::tick(state, bookOf(recipe, "beef")).moved);
    CHECK(state.litTimeRemaining == kCoalTicks);
    CHECK(state.litTotalTime == kCoalTicks);
    CHECK_FALSE(state.items[furnaces::kFuelSlot].has_value());
    CHECK(state.cookingTimer == 1);
    CHECK(state.items[furnaces::kInputSlot]->count == 1);
}

TEST_CASE("a stack of fuel loses exactly one") {
    furnaces::State state = beefAndCoal();
    state.items[furnaces::kFuelSlot] = stack("coal", 3);
    const Recipe recipe = cooking("cooked_beef");

    CHECK(furnaces::tick(state, bookOf(recipe, "beef")).moved);
    REQUIRE(state.items[furnaces::kFuelSlot].has_value());
    CHECK(state.items[furnaces::kFuelSlot]->item == item("coal"));
    CHECK(state.items[furnaces::kFuelSlot]->count == 2);
}

TEST_CASE("a bucket of lava leaves the bucket behind") {
    furnaces::State state = beefAndCoal();
    state.items[furnaces::kFuelSlot] = stack("lava_bucket");
    const Recipe recipe = cooking("cooked_beef");

    CHECK(furnaces::tick(state, bookOf(recipe, "beef")).moved);
    CHECK(state.litTimeRemaining == kLavaTicks);
    REQUIRE(state.items[furnaces::kFuelSlot].has_value());
    CHECK(state.items[furnaces::kFuelSlot]->item == item("bucket"));
    CHECK(state.items[furnaces::kFuelSlot]->count == 1);
}

TEST_CASE("something in the fuel slot that does not burn puts the counters back to zero") {
    furnaces::State state = beefAndCoal();
    state.items[furnaces::kFuelSlot] = stack("stone");
    state.litTotalTime = 99;
    state.cookingTimer = 5;
    const Recipe recipe = cooking("cooked_beef");

    CHECK(furnaces::tick(state, bookOf(recipe, "beef")).moved);
    CHECK(state.litTimeRemaining == 0);
    CHECK(state.litTotalTime == 0);
    CHECK(state.cookingTimer == 0);
    CHECK(state.items[furnaces::kFuelSlot]->count == 1);

    // With no progress to lose there is nothing to write back either.
    CHECK_FALSE(furnaces::tick(state, bookOf(recipe, "beef")).moved);
}

TEST_CASE("a full result slot stops the fuel being eaten") {
    furnaces::State state = beefAndCoal();
    state.items[furnaces::kResultSlot] = stack("cooked_beef", 64);
    state.cookingTimer = 5;
    const Recipe recipe = cooking("cooked_beef");

    CHECK(furnaces::tick(state, bookOf(recipe, "beef")).moved);
    CHECK(state.cookingTimer == 0);
    CHECK(state.litTimeRemaining == 0);
    CHECK(state.items[furnaces::kFuelSlot]->count == 1);

    CHECK_FALSE(furnaces::tick(state, bookOf(recipe, "beef")).moved);
}

TEST_CASE("the result slot takes it while it fits") {
    const Recipe recipe = cooking("cooked_beef");
    furnaces::State state = beefAndCoal();
    state.items[furnaces::kResultSlot] = stack("cooked_beef", 63);

    CHECK(furnaces::tick(state, bookOf(recipe, "beef")).moved);
    CHECK(state.litTimeRemaining == kCoalTicks);

    // One more in the slot and the same stack no longer fits.
    furnaces::State full = beefAndCoal();
    full.items[furnaces::kResultSlot] = stack("cooked_beef", 64);
    CHECK_FALSE(furnaces::tick(full, bookOf(recipe, "beef")).moved);
}

TEST_CASE("the furnace's own ceiling caps the result") {
    const Recipe recipe = cooking("cooked_beef");
    furnaces::State state = beefAndCoal();
    state.items[furnaces::kResultSlot] = stack("cooked_beef", 3);

    CHECK_FALSE(furnaces::tick(state, bookOf(recipe, "beef"), 3).moved);

    furnaces::State roomy = beefAndCoal();
    roomy.items[furnaces::kResultSlot] = stack("cooked_beef", 3);
    CHECK(furnaces::tick(roomy, bookOf(recipe, "beef"), 4).moved);
}

TEST_CASE("the result item's own ceiling caps it too") {
    // A bucket stacks to sixteen and a full one to a single item, so the second never fits.
    const Recipe recipe = cooking("water_bucket");
    furnaces::State state = beefAndCoal();
    state.items[furnaces::kResultSlot] = stack("water_bucket", 1);

    CHECK_FALSE(furnaces::tick(state, bookOf(recipe, "beef")).moved);

    const Recipe buckets = cooking("bucket");
    furnaces::State stacking = beefAndCoal();
    stacking.items[furnaces::kResultSlot] = stack("bucket", 1);
    CHECK(furnaces::tick(stacking, bookOf(buckets, "beef")).moved);
}

TEST_CASE("a result slot holding the same item with different components refuses it") {
    const Recipe recipe = cooking("cooked_beef");
    furnaces::State state = beefAndCoal();
    ItemStack held = stack("cooked_beef", 1);
    held.components.putString("mark", "somebody else's");
    state.items[furnaces::kResultSlot] = held;

    CHECK_FALSE(furnaces::tick(state, bookOf(recipe, "beef")).moved);
    CHECK(state.litTimeRemaining == 0);
}

TEST_CASE("a result slot holding another item entirely refuses it") {
    const Recipe recipe = cooking("cooked_beef");
    furnaces::State state = beefAndCoal();
    state.items[furnaces::kResultSlot] = stack("cooked_porkchop", 1);

    CHECK_FALSE(furnaces::tick(state, bookOf(recipe, "beef")).moved);
}

TEST_CASE("a recipe that makes nothing cooks nothing") {
    const Recipe recipe = cooking("cooked_beef", 0);
    furnaces::State state = beefAndCoal();
    state.cookingTimer = 5;

    CHECK(furnaces::tick(state, bookOf(recipe, "beef")).moved);
    CHECK(state.cookingTimer == 0);
    CHECK(state.litTimeRemaining == 0);
}

TEST_CASE("an input nothing cooks leaves the progress where it stands") {
    // parity: vanilla only walks the progress back for a recipe whose result does not fit; an input
    // with no recipe at all is left alone.
    const Recipe recipe = cooking("cooked_beef");
    furnaces::State state = beefAndCoal();
    state.items[furnaces::kInputSlot] = stack("stone");
    state.cookingTimer = 5;

    CHECK_FALSE(furnaces::tick(state, bookOf(recipe, "beef")).moved);
    CHECK(state.cookingTimer == 5);
    CHECK(state.items[furnaces::kFuelSlot]->count == 1);
}

TEST_CASE("without a recipe book nothing cooks at all") {
    furnaces::State state = beefAndCoal();

    CHECK_FALSE(furnaces::tick(state, furnaces::RecipeFor{}).moved);
    CHECK(state.litTimeRemaining == 0);
    CHECK(furnaces::cookTimeFor(state, furnaces::RecipeFor{}) == furnaces::kDefaultCookTime);
}

TEST_CASE("an empty input slot on a lit furnace loses the progress") {
    const Recipe recipe = cooking("cooked_beef");
    furnaces::State state;
    state.litTimeRemaining = 10;
    state.cookingTimer = 5;

    CHECK(furnaces::tick(state, bookOf(recipe, "beef")).moved);
    CHECK(state.cookingTimer == 0);
    CHECK(state.litTimeRemaining == 9);
}

TEST_CASE("the cook lands on the tick the timer meets the total") {
    const Recipe recipe = cooking("cooked_beef", 1, 120);
    furnaces::State state;
    state.litTimeRemaining = 50;
    state.cookingTimer = 118;
    // parity: while there is an input, the total is that input's own cooking time: `setItem` put it
    // there. A furnace whose total says anything else with something in the slot is not a state the
    // game can reach.
    state.cookingTotalTime = 120;
    state.items[furnaces::kInputSlot] = stack("beef", 2);

    CHECK(furnaces::tick(state, bookOf(recipe, "beef")).moved);
    CHECK(state.cookingTimer == 119);
    CHECK(state.cookingTotalTime == 120);
    CHECK_FALSE(state.items[furnaces::kResultSlot].has_value());

    CHECK(furnaces::tick(state, bookOf(recipe, "beef")).moved);
    CHECK(state.cookingTimer == 0);
    // The next item's time comes from the recipe that just finished.
    CHECK(state.cookingTotalTime == 120);
    REQUIRE(state.items[furnaces::kResultSlot].has_value());
    CHECK(state.items[furnaces::kResultSlot]->item == item("cooked_beef"));
    CHECK(state.items[furnaces::kResultSlot]->count == 1);
    CHECK(state.items[furnaces::kInputSlot]->count == 1);
}

TEST_CASE("the last item of the input leaves the slot empty") {
    const Recipe recipe = cooking("cooked_beef", 2, 1);
    furnaces::State state;
    state.litTimeRemaining = 50;
    state.cookingTimer = 0;
    state.cookingTotalTime = 1;
    state.items[furnaces::kInputSlot] = stack("beef");
    state.items[furnaces::kResultSlot] = stack("cooked_beef", 5);

    CHECK(furnaces::tick(state, bookOf(recipe, "beef")).moved);
    CHECK_FALSE(state.items[furnaces::kInputSlot].has_value());
    CHECK(state.items[furnaces::kResultSlot]->count == 7);
}

TEST_CASE("a wet sponge fills the bucket standing next to it") {
    const Recipe recipe = cooking("sponge", 1, 1);
    furnaces::State state;
    state.litTimeRemaining = 50;
    state.cookingTotalTime = 1;
    state.items[furnaces::kInputSlot] = stack("wet_sponge");
    state.items[furnaces::kFuelSlot] = stack("bucket");

    CHECK(furnaces::tick(state, bookOf(recipe, "wet_sponge")).moved);
    REQUIRE(state.items[furnaces::kFuelSlot].has_value());
    CHECK(state.items[furnaces::kFuelSlot]->item == item("water_bucket"));
    CHECK(state.items[furnaces::kFuelSlot]->count == 1);
    CHECK(state.items[furnaces::kResultSlot]->item == item("sponge"));
}

TEST_CASE("the bucket a lava bucket left behind is filled by the same tick's sponge") {
    const Recipe recipe = cooking("sponge", 1, 1);
    furnaces::State state;
    state.cookingTotalTime = 1;
    state.items[furnaces::kInputSlot] = stack("wet_sponge");
    state.items[furnaces::kFuelSlot] = stack("lava_bucket");

    CHECK(furnaces::tick(state, bookOf(recipe, "wet_sponge")).moved);
    CHECK(state.litTimeRemaining == kLavaTicks);
    REQUIRE(state.items[furnaces::kFuelSlot].has_value());
    CHECK(state.items[furnaces::kFuelSlot]->item == item("water_bucket"));
}

TEST_CASE("anything else in the fuel slot is left alone by a drying sponge") {
    const Recipe recipe = cooking("sponge", 1, 1);
    furnaces::State state;
    state.litTimeRemaining = 50;
    state.cookingTotalTime = 1;
    state.items[furnaces::kInputSlot] = stack("wet_sponge");
    state.items[furnaces::kFuelSlot] = stack("coal", 4);

    CHECK(furnaces::tick(state, bookOf(recipe, "wet_sponge")).moved);
    CHECK(state.items[furnaces::kFuelSlot]->item == item("coal"));
    CHECK(state.items[furnaces::kFuelSlot]->count == 4);
}

TEST_CASE("an empty fuel slot is left alone by a drying sponge") {
    const Recipe recipe = cooking("sponge", 1, 1);
    furnaces::State state;
    state.litTimeRemaining = 50;
    state.cookingTotalTime = 1;
    state.items[furnaces::kInputSlot] = stack("wet_sponge");

    CHECK(furnaces::tick(state, bookOf(recipe, "wet_sponge")).moved);
    CHECK_FALSE(state.items[furnaces::kFuelSlot].has_value());
}

TEST_CASE("a fire that went out walks the progress back two ticks at a time") {
    const Recipe recipe = cooking("cooked_beef");
    furnaces::State state;
    state.cookingTimer = 5;
    state.cookingTotalTime = 200;

    CHECK(furnaces::tick(state, bookOf(recipe, "beef")).moved);
    CHECK(state.cookingTimer == 3);
    CHECK(furnaces::tick(state, bookOf(recipe, "beef")).moved);
    CHECK(state.cookingTimer == 1);
    CHECK(furnaces::tick(state, bookOf(recipe, "beef")).moved);
    CHECK(state.cookingTimer == 0);
    // At zero it has nothing left to give back.
    CHECK_FALSE(furnaces::tick(state, bookOf(recipe, "beef")).moved);
}

TEST_CASE("the progress a cooling furnace keeps never passes its own total") {
    const Recipe recipe = cooking("cooked_beef");
    furnaces::State state;
    state.cookingTimer = 10;
    state.cookingTotalTime = 4;

    CHECK(furnaces::tick(state, bookOf(recipe, "beef")).moved);
    CHECK(state.cookingTimer == 4);
}

TEST_CASE("what the next item takes comes from its own recipe") {
    const Recipe recipe = cooking("cooked_beef", 1, 120);
    furnaces::State state;
    CHECK(furnaces::cookTimeFor(state, bookOf(recipe, "beef")) == furnaces::kDefaultCookTime);

    state.items[furnaces::kInputSlot] = stack("stone");
    CHECK(furnaces::cookTimeFor(state, bookOf(recipe, "beef")) == furnaces::kDefaultCookTime);

    state.items[furnaces::kInputSlot] = stack("beef");
    CHECK(furnaces::cookTimeFor(state, bookOf(recipe, "beef")) == 120);
}

TEST_CASE("a furnace saved mid-smelt comes back the way it went in") {
    furnaces::State state;
    state.litTimeRemaining = 1234;
    state.litTotalTime = 1600;
    state.cookingTimer = 77;
    state.cookingTotalTime = 200;
    state.items[furnaces::kInputSlot] = stack("beef", 3);
    state.items[furnaces::kFuelSlot] = stack("coal", 5);
    state.items[furnaces::kResultSlot] = stack("cooked_beef", 7);

    nbt::NbtCompound data;
    furnaces::write(state, data);

    // parity: 26.3 writes the four counters as ints, and the speed beside them.
    CHECK(data.getInt("lit_time_remaining") == 1234);
    CHECK(data.getInt("lit_total_time") == 1600);
    CHECK(data.getInt("cooking_time_spent") == 77);
    CHECK(data.getInt("cooking_total_time") == 200);
    CHECK(data.getFloat("speed_multiplier") == 1.0F);

    const furnaces::State back = furnaces::read(data);
    CHECK(back.litTimeRemaining == 1234);
    CHECK(back.litTotalTime == 1600);
    CHECK(back.cookingTimer == 77);
    CHECK(back.cookingTotalTime == 200);
    REQUIRE(back.items.size() == 3);
    CHECK(back.items[furnaces::kInputSlot]->count == 3);
    CHECK(back.items[furnaces::kFuelSlot]->item == item("coal"));
    CHECK(back.items[furnaces::kFuelSlot]->count == 5);
    CHECK(back.items[furnaces::kResultSlot]->count == 7);
}

TEST_CASE("a furnace saved by 26.2 with short counters still reads them") {
    nbt::NbtCompound data;
    data.putShort("lit_time_remaining", 50);
    data.putShort("cooking_time_spent", 9);
    const furnaces::State state = furnaces::read(data);
    CHECK(state.litTimeRemaining == 50);
    CHECK(state.cookingTimer == 9);
    CHECK(state.speedMultiplier == 1.0F);
}

TEST_CASE("a smoker burns fuel for half as long and cooks twice as fast") {
    // parity: `cooking/time_coal` and `cooking/speed_default` under `block/fast_cooking`.
    CHECK(furnaces::fuelEffect(item("coal"), Cooker::Smelting).burnTicks == kCoalTicks);
    CHECK(furnaces::fuelEffect(item("coal"), Cooker::Smoking).burnTicks == kCoalTicks / 2);
    CHECK(furnaces::fuelEffect(item("coal"), Cooker::Blasting).speedMultiplier == 2.0F);
    CHECK(furnaces::fuelEffect(item("coal"), Cooker::Smelting).speedMultiplier == 1.0F);
    CHECK(furnaces::fuelEffect(item("stone"), Cooker::Smoking).burnTicks == 0);

    // Half way through at the plain speed, the smoker's fuel halves what is left to go.
    furnaces::State state = beefAndCoal();
    state.cookingTimer = 100;
    const Recipe recipe = cooking("cooked_beef");
    CHECK(furnaces::tick(state, bookOf(recipe, "beef"), furnaces::kDefaultStackSize, Cooker::Smoking).moved);
    CHECK(state.litTimeRemaining == kCoalTicks / 2);
    CHECK(state.speedMultiplier == 2.0F);
    CHECK(state.cookingTotalTime == 100);
    // parity: `ceil(100 / 200 * 100)` is 50, then the tick's own step.
    CHECK(state.cookingTimer == 51);
    CHECK(furnaces::cookTimeFor(state, bookOf(recipe, "beef")) == 100);
}

TEST_CASE("fuel left in the slot drops the remainder rather than keeping it") {
    furnaces::State state = beefAndCoal();
    state.items[furnaces::kFuelSlot] = stack("lava_bucket", 2);
    const Recipe recipe = cooking("cooked_beef");
    std::vector<ItemStack> dropped;
    CHECK(furnaces::tick(state, bookOf(recipe, "beef"), furnaces::kDefaultStackSize, Cooker::Smelting,
                         [&dropped](const ItemStack& remainder) { dropped.push_back(remainder); })
              .moved);
    REQUIRE(dropped.size() == 1);
    CHECK(dropped.front().item == item("bucket"));
    CHECK(state.items[furnaces::kFuelSlot]->item == item("lava_bucket"));
}

TEST_CASE("a furnace that was never saved reads as a cold empty one") {
    const nbt::NbtCompound data;
    const furnaces::State state = furnaces::read(data);

    CHECK(state.litTimeRemaining == 0);
    CHECK(state.litTotalTime == 0);
    CHECK(state.cookingTimer == 0);
    CHECK(state.cookingTotalTime == 0);
    REQUIRE(state.items.size() == 3);
    CHECK_FALSE(state.items[furnaces::kInputSlot].has_value());
}

TEST_CASE("only the fuel, the smelt and the fire flipping count as a change") {
    const Recipe recipe = cooking("cooked_beef");
    furnaces::State state = beefAndCoal();

    // parity: lighting the fire consumes the fuel and flips `lit`, both of which set `changed`.
    CHECK(furnaces::tick(state, bookOf(recipe, "beef")).changed);

    // A furnace merely counting down moves its numbers and changes nothing vanilla would save.
    const furnaces::Ticked counting = furnaces::tick(state, bookOf(recipe, "beef"));
    CHECK(counting.moved);
    CHECK_FALSE(counting.changed);

    // The tick the smelt finishes on is a change again.
    state.cookingTimer = state.cookingTotalTime - 1;
    CHECK(furnaces::tick(state, bookOf(recipe, "beef")).changed);
}

TEST_CASE("each smelt is banked by recipe name and saved with the furnace") {
    Recipe steak = cooking("cooked_beef");
    steak.id = "minecraft:cooked_beef";
    steak.experience = 0.35F;
    furnaces::State state = beefAndCoal();
    state.items[furnaces::kInputSlot] = stack("beef", 2);
    state.cookingTotalTime = 1;
    steak.cookingTime = 1;
    static_cast<void>(furnaces::tick(state, bookOf(steak, "beef")));
    static_cast<void>(furnaces::tick(state, bookOf(steak, "beef")));
    REQUIRE(state.recipesUsed.size() == 1);
    CHECK(state.recipesUsed.front().second == 2);

    // parity: `RecipesUsed`, a compound of counts by recipe name.
    nbt::NbtCompound data;
    furnaces::write(state, data);
    CHECK(data.getCompound("RecipesUsed")->getInt("minecraft:cooked_beef") == 2);
    CHECK(furnaces::recipesUsedIn(data) == state.recipesUsed);

    // Two steaks at 0.35 are 0.7: nothing whole, and a 70% chance of one.
    basalt::server::random::RandomSource random = basalt::server::random::RandomSource::legacy(9);
    std::int32_t paid = 0;
    for (std::int32_t roll = 0; roll < 1000; ++roll) {
        paid += furnaces::experienceFor(2, 0.35F, random);
    }
    CHECK(paid > 600);
    CHECK(paid < 800);
    CHECK(furnaces::experienceFor(4, 1.0F, random) == 4);
}
