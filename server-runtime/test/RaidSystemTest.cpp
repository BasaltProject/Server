#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/BlockChangeBroadcaster.hpp>
#include <basalt/server/runtime/ChunkSimulationView.hpp>
#include <basalt/server/runtime/NaturalSpawningSystem.hpp>
#include <basalt/server/runtime/RaidSystem.hpp>
#include <basalt/server/runtime/entity/Projectiles.hpp>
#include <basalt/server/runtime/entity/RaiderEquipment.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/sched/RegionScheduler.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/MobEffects.hpp>
#include <basalt/server/runtime/world/RealBiomeSpawns.hpp>
#include <basalt/server/runtime/world/RealLootTables.hpp>
#include <basalt/server/runtime/world/SpawnEquipment.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>
#include <basalt/wire/protocol/play/SimplePackets.hpp>

#include <doctest.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>

using basalt::server::runtime::BlockChangeBroadcaster;
using basalt::server::runtime::NaturalSpawningSystem;
using basalt::server::runtime::RaidSystem;
using basalt::server::runtime::entity::Raid;
using basalt::server::runtime::entity::ServerEntity;
using basalt::server::runtime::player::PlayerConnection;
using basalt::server::runtime::player::PlayerRegistry;
using basalt::server::runtime::player::PlayerSession;
using basalt::server::runtime::world::LoadedWorld;
using basalt::server::runtime::world::RealLootTables;
using basalt::server::runtime::world::WorldMetadata;
using basalt::wire::common::Identifier;
using basalt::wire::common::Uuid;
namespace blocks = basalt::server::registry::blocks;
namespace config = basalt::server::runtime::config;
namespace play = basalt::wire::protocol::play;

namespace {

[[nodiscard]] std::string scratch(std::string_view name) {
    const std::filesystem::path directory = std::filesystem::path{BASALT_TEST_SCRATCH_DIR} / name;
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return directory.string();
}

struct Fixture {
    PlayerRegistry registry;
    std::unique_ptr<LoadedWorld> world;
    std::unique_ptr<PlayerSession> player;
    std::vector<std::int32_t> packets;
    float hurt = 0.0F;
    std::vector<std::pair<std::string, std::int32_t>> effects;
    std::map<std::string, std::string> lootFiles;

    explicit Fixture(const std::string& root)
        : world(std::make_unique<LoadedWorld>(
              config::WorldDeclaration{.name = "world", .type = config::WorldType::Flat, .isDefault = true},
              config::WorldSettings{}, WorldMetadata{}, root, basalt::wire::common::Logger{"Test"})) {
        PlayerConnection connection;
        connection.sendPlayPacket = [this](std::int32_t id,
                                           const std::function<void(basalt::wire::io::ByteBuffer&)>& write) {
            basalt::wire::io::ByteBuffer buffer;
            write(buffer);
            packets.push_back(id);
        };
        connection.hurt = [this](float amount, std::string_view, const Identifier&,
                                 const std::optional<basalt::server::runtime::entity::DamageSource>&) {
            hurt += amount;
        };
        connection.addEffect = [this](const Identifier& effect, std::int32_t ticks, std::int32_t) {
            effects.emplace_back(effect.path(), ticks);
        };
        player = std::make_unique<PlayerSession>(Uuid{1, 1}, "Steve", 1, std::move(connection));
        player->worldName = "world";
        registry.add(player.get());
        world->state().getOrGenerateChunk(0, 0);
    }

    [[nodiscard]] std::vector<PlayerSession*> watchers() { return {player.get()}; }

    [[nodiscard]] RealLootTables::Jar jar() {
        return {.readText = [this](std::string_view name) -> std::optional<std::string> {
                    const auto found = lootFiles.find(std::string{name});
                    return found == lootFiles.end() ? std::nullopt : std::optional{found->second};
                },
                .entriesUnder = [](std::string_view) { return std::vector<std::string>{}; }};
    }

    [[nodiscard]] std::int32_t sentCount(std::int32_t id) const {
        return static_cast<std::int32_t>(std::ranges::count(packets, id));
    }
};

} // namespace

namespace {

struct SpawnFixture {
    Fixture base;
    basalt::server::runtime::world::RealBiomeSpawns biomeSpawns{
        {.readText = [](std::string_view) { return std::optional<std::string>{}; }}};
    basalt::server::runtime::world::SpawnEquipmentFinalizer equipment{
        {.entriesUnder = [](std::string_view) { return std::vector<std::string>{}; },
         .readText = [](std::string_view) { return std::optional<std::string>{}; }}};
    basalt::server::runtime::ChunkSimulationView simulation{base.registry};

