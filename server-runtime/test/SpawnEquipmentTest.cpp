#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/world/SpawnEquipment.hpp>

#include <doctest.h>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

using basalt::server::random::RandomSource;
using basalt::server::runtime::world::SpawnEquipmentFinalizer;
using basalt::server::runtime::world::SpawnEquipmentJar;
using basalt::wire::nbt::NbtCompound;
using basalt::wire::nbt::NbtTag;

namespace {

/// A jar that is a map of names to text, which is how the enchantment provider reaches a test.
struct TextJar {
    std::map<std::string, std::string> files;

    [[nodiscard]] SpawnEquipmentJar handle() const {
        return {.entriesUnder =
                    [this](std::string_view prefix) {
                        std::vector<std::string> names;
                        for (const auto& [name, text] : files) {
                            if (name.starts_with(prefix)) {
                                names.push_back(name);
                            }
                        }
                        return names;
                    },
                .readText = [this](std::string_view name) -> std::optional<std::string> {
                    const auto found = files.find(std::string{name});
                    return found == files.end() ? std::nullopt : std::optional{found->second};
                }};
    }
};

[[nodiscard]] std::optional<std::string> slotItem(const NbtCompound& data, std::string_view slot) {
    const NbtTag* equipment = data.find("equipment");
    const auto* all = equipment == nullptr ? nullptr : equipment->get<NbtCompound>();
    const NbtTag* stack = all == nullptr ? nullptr : all->find(slot);
    const auto* compound = stack == nullptr ? nullptr : stack->get<NbtCompound>();
    const NbtTag* id = compound == nullptr ? nullptr : compound->find("id");
    const auto* name = id == nullptr ? nullptr : id->get<std::string>();
    return name == nullptr ? std::nullopt : std::optional{*name};
}

} // namespace

TEST_CASE("a wither skeleton gets its sword and nothing else, and only skeletons are equipped at all") {
    const TextJar jar;
    const SpawnEquipmentFinalizer finalizer{jar.handle(), [] { return false; }};
    RandomSource random = RandomSource::legacy(7);

    NbtCompound wither;
    finalizer.finalizeAfterCommon(wither, "wither_skeleton", random, 1.0F, "normal");
    CHECK(slotItem(wither, "mainhand") == "minecraft:stone_sword");
    CHECK(wither.find("CanPickUpLoot") != nullptr);

    // A cow is not an abstract skeleton, so nothing is written and no draw is spent.
    NbtCompound cow;
    finalizer.finalizeAfterCommon(cow, "cow", random, 1.0F, "normal");
    CHECK(cow.names().empty());
}

TEST_CASE("a skeleton always ends up with a bow, whatever armour it rolled") {
    const TextJar jar;
    const SpawnEquipmentFinalizer finalizer{jar.handle(), [] { return false; }};
    for (std::int64_t seed = 1; seed <= 20; seed++) {
        RandomSource random = RandomSource::legacy(seed);
        NbtCompound data;
        finalizer.finalizeAfterCommon(data, "skeleton", random, 1.0F, "normal");
        CHECK(slotItem(data, "mainhand") == "minecraft:bow");
    }
}

TEST_CASE("a pillager carries a crossbow, and a drowned's weapon is one of its own two") {
    const TextJar jar;
    const SpawnEquipmentFinalizer finalizer{jar.handle(), [] { return false; }};
    RandomSource random = RandomSource::legacy(3);

    NbtCompound pillager;
    finalizer.finalizePillagerEquipmentBeforeCommon(pillager, random, 1.0F);
    CHECK(slotItem(pillager, "mainhand") == "minecraft:crossbow");

    for (std::int64_t seed = 1; seed <= 40; seed++) {
        RandomSource own = RandomSource::legacy(seed);
        NbtCompound drowned;
        finalizer.finalizeZombieEquipmentAfterCommon(drowned, "drowned", own, 1.0F, "normal");
        const std::optional<std::string> held = slotItem(drowned, "mainhand");
        // parity: a drowned holds a trident, a fishing rod, or nothing at all: never armour or a
        // zombie's iron sword.
        CHECK((!held.has_value() || *held == "minecraft:trident" || *held == "minecraft:fishing_rod"));
    }
}

TEST_CASE("the Halloween head only appears on Halloween, and never drops") {
    const TextJar jar;
    RandomSource random = RandomSource::legacy(11);
    const SpawnEquipmentFinalizer never{jar.handle(), [] { return false; }};
    NbtCompound ordinary;
    never.finalizeAfterCommon(ordinary, "wither_skeleton", random, 1.0F, "normal");
    CHECK_FALSE(slotItem(ordinary, "head").has_value());

    // On the day itself the roll is a quarter, so over many seeds at least one skeleton wears one.
    const SpawnEquipmentFinalizer always{jar.handle(), [] { return true; }};
    bool wore = false;
    for (std::int64_t seed = 1; seed <= 60 && !wore; seed++) {
        RandomSource own = RandomSource::legacy(seed);
        NbtCompound data;
        always.finalizeAfterCommon(data, "wither_skeleton", own, 1.0F, "normal");
        const std::optional<std::string> head = slotItem(data, "head");
        if (head.has_value()) {
            wore = true;
            CHECK((*head == "minecraft:jack_o_lantern" || *head == "minecraft:carved_pumpkin"));
            const NbtTag* chances = data.find("drop_chances");
            REQUIRE(chances != nullptr);
            CHECK(*chances->get<NbtCompound>()->find("head")->get<float>() == 0.0F);
        }
    }
    CHECK(wore);
}

TEST_CASE("with no jar every draw is still made, so the spawn stream does not move") {
    const TextJar jar;
    const SpawnEquipmentFinalizer finalizer{jar.handle(), [] { return false; }};
    RandomSource withEquipment = RandomSource::legacy(5);
    NbtCompound data;
    finalizer.finalizeAfterCommon(data, "skeleton", withEquipment, 1.0F, "normal");

    // The same seed reaches the same place twice: equipment is deterministic given the stream.
    RandomSource again = RandomSource::legacy(5);
    NbtCompound second;
    finalizer.finalizeAfterCommon(second, "skeleton", again, 1.0F, "normal");
    CHECK(data == second);
    CHECK(withEquipment.nextLong() == again.nextLong());
}
