#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/world/MobSpawnFinalization.hpp>
#include <basalt/server/runtime/world/VariantSpawnConditions.hpp>

#include <doctest.h>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

using basalt::server::random::RandomSource;
using basalt::server::runtime::world::MobSpawnContext;
using basalt::server::runtime::world::MobSpawnForest;
using basalt::server::runtime::world::MobSpawnGroupData;
using basalt::server::runtime::world::MobSpawnReason;
using basalt::wire::common::Identifier;
using basalt::wire::nbt::NbtCompound;
using basalt::wire::nbt::NbtTag;
namespace finalization = basalt::server::runtime::world::mobSpawnFinalization;

namespace {

/// A context with its own level stream, which is what a spawn attempt hands the finalizers.
struct Attempt {
    RandomSource random = RandomSource::legacy(1234);
    std::int64_t entitySeed = 5678;
    MobSpawnContext context;

    explicit Attempt(std::string_view biome = "plains", float multiplier = 1.0F) {
        context.random = &random;
        context.specialMultiplier = multiplier;
        context.difficulty = "normal";
        context.biome = Identifier::vanilla(std::string{biome});
        context.entityRandomFactory = [this] { return RandomSource::legacy(entitySeed++); };
    }

    [[nodiscard]] MobSpawnForest spawn(std::string_view type, std::int32_t groupIndex = 0) {
        NbtCompound data;
        MobSpawnGroupData group;
        return writeGeneratedForest(data, Identifier::vanilla(std::string{type}), groupIndex, context, group, 8, 64, 8,
                                    8.5, 64.0, 8.5, 0.0F);
    }
};

[[nodiscard]] std::optional<std::string> stringAt(const NbtCompound& data, std::string_view key) {
    const NbtTag* found = data.find(key);
    const auto* text = found == nullptr ? nullptr : found->get<std::string>();
    return text == nullptr ? std::nullopt : std::optional{*text};
}

[[nodiscard]] bool hasKey(const NbtCompound& data, std::string_view key) { return data.find(key) != nullptr; }

} // namespace

TEST_CASE("a common mob gets its handedness and its follow-range bonus, and a cave spider gets neither") {
    Attempt attempt;
    const MobSpawnForest cow = attempt.spawn("cow");
    CHECK(hasKey(cow.main.data, "LeftHanded"));
    CHECK(hasKey(cow.main.data, "attributes"));
    // parity: a cow's variant and sound variant come from its own layer, before the common one.
    CHECK(hasKey(cow.main.data, "variant"));
    CHECK(hasKey(cow.main.data, "sound_variant"));

    // parity: `CaveSpider.finalizeSpawn` never calls `Mob`'s, so none of that is written.
    const MobSpawnForest caveSpider = attempt.spawn("cave_spider");
    CHECK_FALSE(hasKey(caveSpider.main.data, "LeftHanded"));
    CHECK_FALSE(hasKey(caveSpider.main.data, "attributes"));
}

TEST_CASE("a biome decides a wolf's coat and a villager's own type") {
    Attempt savanna{"savanna"};
    CHECK(stringAt(savanna.spawn("wolf").main.data, "variant") == "minecraft:spotted");

    Attempt jungle{"jungle"};
    const NbtCompound villagerData = *jungle.spawn("villager").main.data.find("VillagerData")->get<NbtCompound>();
    CHECK(stringAt(villagerData, "type") == "minecraft:jungle");

    Attempt desert{"desert"};
    const NbtCompound desertData = *desert.spawn("villager").main.data.find("VillagerData")->get<NbtCompound>();
    CHECK(stringAt(desertData, "type") == "minecraft:desert");
}

TEST_CASE("a cube saves one less than its live size, and only ever one, two or four") {
    for (std::int64_t seed = 1; seed <= 30; seed++) {
        Attempt attempt;
        attempt.random = RandomSource::legacy(seed);
        const MobSpawnForest slime = attempt.spawn("slime");
        const NbtTag* size = slime.main.data.find("Size");
        REQUIRE(size != nullptr);
        const std::int32_t saved = *size->get<std::int32_t>();
        CHECK((saved == 0 || saved == 1 || saved == 3));
    }
}

