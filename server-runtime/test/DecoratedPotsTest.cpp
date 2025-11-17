#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/world/BlockInventory.hpp>
#include <basalt/server/runtime/world/DecoratedPots.hpp>

#include <doctest.h>

#include <string>

using namespace basalt::server::runtime::world;
using basalt::server::world::loot::ItemStack;
namespace nbt = basalt::wire::nbt;

namespace {

[[nodiscard]] basalt::wire::common::Identifier item(const std::string& path) {
    return basalt::wire::common::Identifier::vanilla(path);
}

[[nodiscard]] const blockEffect::Sound& soundOf(const std::vector<BlockEffect>& effects) {
    REQUIRE(effects.size() == 2);
    const blockEffect::Sound* sound = std::get_if<blockEffect::Sound>(&effects[1]);
    REQUIRE(sound != nullptr);
    return *sound;
}

[[nodiscard]] const blockEffect::Event& eventOf(const std::vector<BlockEffect>& effects) {
    REQUIRE(!effects.empty());
    const blockEffect::Event* event = std::get_if<blockEffect::Event>(&effects[0]);
    REQUIRE(event != nullptr);
    return *event;
}

} // namespace

TEST_CASE("only the decorated pot is a pot") {
    CHECK(decoratedPots::isPot("decorated_pot"));
    CHECK_FALSE(decoratedPots::isPot("flower_pot"));
    CHECK_FALSE(decoratedPots::isPot(""));
    CHECK(decoratedPots::block().toString() == "minecraft:decorated_pot");
}

TEST_CASE("an empty pot takes the first item and wobbles forward") {
    nbt::NbtCompound data;

    const std::optional<std::vector<BlockEffect>> effects = decoratedPots::insert(data, item("emerald"));

    REQUIRE(effects.has_value());
    CHECK(eventOf(*effects).b0 == 1);
    CHECK(eventOf(*effects).b1 == 0);
    CHECK(soundOf(*effects).name == "block.decorated_pot.insert");
    const std::optional<ItemStack> held = decoratedPots::contents(data);
    REQUIRE(held.has_value());
    CHECK(held->item == item("emerald"));
    CHECK(held->count == 1);
}

TEST_CASE("the pitch rises as the pot fills") {
    nbt::NbtCompound data;
    float first = 0.0F;
    float last = 0.0F;
    for (std::int32_t count = 1; count <= 64; ++count) {
        const std::optional<std::vector<BlockEffect>> effects = decoratedPots::insert(data, item("emerald"));
        REQUIRE(effects.has_value());
        if (count == 1) {
            first = soundOf(*effects).pitch;
        }
        last = soundOf(*effects).pitch;
    }

    // parity: seven tenths plus half of how full it is, so a full pot is a fifth higher.
    CHECK(first == doctest::Approx(0.7F + (0.5F / 64.0F)));
    CHECK(last == doctest::Approx(1.2F));
    CHECK(decoratedPots::contents(data)->count == 64);
}

TEST_CASE("a full pot takes nothing more, and neither does one holding something else") {
    nbt::NbtCompound data;
    for (std::int32_t count = 0; count < 64; ++count) {
        REQUIRE(decoratedPots::insert(data, item("emerald")).has_value());
    }

    CHECK_FALSE(decoratedPots::insert(data, item("emerald")).has_value());
    CHECK_FALSE(decoratedPots::insert(data, item("diamond")).has_value());
    CHECK(decoratedPots::contents(data)->count == 64);

    nbt::NbtCompound other;
    REQUIRE(decoratedPots::insert(other, item("emerald")).has_value());
    CHECK_FALSE(decoratedPots::insert(other, item("diamond")).has_value());
}

TEST_CASE("an empty hand only makes it wobble the other way") {
    const std::vector<BlockEffect> effects = decoratedPots::rebuff();

    CHECK(eventOf(effects).b0 == 1);
    CHECK(eventOf(effects).b1 == 1);
    CHECK(soundOf(effects).name == "block.decorated_pot.insert_fail");
    CHECK(soundOf(effects).pitch == doctest::Approx(1.0F));
}

TEST_CASE("a hopper takes one item at a time until the pot is empty") {
    nbt::NbtCompound data;
    REQUIRE(decoratedPots::insert(data, item("emerald")).has_value());
    REQUIRE(decoratedPots::insert(data, item("emerald")).has_value());

    const std::optional<ItemStack> first = decoratedPots::takeOne(data);
    REQUIRE(first.has_value());
    CHECK(first->item == item("emerald"));
    CHECK(first->count == 1);
    CHECK(decoratedPots::contents(data)->count == 1);

    const std::optional<ItemStack> second = decoratedPots::takeOne(data);
    REQUIRE(second.has_value());
    CHECK(second->count == 1);
    CHECK_FALSE(decoratedPots::contents(data).has_value());

    CHECK_FALSE(decoratedPots::takeOne(data).has_value());
}