    explicit SpawnFixture(const std::string& root) : base(root) {}

    [[nodiscard]] NaturalSpawningSystem system(NaturalSpawningSystem::Hooks hooks = {}) {
        return NaturalSpawningSystem{base.registry, biomeSpawns,      simulation,
                                     equipment,     std::move(hooks), {.nextSeed = [] { return std::int64_t{7}; }}};
    }
};

} // namespace

namespace {

struct RaidFixture {
    SpawnFixture spawn;
    RealLootTables tables{spawn.base.jar()};
    BlockChangeBroadcaster broadcaster{spawn.base.registry, tables, {}, {.nextSeed = [] { return std::int64_t{5}; }}};
    NaturalSpawningSystem spawning = spawn.system();
    std::vector<bool> finished;
    RaidSystem raids{spawn.base.registry,
                     broadcaster,
                     spawning,
                     {.raidFinished = [this](const LoadedWorld&, const Raid&, bool won) { finished.push_back(won); }},
                     {.nextSeed = [] { return std::int64_t{13}; }}};

    explicit RaidFixture(const std::string& root) : spawn(root) {}
};

} // namespace

namespace {

/// A claimed bell at 8, 64, 8: the sections around it are a village.
void buildVillage(LoadedWorld& world) {
    REQUIRE(world.state().setBlockState(8, 64, 8, blocks::named("bell")->stateId));
    REQUIRE(world.state().poi().acquire(basalt::server::runtime::world::poi::BlockPos{8, 64, 8}));
}

void standAt(PlayerSession& player, double x, double y, double z) {
    player.x = x;
    player.y = y;
    player.z = z;
}

} // namespace

TEST_CASE("a bad omen carried into a village turns into a raid omen") {
    RaidFixture fixture{scratch("raid-omen")};
    LoadedWorld& world = *fixture.spawn.base.world;
    buildVillage(world);
    standAt(*fixture.spawn.base.player, 8.0, 64.0, 8.0);

    basalt::server::runtime::world::ActiveEffect omen{
        .effect = basalt::server::runtime::world::mobEffects::badOmen(), .amplifier = 0, .remainingTicks = 100};
    fixture.raids.tickOmens(world, *fixture.spawn.base.player, omen);

    REQUIRE(fixture.spawn.base.effects.size() == 1);
    CHECK(fixture.spawn.base.effects.front().first == "raid_omen");
    REQUIRE(fixture.spawn.base.player->raidOmenPosition.has_value());
    CHECK((*fixture.spawn.base.player->raidOmenPosition)[0] == 8);
}

TEST_CASE("a bad omen away from any village does nothing at all") {
    RaidFixture fixture{scratch("raid-no-village")};
    basalt::server::runtime::world::ActiveEffect omen{
        .effect = basalt::server::runtime::world::mobEffects::badOmen(), .amplifier = 0, .remainingTicks = 100};
    fixture.raids.tickOmens(*fixture.spawn.base.world, *fixture.spawn.base.player, omen);
    CHECK(fixture.spawn.base.effects.empty());
    CHECK_FALSE(fixture.spawn.base.player->raidOmenPosition.has_value());
}

TEST_CASE("a raid omen running out starts the raid at the middle of the village's claimed points") {
    RaidFixture fixture{scratch("raid-start")};
    LoadedWorld& world = *fixture.spawn.base.world;
    buildVillage(world);
    fixture.spawn.base.player->raidOmenPosition = std::array<std::int32_t, 3>{20, 64, 20};
    basalt::server::runtime::world::ActiveEffect ending{
        .effect = basalt::server::runtime::world::mobEffects::raidOmen(), .amplifier = 1, .remainingTicks = 1};

    fixture.raids.tickOmens(world, *fixture.spawn.base.player, ending);
    REQUIRE(world.raids().size() == 1);
    const Raid* raid = RaidSystem::raidAt(world, 8.0, 64.0, 8.0);
    REQUIRE(raid != nullptr);
    CHECK(raid->center == basalt::server::runtime::entity::RaidPos{8, 64, 8});
    // parity: the omen's amplifier plus one.
    CHECK(raid->raidOmenLevel == 2);
    CHECK_FALSE(fixture.spawn.base.player->raidOmenPosition.has_value());
}

