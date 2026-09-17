#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/ExperienceOrbSystem.hpp>
#include <basalt/server/runtime/entity/DeathDrops.hpp>
#include <basalt/server/runtime/entity/EntityCombat.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/world/BlockExperience.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>

#include <doctest.h>

#include <algorithm>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

using basalt::server::runtime::ExperienceOrbSystem;
using basalt::server::runtime::entity::ServerEntity;
using basalt::server::runtime::player::PlayerConnection;
using basalt::server::runtime::player::PlayerRegistry;
using basalt::server::runtime::player::PlayerSession;
using basalt::server::runtime::world::LoadedWorld;
using basalt::server::runtime::world::WorldMetadata;
using basalt::wire::common::Identifier;
using basalt::wire::common::Uuid;
namespace deathDrops = basalt::server::runtime::entity::deathDrops;
namespace entityCombat = basalt::server::runtime::entity::entityCombat;
namespace config = basalt::server::runtime::config;

namespace {

[[nodiscard]] std::string scratch(std::string_view name) {
    const std::filesystem::path directory = std::filesystem::path{BASALT_TEST_SCRATCH_DIR} / name;
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return directory.string();
}

[[nodiscard]] std::vector<ServerEntity*> orbsIn(LoadedWorld& world) {
    std::vector<ServerEntity*> found;
    for (ServerEntity* entity : world.state().entities()) {
        if (!entity->removed && ExperienceOrbSystem::isOrb(*entity)) {
            found.push_back(entity);
        }
    }
    return found;
}

} // namespace

TEST_CASE("experience splits into vanilla's orb sizes and a player takes them one at a time") {
    PlayerRegistry registry;
    LoadedWorld world{config::WorldDeclaration{.name = "world", .type = config::WorldType::Flat, .isDefault = true},
                      config::WorldSettings{}, WorldMetadata{.seed = 1, .spawnX = 8, .spawnY = 64, .spawnZ = 8},
                      scratch("orbs"), basalt::wire::common::Logger{"Test"}};
    const auto ground = static_cast<double>(
        world.state().getOrGenerateChunk(0, 0).heightAt(8, 8, basalt::server::world::HeightmapType::MotionBlocking));
    ExperienceOrbSystem orbs{registry, {.nextSeed = [] { return std::int64_t{7}; }}};

    // parity: 12000 is four of 2477, then 1237, 617, 149, 73, 17, 7, 3 and 1 once each, unless two
    // of a size happen to fall in the same merge group.
    orbs.award(world, 8.5, ground + 1.0, 8.5, 12000);
    std::int32_t total = 0;
    for (const ServerEntity* orb : orbsIn(world)) {
        total += ExperienceOrbSystem::valueOf(*orb) * orb->sourceNbt->getInt("Count", 1);
    }
    CHECK(total == 12000);

    PlayerSession player{Uuid{1, 1}, "Steve", 1, PlayerConnection{}};
    player.worldName = "world";
    player.x = 8.5;
    player.y = ground;
    player.z = 8.5;
    const std::int32_t before = player.experience.total();
    std::int32_t pickups = 0;
    for (std::int32_t tick = 0; tick < 200 && !orbsIn(world).empty(); ++tick) {
        orbs.tickPlayers({&player});
        const std::int32_t points = player.experience.total();
        for (ServerEntity* orb : orbsIn(world)) {
            orbs.tick(world, {&player}, orbsIn(world), *orb);
        }
        pickups += player.experience.total() != points ? 1 : 0;
    }
    CHECK(orbsIn(world).empty());
    CHECK(player.experience.total() - before == 12000);
    // parity: `takeXpDelay` lets a player take one orb every other tick.
    CHECK(pickups >= 10);
}

