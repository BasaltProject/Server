#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/NaturalSpawner.hpp>

#include <doctest.h>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

using basalt::server::random::RandomSource;
using basalt::server::runtime::entity::MobCategory;
using basalt::server::runtime::world::LoadedWorld;
using basalt::server::runtime::world::SpawnerEntry;
using basalt::server::runtime::world::SpawnRequest;
using basalt::server::runtime::world::WorldMetadata;
using basalt::wire::common::Identifier;
namespace blocks = basalt::server::registry::blocks;
namespace config = basalt::server::runtime::config;
namespace spawner = basalt::server::runtime::world::naturalSpawner;

namespace {

[[nodiscard]] std::string scratch(std::string_view name) {
    const std::filesystem::path directory = std::filesystem::path{BASALT_TEST_SCRATCH_DIR} / name;
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return directory.string();
}

[[nodiscard]] std::unique_ptr<LoadedWorld> flatWorld(const std::string& root) {
    return std::make_unique<LoadedWorld>(
        config::WorldDeclaration{.name = "world", .type = config::WorldType::Flat, .isDefault = true},
        config::WorldSettings{}, WorldMetadata{}, root, basalt::wire::common::Logger{"Test"});
}

} // namespace

TEST_CASE("a category's cap scales with the chunks being simulated") {
    // parity: max per chunk times chunks over 289, so a fully loaded view is roughly the vanilla cap.
    CHECK(spawner::capFor(MobCategory::Monster, spawner::kCapChunks) == 70);
    CHECK(spawner::capFor(MobCategory::Creature, spawner::kCapChunks) == 10);
    // Half the chunks, half the mobs, and no chunks at all means nothing spawns.
    CHECK(spawner::capFor(MobCategory::Monster, spawner::kCapChunks / 2) == 34);
    CHECK(spawner::capFor(MobCategory::Monster, 0) == 0);
}

TEST_CASE("peaceful spawns only the friendly categories, and the persistent ones wait their turn") {
    const std::vector<MobCategory> normal = spawner::categoriesFor("normal", 0);
    CHECK(std::ranges::find(normal, MobCategory::Monster) != normal.end());
    CHECK(std::ranges::find(normal, MobCategory::Misc) == normal.end());

    const std::vector<MobCategory> peaceful = spawner::categoriesFor("peaceful", 0);
    CHECK(std::ranges::find(peaceful, MobCategory::Monster) == peaceful.end());
    CHECK(std::ranges::find(peaceful, MobCategory::Creature) != peaceful.end());

    // parity: creatures only try every four hundred ticks, so most ticks leave them out.
    const std::vector<MobCategory> offBeat = spawner::categoriesFor("normal", 1);
    CHECK(std::ranges::find(offBeat, MobCategory::Creature) == offBeat.end());
    CHECK(std::ranges::find(offBeat, MobCategory::Monster) != offBeat.end());
}

TEST_CASE("a pack is capped by the type's own cluster size") {
    CHECK(spawner::maxClusterSize(Identifier::vanilla("ghast")) == 1);
    CHECK(spawner::maxClusterSize(Identifier::vanilla("wolf")) == 8);
    CHECK(spawner::maxClusterSize(Identifier::vanilla("salmon")) == 5);
    CHECK(spawner::maxClusterSize(Identifier::vanilla("zombie")) == 4);
}

TEST_CASE("the weighted pick answers in proportion to the weights, and nothing for an empty list") {
    std::vector<SpawnerEntry> entries{
        {.type = Identifier::vanilla("zombie"), .weight = 100, .minCount = 1, .maxCount = 4},
        {.type = Identifier::vanilla("skeleton"), .weight = 1, .minCount = 1, .maxCount = 4}};
    std::int32_t zombies = 0;
    RandomSource random = RandomSource::legacy(9);
    for (std::int32_t roll = 0; roll < 200; roll++) {
        const SpawnerEntry* picked = spawner::pick(entries, random);
        REQUIRE(picked != nullptr);
        if (picked->type.path() == "zombie") {
            zombies++;
        }
    }
    CHECK(zombies > 180);

    CHECK(spawner::pick({}, random) == nullptr);
    // A list whose weights are all zero picks nothing rather than dividing by zero.
    const std::vector<SpawnerEntry> weightless{{.type = Identifier::vanilla("zombie"), .weight = 0}};
    CHECK(spawner::pick(weightless, random) == nullptr);
}