TEST_CASE("a raid whose last wave is gone waits two seconds, then rewards its heroes") {
    RaidFixture fixture{scratch("raid-victory")};
    LoadedWorld& world = *fixture.spawn.base.world;
    buildVillage(world);
    standAt(*fixture.spawn.base.player, 8.0, 64.0, 8.0);
    const std::int32_t id =
        world.raids().add(std::make_unique<Raid>(basalt::server::runtime::entity::RaidPos{8, 64, 8}, 3, 1));
    Raid& raid = *world.raids().get(id);
    raid.started = true;
    raid.raidOmenLevel = 1;
    raid.groupsSpawned = 3;
    raid.heroes.push_back(fixture.spawn.base.player->uuid());

    for (std::int32_t tick = 0; tick <= Raid::kPostRaidTicks; ++tick) {
        fixture.raids.tickRaids(world);
    }
    CHECK(raid.status == basalt::server::runtime::entity::RaidStatus::Victory);
    REQUIRE(fixture.spawn.base.effects.size() == 1);
    CHECK(fixture.spawn.base.effects.front().first == "hero_of_the_village");
    CHECK(fixture.finished.empty());
}

TEST_CASE("a raid whose village is gone after its first wave is lost, and before it just stops") {
    RaidFixture fixture{scratch("raid-loss")};
    LoadedWorld& world = *fixture.spawn.base.world;
    const std::int32_t lost =
        world.raids().add(std::make_unique<Raid>(basalt::server::runtime::entity::RaidPos{8, 64, 8}, 3, 1));
    world.raids().get(lost)->groupsSpawned = 1;
    const std::int32_t stopped =
        world.raids().add(std::make_unique<Raid>(basalt::server::runtime::entity::RaidPos{8, 64, 200}, 3, 2));
    world.state().getOrGenerateChunk(0, 12);

    fixture.raids.tickRaids(world);
    CHECK(world.raids().get(lost)->status == basalt::server::runtime::entity::RaidStatus::Loss);
    CHECK(world.raids().get(stopped)->isStopped());
    fixture.raids.tickRaids(world);
    CHECK(world.raids().get(stopped) == nullptr);
    REQUIRE(fixture.finished.size() == 1);
    CHECK_FALSE(fixture.finished.front());
}

TEST_CASE("a wave spawns round the village with a bannered captain, and each raider knows its raid") {
    RaidFixture fixture{scratch("raid-wave")};
    LoadedWorld& world = *fixture.spawn.base.world;
    for (std::int32_t chunkX = -2; chunkX <= 2; ++chunkX) {
        for (std::int32_t chunkZ = -2; chunkZ <= 2; ++chunkZ) {
            world.state().getOrGenerateChunk(chunkX, chunkZ);
        }
    }
    const std::int32_t surface =
        world.state().chunkIfLoaded(0, 0)->heightAt(8, 8, basalt::server::world::HeightmapType::WorldSurface);
    REQUIRE(world.state().setBlockState(8, surface, 8, blocks::named("bell")->stateId));
    REQUIRE(world.state().poi().acquire(basalt::server::runtime::world::poi::BlockPos{8, surface, 8}));
    standAt(*fixture.spawn.base.player, 8.0, surface, 8.0);
    const std::int32_t id =
        world.raids().add(std::make_unique<Raid>(basalt::server::runtime::entity::RaidPos{8, surface, 8}, 3, 9));
    Raid& raid = *world.raids().get(id);
    raid.raidOmenLevel = 1;
    raid.cooldownTicks = 0;

    fixture.raids.tickRaids(world);
    REQUIRE(raid.started);
    CHECK(raid.groupsSpawned == 1);
    // parity: the first wave is four pillagers, plus what the bonus roll adds.
    CHECK(raid.totalRaidersAlive() >= 4);
    REQUIRE(raid.leaders.contains(1));
    const ServerEntity* captain = world.state().entity(raid.leaders.at(1));
    REQUIRE(captain != nullptr);
    CHECK(captain->patrolLeader);
    CHECK(basalt::server::runtime::entity::raiderEquipment::wearsOminousBanner(*captain));
    for (const std::int32_t raiderId : raid.groups.at(1)) {
        const ServerEntity* raider = world.state().entity(raiderId);
        REQUIRE(raider != nullptr);
        CHECK(raider->raidId == std::optional<std::int32_t>{id});
        CHECK(raider->raidWave == 1);
        CHECK(raider->canJoinRaid);
    }
    CHECK(raid.totalHealth > 0.0F);
}
