#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/BlockChangeBroadcaster.hpp>
#include <basalt/server/runtime/ChunkSimulationView.hpp>
#include <basalt/server/runtime/CombatSystem.hpp>
#include <basalt/server/runtime/ExplosionSystem.hpp>
#include <basalt/server/runtime/MobActionSystem.hpp>
#include <basalt/server/runtime/MobTickSystem.hpp>
#include <basalt/server/runtime/NaturalSpawningSystem.hpp>
#include <basalt/server/runtime/RaidSystem.hpp>
#include <basalt/server/runtime/entity/Projectiles.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/WanderingTraderLifetime.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/sched/RegionScheduler.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/MobEffects.hpp>
#include <basalt/server/runtime/world/RealBiomeSpawns.hpp>
#include <basalt/server/runtime/world/RealLootTables.hpp>
#include <basalt/server/runtime/world/SpawnEquipment.hpp>
#include <basalt/wire/protocol/play/EntityData.hpp>
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
using basalt::server::runtime::CombatSystem;
using basalt::server::runtime::ExplosionSystem;
using basalt::server::runtime::MobActionSystem;
using basalt::server::runtime::MobTickSystem;
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

struct ActionFixture {
    RaidFixture raid;
    ExplosionSystem explosions{raid.spawn.base.registry, raid.tables, {}, {.nextSeed = [] { return std::int64_t{3}; }}};
    CombatSystem combat{raid.broadcaster,
                        explosions,
                        {},
                        {.nextInt = [](std::int32_t) { return 0; }, .nextSeed = [] { return std::int64_t{3}; }}};
    MobActionSystem actions{raid.spawn.base.registry,
                            raid.broadcaster,
                            explosions,
                            raid.spawning,
                            combat,
                            raid.raids,
                            raid.tables,
                            [this] { return std::vector<LoadedWorld*>{raid.spawn.base.world.get()}; },
                            {},
                            {.nextSeed = [] { return std::int64_t{3}; }, .nextDouble = [] { return 0.5; }}};

    explicit ActionFixture(const std::string& root) : raid(root) {}

    [[nodiscard]] ServerEntity& spawn(std::string_view type, double x, double y, double z) {
        ServerEntity* entity =
            raid.spawning.spawnMob(*raid.spawn.base.world, Identifier::vanilla(std::string{type}), x, y, z);
        REQUIRE(entity != nullptr);
        return *entity;
    }
};

} // namespace

TEST_CASE("a passenger is ticked by its vehicle and moved onto it") {
    ActionFixture fixture{scratch("mob-tick-riders")};
    MobTickSystem ticks{fixture.actions};
    ServerEntity& spider = fixture.spawn("spider", 8.0, 64.0, 8.0);
    ServerEntity& skeleton = fixture.spawn("skeleton", 20.0, 70.0, 20.0);
    spider.passengerIds.push_back(skeleton.entityId());
    skeleton.vehicleId = spider.entityId();

    basalt::server::random::RandomSource random = basalt::server::random::RandomSource::legacy(4);
    ticks.tickTree(*fixture.raid.spawn.base.world, spider, {}, {}, {}, random);

    // parity: the jockey sits on the spider's own attachment point rather than on its head.
    CHECK(skeleton.x == doctest::Approx(spider.x));
    CHECK(skeleton.z == doctest::Approx(spider.z));
    CHECK(skeleton.y == doctest::Approx(spider.y + 0.765 - 0.7));
    CHECK(skeleton.tickCount > 0);
}

TEST_CASE("an entity riding something is left to its vehicle's turn") {
    ActionFixture fixture{scratch("mob-tick-vehicle")};
    LoadedWorld& world = *fixture.raid.spawn.base.world;
    ServerEntity& spider = fixture.spawn("spider", 8.0, 64.0, 8.0);
    ServerEntity& skeleton = fixture.spawn("skeleton", 8.0, 64.0, 8.0);

    CHECK_FALSE(MobTickSystem::hasActiveVehicle(world, skeleton));

    skeleton.vehicleId = spider.entityId();
    spider.passengerIds.push_back(skeleton.entityId());
    CHECK(MobTickSystem::hasActiveVehicle(world, skeleton));

    // A vehicle that has forgotten its rider is not a vehicle, and the rider forgets it back.
    spider.passengerIds.clear();
    CHECK_FALSE(MobTickSystem::hasActiveVehicle(world, skeleton));
    CHECK_FALSE(skeleton.vehicleId.has_value());
}

TEST_CASE("a trader counts down to its own despawn rather than waiting to be far away") {
    ActionFixture fixture{scratch("mob-tick-trader")};
    ServerEntity& trader = fixture.spawn("wandering_trader", 8.0, 64.0, 8.0);
    trader.despawnDelay = 2;

    const std::vector<ServerEntity*> living{&trader};
    basalt::server::runtime::entity::wanderingTraderLifetime::tick(trader, living, {});
    CHECK(trader.despawnDelay == 1);
    CHECK_FALSE(trader.removed);

    basalt::server::runtime::entity::wanderingTraderLifetime::tick(trader, living, {});
    CHECK(trader.removed);
}

TEST_CASE("a llama on a trader's leash goes when the trader does") {
    ActionFixture fixture{scratch("mob-tick-llama")};
    ServerEntity& trader = fixture.spawn("wandering_trader", 8.0, 64.0, 8.0);
    ServerEntity& llama = fixture.spawn("trader_llama", 9.0, 64.0, 8.0);
    trader.despawnDelay = 1;
    llama.leashHolderId = trader.entityId();
    llama.despawnDelay = 500;

    const std::vector<ServerEntity*> living{&trader, &llama};
    basalt::server::runtime::entity::wanderingTraderLifetime::tick(llama, living, {});
    // parity: the llama's own delay is replaced by its trader's, so the pair leaves together.
    CHECK(llama.despawnDelay == 0);
    CHECK(llama.removed);
}

TEST_CASE("a llama somebody owns or is riding stays put") {
    ActionFixture fixture{scratch("mob-tick-llama-kept")};
    ServerEntity& llama = fixture.spawn("trader_llama", 9.0, 64.0, 8.0);
    llama.despawnDelay = 1;
    llama.owner = "somebody";

    const std::vector<ServerEntity*> living{&llama};
    basalt::server::runtime::entity::wanderingTraderLifetime::tick(llama, living, {});
    CHECK_FALSE(llama.removed);
    CHECK(llama.despawnDelay == 1);
}
