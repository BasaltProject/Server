#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/BlockChangeBroadcaster.hpp>
#include <basalt/server/runtime/ChunkSimulationView.hpp>
#include <basalt/server/runtime/CombatSystem.hpp>
#include <basalt/server/runtime/ExplosionSystem.hpp>
#include <basalt/server/runtime/MobActionSystem.hpp>
#include <basalt/server/runtime/NaturalSpawningSystem.hpp>
#include <basalt/server/runtime/RaidSystem.hpp>
#include <basalt/server/runtime/entity/Projectiles.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
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

TEST_CASE("a mob hitting another one leaves it hurt, panicking and remembering who did it") {
    ActionFixture fixture{scratch("actions-hurt")};
    ServerEntity& zombie = fixture.spawn("zombie", 8.0, 64.0, 8.0);
    ServerEntity& cow = fixture.spawn("cow", 9.0, 64.0, 8.0);
    const double before = cow.health;

    fixture.actions.hurtMob(zombie, cow, 3.0F);
    CHECK(cow.health < before);
    REQUIRE(cow.hurtBy.has_value());
    CHECK(*cow.hurtBy == zombie.entityId());
    CHECK(cow.panicTicks > 0);
}

TEST_CASE("a ghast charging is told to the world once and not again") {
    ActionFixture fixture{scratch("actions-charging")};
    ServerEntity& ghast = fixture.spawn("ghast", 8.0, 64.0, 8.0);
    fixture.raid.spawn.base.packets.clear();

    fixture.actions.setCharging(ghast, true);
    CHECK(ghast.charging);
    CHECK(fixture.raid.spawn.base.sentCount(play::SetEntityDataPacket::kId) == 1);

    // Saying the same thing twice is not worth a packet.
    fixture.actions.setCharging(ghast, true);
    CHECK(fixture.raid.spawn.base.sentCount(play::SetEntityDataPacket::kId) == 1);

    fixture.actions.setCharging(ghast, false);
    CHECK_FALSE(ghast.charging);
    CHECK(fixture.raid.spawn.base.sentCount(play::SetEntityDataPacket::kId) == 2);
}

TEST_CASE("a sheep eating grass leaves dirt behind") {
    ActionFixture fixture{scratch("actions-grass")};
    LoadedWorld& world = *fixture.raid.spawn.base.world;
    const std::int32_t grass = blocks::named("grass_block")->stateId;
    const std::int32_t dirt = blocks::named("dirt")->stateId;
    REQUIRE(world.state().setBlockState(8, 63, 8, grass));

    ServerEntity& sheep = fixture.spawn("sheep", 8.5, 64.0, 8.5);
    fixture.actions.eatGrass(sheep);
    CHECK(world.state().blockState(8, 63, 8) == dirt);
}

TEST_CASE("a sheep standing in short grass eats that instead of the block below") {
    ActionFixture fixture{scratch("actions-short-grass")};
    LoadedWorld& world = *fixture.raid.spawn.base.world;
    const std::int32_t grass = blocks::named("grass_block")->stateId;
    REQUIRE(world.state().setBlockState(8, 63, 8, grass));
    REQUIRE(world.state().setBlockState(8, 64, 8, blocks::named("short_grass")->stateId));

    ServerEntity& sheep = fixture.spawn("sheep", 8.5, 64.0, 8.5);
    fixture.actions.eatGrass(sheep);
    CHECK(world.state().blockState(8, 64, 8) == blocks::air().stateId);
    // The block below is left alone: only one of the two is eaten.
    CHECK(world.state().blockState(8, 63, 8) == grass);
}

TEST_CASE("two animals that breed leave a calf where they stood") {
    ActionFixture fixture{scratch("actions-breed")};
    ServerEntity& cow = fixture.spawn("cow", 8.0, 64.0, 8.0);
    ServerEntity& partner = fixture.spawn("cow", 8.5, 64.0, 8.0);
    const std::size_t before = fixture.raid.spawn.base.world->state().entities().size();

    fixture.actions.breed(cow, partner);
    const std::vector<ServerEntity*> after = fixture.raid.spawn.base.world->state().entities();
    REQUIRE(after.size() == before + 1);
    const ServerEntity* baby = after.back();
    CHECK(baby->type().path() == "cow");
    // parity: `Animal.setBaby`, twenty minutes of being a calf.
    CHECK(baby->age < 0);
}

TEST_CASE("a plugin that refuses a breeding leaves the pair childless") {
    ActionFixture fixture{scratch("actions-breed-refused")};
    ServerEntity& cow = fixture.spawn("cow", 8.0, 64.0, 8.0);
    ServerEntity& partner = fixture.spawn("cow", 8.5, 64.0, 8.0);
    const std::size_t before = fixture.raid.spawn.base.world->state().entities().size();

    MobActionSystem refusing{
        fixture.raid.spawn.base.registry,
        fixture.raid.broadcaster,
        fixture.explosions,
        fixture.raid.spawning,
        fixture.combat,
        fixture.raid.raids,
        fixture.raid.tables,
        [&fixture] { return std::vector<LoadedWorld*>{fixture.raid.spawn.base.world.get()}; },
        {.breedAllowed = [](const LoadedWorld&, const ServerEntity&, const ServerEntity&) { return false; }},
        {}};
    refusing.breed(cow, partner);
    CHECK(fixture.raid.spawn.base.world->state().entities().size() == before);
}