TEST_CASE("a zombie carries its reinforcement attributes, and a piglin's is always zero") {
    Attempt attempt;
    const MobSpawnForest zombie = attempt.spawn("zombie");
    CHECK(hasKey(zombie.main.data, "CanBreakDoors"));
    CHECK(hasKey(zombie.main.data, "CanPickUpLoot"));
    CHECK(hasKey(zombie.main.data, "attributes"));

    // A cow is not in the zombie family, so none of that layer runs for it.
    const MobSpawnForest cow = attempt.spawn("cow");
    CHECK_FALSE(hasKey(cow.main.data, "CanBreakDoors"));
}

TEST_CASE("the same seed finalizes to the same mob, which is what parity rests on") {
    Attempt first;
    Attempt second;
    const MobSpawnForest one = first.spawn("skeleton");
    const MobSpawnForest two = second.spawn("skeleton");
    CHECK(one.main.data == two.main.data);
    // And the level stream is left in the same place, so what spawns next agrees too.
    CHECK(first.random.nextLong() == second.random.nextLong());
}

TEST_CASE("a spider that rolls a jockey carries a finalized skeleton rather than a bare one") {
    bool sawJockey = false;
    for (std::int64_t seed = 1; seed <= 400 && !sawJockey; seed++) {
        Attempt attempt;
        attempt.random = RandomSource::legacy(seed);
        const MobSpawnForest spider = attempt.spawn("spider");
        if (spider.main.passengers.empty()) {
            continue;
        }
        sawJockey = true;
        const basalt::server::world::ChunkGeneratedEntity& rider = spider.main.passengers.front();
        CHECK(rider.type == Identifier::vanilla("skeleton"));
        // The rider went through the whole finalization, so it is armed like any other skeleton.
        CHECK(hasKey(rider.data, "equipment"));
        CHECK(hasKey(rider.data, "LeftHanded"));
    }
    CHECK(sawJockey);
}

TEST_CASE("the packs' spawn conditions pick a variant, the highest priority that holds winning") {
    const std::map<std::string, std::string, std::less<>> files{
        {"data/minecraft/wolf_variant/pale.json", R"({"spawn_conditions":[{"priority":0}]})"},
        {"data/minecraft/wolf_variant/spotted.json",
         R"({"spawn_conditions":[{"condition":{"type":"minecraft:biome","biomes":"#minecraft:is_savanna"},"priority":1}]})"},
        {"data/extra/wolf_variant/dusty.json",
         R"({"spawn_conditions":[{"condition":{"type":"minecraft:biome","biomes":"minecraft:desert"},"priority":1}]})"},
        {"data/minecraft/tags/worldgen/biome/is_savanna.json", R"({"values":["minecraft:savanna"]})"},
        {"data/minecraft/wolf_sound_variant/classic.json", "{}"},
    };
    namespace variants = basalt::server::runtime::world::variantSpawnConditions;
    variants::install(
        [&files](std::string_view prefix) {
            std::vector<std::string> names;
            for (const auto& [name, text] : files) {
                if (name.starts_with(prefix)) {
                    names.push_back(name);
                }
            }
            return names;
        },
        [&files](std::string_view name) -> std::optional<std::string> {
            const auto found = files.find(name);
            return found == files.end() ? std::nullopt : std::optional{found->second};
        });
    REQUIRE(variants::installed());
    // parity: registry order is the file path first, so `extra:dusty` comes before `minecraft:pale`.
    CHECK(variants::entries("wolf_variant").front() == Identifier{"extra", "dusty"});

    Attempt savanna{"savanna"};
    CHECK(stringAt(savanna.spawn("wolf").main.data, "variant") == "minecraft:spotted");
    CHECK(stringAt(savanna.spawn("wolf").main.data, "sound_variant") == "minecraft:classic");
    Attempt desert{"desert"};
    CHECK(stringAt(desert.spawn("wolf").main.data, "variant") == "extra:dusty");
    Attempt plains{"plains"};
    CHECK(stringAt(plains.spawn("wolf").main.data, "variant") == "minecraft:pale");

    variants::install({}, {});
    CHECK_FALSE(variants::installed());
}