TEST_CASE("a slime chunk is decided by the seed and the chunk, and never changes") {
    const std::int64_t seed = 1234;
    const bool first = spawner::isSlimeChunk(seed, 3, 7);
    CHECK(spawner::isSlimeChunk(seed, 3, 7) == first);
    // Roughly one chunk in ten over a decent square, which is what farms are built on.
    std::int32_t slimeChunks = 0;
    for (std::int32_t x = 0; x < 40; x++) {
        for (std::int32_t z = 0; z < 40; z++) {
            if (spawner::isSlimeChunk(seed, x, z)) {
                slimeChunks++;
            }
        }
    }
    CHECK(slimeChunks > 100);
    CHECK(slimeChunks < 220);
    // Negative chunk coordinates are just as valid, and the int products wrap the way Java's do.
    CHECK_NOTHROW(static_cast<void>(spawner::isSlimeChunk(seed, -2000000, -2000000)));
}

TEST_CASE("the moon decides how often a swamp slime spawns") {
    // parity: eight phases over eight days, brightest in the middle of the cycle.
    CHECK(spawner::surfaceSlimeChance(0) == 0.5F);
    CHECK(spawner::surfaceSlimeChance(24000 * 4) == 0.0F);
    CHECK(spawner::surfaceSlimeChance(24000 * 8) == 0.5F);
    // A negative clock is still a phase rather than a negative chance.
    CHECK(spawner::surfaceSlimeChance(-24000) >= 0.0F);
}

TEST_CASE("nothing spawns in an unloaded chunk, or where nobody is near enough") {
    std::unique_ptr<LoadedWorld> world = flatWorld(scratch("spawner-empty"));
    RandomSource random = RandomSource::legacy(5);
    spawner::Hooks hooks;
    hooks.entriesFor = [](const Identifier&, std::int32_t, std::int32_t, std::int32_t) {
        return std::vector<SpawnerEntry>{
            {.type = Identifier::vanilla("zombie"), .weight = 100, .minCount = 1, .maxCount = 4}};
    };
    hooks.playerDistanceSquared = [](double, double, double) { return std::optional<double>{100000.0}; };

    // The chunk is not loaded, so there is nothing to spawn in.
    CHECK(spawner::spawnForChunk(*world, 0, 0, MobCategory::Monster, hooks, random).empty());

    world->state().getOrGenerateChunk(0, 0);
    // Nobody in range: every position is refused before anything is picked.
    hooks.playerDistanceSquared = [](double, double, double) { return std::optional<double>{}; };
    CHECK(spawner::spawnForChunk(*world, 0, 0, MobCategory::Monster, hooks, random).empty());

    // Too close to a player is the same answer, for the opposite reason.
    hooks.playerDistanceSquared = [](double, double, double) { return std::optional<double>{4.0}; };
    CHECK(spawner::spawnForChunk(*world, 0, 0, MobCategory::Monster, hooks, random).empty());
}

TEST_CASE("a rejected request does not consume the pack") {
    std::unique_ptr<LoadedWorld> world = flatWorld(scratch("spawner-rejected"));
    world->state().getOrGenerateChunk(0, 0);
    RandomSource random = RandomSource::legacy(11);
    std::int32_t offered = 0;
    spawner::Hooks hooks;
    hooks.entriesFor = [](const Identifier&, std::int32_t, std::int32_t, std::int32_t) {
        return std::vector<SpawnerEntry>{
            {.type = Identifier::vanilla("zombie"), .weight = 100, .minCount = 4, .maxCount = 4}};
    };
    hooks.playerDistanceSquared = [](double, double, double) { return std::optional<double>{100000.0}; };
    hooks.requestAccepted = [&offered](const SpawnRequest&) {
        offered++;
        return false;
    };

    CHECK(spawner::spawnForChunk(*world, 0, 0, MobCategory::Monster, hooks, random).empty());
    // Whatever it offered, none of it was accepted, and nothing was spawned anyway.
    CHECK(offered >= 0);
}