TEST_CASE("a mob killed by a player is worth its class's reward") {
    basalt::server::random::RandomSource random = basalt::server::random::RandomSource::legacy(1);
    ServerEntity zombie{1, Uuid{1, 2}, Identifier::vanilla("zombie"), 0, 0.0, 0.0, 0.0};
    CHECK_FALSE(deathDrops::dropsExperience(zombie, true));
    entityCombat::rememberPlayer(zombie, Uuid{1, 1});
    CHECK(deathDrops::dropsExperience(zombie, true));
    CHECK_FALSE(deathDrops::dropsExperience(zombie, false));
    CHECK(deathDrops::experienceReward(zombie, random) == 5);
    // parity: `Zombie.getBaseExperienceReward`, a baby is worth two and a half times as much.
    zombie.age = -1;
    CHECK(deathDrops::dropsExperience(zombie, true));
    CHECK(deathDrops::experienceReward(zombie, random) == 12);

    ServerEntity calf{2, Uuid{1, 3}, Identifier::vanilla("cow"), 0, 0.0, 0.0, 0.0};
    entityCombat::rememberPlayer(calf, Uuid{1, 1});
    const std::int32_t reward = deathDrops::experienceReward(calf, random);
    CHECK((reward >= 1 && reward <= 3));
    calf.age = -1;
    CHECK_FALSE(deathDrops::dropsExperience(calf, true));
    CHECK_FALSE(deathDrops::dropsLoot(calf, true));
    CHECK(deathDrops::dropsLoot(zombie, true));

    // parity: the memory lasts `resolvePlayerResponsibleForDamage`'s hundred ticks.
    for (std::int32_t tick = 0; tick < entityCombat::kPlayerHitMemoryTicks; ++tick) {
        entityCombat::tick(zombie);
    }
    CHECK_FALSE(deathDrops::dropsExperience(zombie, true));
}

TEST_CASE("an ore is worth its range unless silk touch took it, and a spawner always pays") {
    namespace blockExperience = basalt::server::runtime::world::blockExperience;
    basalt::server::random::RandomSource random = basalt::server::random::RandomSource::legacy(4);
    for (std::int32_t roll = 0; roll < 50; ++roll) {
        const std::int32_t diamond = blockExperience::onBreak(Identifier::vanilla("diamond_ore"), false, random);
        CHECK((diamond >= 3 && diamond <= 7));
        const std::int32_t spawner = blockExperience::onBreak(Identifier::vanilla("spawner"), true, random);
        CHECK((spawner >= 15 && spawner <= 43));
    }
    CHECK(blockExperience::onBreak(Identifier::vanilla("diamond_ore"), true, random) == 0);
    CHECK(blockExperience::onBreak(Identifier::vanilla("sculk_catalyst"), false, random) == 5);
    CHECK(blockExperience::onBreak(Identifier::vanilla("iron_ore"), false, random) == 0);
}

TEST_CASE("mending spends an orb on the damage first, two points of durability a point") {
    PlayerRegistry registry;
    LoadedWorld world{config::WorldDeclaration{.name = "world", .type = config::WorldType::Flat},
                      config::WorldSettings{}, WorldMetadata{}, scratch("orbs-mending"),
                      basalt::wire::common::Logger{"Test"}};
    const double ground = static_cast<double>(
        world.state().getOrGenerateChunk(0, 0).heightAt(8, 8, basalt::server::world::HeightmapType::MotionBlocking));
    ExperienceOrbSystem orbs{registry, {.nextSeed = [] { return std::int64_t{7}; }}};
    PlayerSession player{Uuid{1, 1}, "Steve", 1, PlayerConnection{}};
    player.worldName = "world";
    player.x = 8.5;
    player.y = ground;
    player.z = 8.5;
    basalt::wire::nbt::NbtCompound enchantments;
    enchantments.putInt("minecraft:mending", 1);
    basalt::server::storage::StoredItemStack pick{.id = "minecraft:diamond_pickaxe", .count = 1, .components = {}};
    pick.components.set("minecraft:enchantments", basalt::wire::nbt::NbtTag::ofCompound(std::move(enchantments)));
    pick.components.putInt("minecraft:damage", 10);
    const std::int32_t held = basalt::server::runtime::player::PlayerInventory::kHotbarStart;
    player.inventory.set(held, pick);

    const auto pickUp = [&](std::int32_t value) {
        orbs.spawn(world, 8.5, ground + 0.5, 8.5, value);
        for (std::int32_t tick = 0; tick < 20 && !orbsIn(world).empty(); ++tick) {
            orbs.tickPlayers({&player});
            for (ServerEntity* orb : orbsIn(world)) {
                orbs.tick(world, {&player}, orbsIn(world), *orb);
            }
        }
    };
    // Three points mend six, and nothing reaches the bar.
    pickUp(3);
    CHECK(player.inventory.at(held)->components.getInt("minecraft:damage") == 4);
    CHECK(player.experience.total() == 0);
    // Seven points: two of them mend the last four, and five are left for the bar.
    pickUp(7);
    CHECK_FALSE(player.inventory.at(held)->components.contains("minecraft:damage"));
    CHECK(player.experience.total() == 5);
}
