#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/BlockChangeBroadcaster.hpp>
#include <basalt/server/runtime/ChunkSimulationView.hpp>
#include <basalt/server/runtime/CombatSystem.hpp>
#include <basalt/server/runtime/ExplosionSystem.hpp>
#include <basalt/server/runtime/MobActionSystem.hpp>
#include <basalt/server/runtime/NaturalSpawningSystem.hpp>
#include <basalt/server/runtime/RaidSystem.hpp>
#include <basalt/server/runtime/entity/EntityNbt.hpp>
#include <basalt/server/runtime/entity/GeneratedEntityPresentation.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/RealBiomeSpawns.hpp>
#include <basalt/server/runtime/world/RealEntityTypeTags.hpp>
#include <basalt/server/runtime/world/RealLootTables.hpp>
#include <basalt/server/runtime/world/SpawnEquipment.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>
#include <basalt/wire/protocol/play/WorldPackets.hpp>

#include <doctest.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using basalt::server::runtime::BlockChangeBroadcaster;
using basalt::server::runtime::ChunkSimulationView;
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
using basalt::server::runtime::world::RealBiomeSpawns;
using basalt::server::runtime::world::RealLootTables;
using basalt::server::runtime::world::SpawnEquipmentFinalizer;
using basalt::server::runtime::world::WorldMetadata;
using basalt::wire::common::Identifier;
using basalt::wire::common::Uuid;
using basalt::wire::protocol::play::GameEventPacket;
using basalt::wire::protocol::play::SetEntityDataPacket;
namespace config = basalt::server::runtime::config;
namespace entity = basalt::server::runtime::entity;
namespace entityNbt = basalt::server::runtime::entity::entityNbt;
namespace world = basalt::server::runtime::world;

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
            // A hit that lands starts the invulnerability window, which is how the sting tells.
            player->invulnerableTicks = 20;
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
};

} // namespace

namespace {

struct SpawnFixture {
    Fixture base;
    RealBiomeSpawns biomeSpawns{{.readText = [](std::string_view) { return std::optional<std::string>{}; }}};
    SpawnEquipmentFinalizer equipment{{.entriesUnder = [](std::string_view) { return std::vector<std::string>{}; },
                                       .readText = [](std::string_view) { return std::optional<std::string>{}; }}};
    ChunkSimulationView simulation{base.registry};

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

/// parity: the jar's `not_scary_for_pufferfish` tag, at least the members these tests swim with.
[[nodiscard]] world::realEntityTypeTags::TagMembers scaryTag() {
    world::realEntityTypeTags::TagMembers tags;
    tags[Identifier::vanilla("not_scary_for_pufferfish")] = {
        Identifier::vanilla("turtle"),     Identifier::vanilla("guardian"), Identifier::vanilla("cod"),
        Identifier::vanilla("pufferfish"), Identifier::vanilla("salmon"),   Identifier::vanilla("tropical_fish"),
        Identifier::vanilla("dolphin"),    Identifier::vanilla("squid"),    Identifier::vanilla("glow_squid"),
        Identifier::vanilla("tadpole")};
    return tags;
}

struct ActionFixture {
    RaidFixture raid;
    world::realEntityTypeTags::TagMembers entityTags = scaryTag();
    ExplosionSystem explosions{raid.spawn.base.registry, raid.tables, {}, {.nextSeed = [] { return std::int64_t{3}; }}};
    CombatSystem combat{raid.broadcaster,
                        explosions,
                        {},
                        {.nextInt = [](std::int32_t) { return 0; }, .nextSeed = [] { return std::int64_t{3}; }}};
    MobActionSystem actions{
        raid.spawn.base.registry,
        raid.broadcaster,
        explosions,
        raid.spawning,
        combat,
        raid.raids,
        raid.tables,
        [this] { return std::vector<LoadedWorld*>{raid.spawn.base.world.get()}; },
        {},
        {.nextSeed = [] { return std::int64_t{3}; }, .entityTags = &entityTags, .nextDouble = [] { return 0.5; }}};

    explicit ActionFixture(const std::string& root) : raid(root) {}

    [[nodiscard]] LoadedWorld& world() { return *raid.spawn.base.world; }

    [[nodiscard]] ServerEntity& spawn(std::string_view type, double x, double y, double z) {
        ServerEntity* entity = raid.spawning.spawnMob(world(), Identifier::vanilla(std::string{type}), x, y, z);
        REQUIRE(entity != nullptr);
        return *entity;
    }
};

} // namespace

TEST_CASE("a pufferfish puffs up when something scary draws near and deflates after it leaves") {
    ActionFixture fixture{scratch("pufferfish-puff")};
    ServerEntity& fish = fixture.spawn("pufferfish", 8.0, 64.0, 8.0);
    ServerEntity& zombie = fixture.spawn("zombie", 9.0, 64.0, 8.0);
    const std::vector<ServerEntity*> living{&fish, &zombie};
    const auto tick = [&] {
        fixture.actions.tickPufferfish(fixture.world(), fixture.raid.spawn.base.watchers(), fish, living);
    };

    CHECK(fish.puffState == 0);
    // parity: the PuffGoal's start only winds the counter; the first state arrives a tick later.
    tick();
    CHECK(fish.puffState == 0);
    tick();
    CHECK(fish.puffState == 1);
    // parity: `inflateCounter > 40`, so the full puff is the forty-first tick of inflating.
    for (std::int32_t i = 0; i < 40; ++i) {
        tick();
    }
    CHECK(fish.puffState == 2);

    zombie.x = 30.0;
    tick();
    // parity: sixty ticks after the last scare it lets half the air out, a hundred all of it, so
    // the sixty-first tick of deflating is the first at the middle state.
    for (std::int32_t i = 0; i < 62; ++i) {
        tick();
    }
    CHECK(fish.puffState == 1);
    for (std::int32_t i = 0; i < 40; ++i) {
        tick();
    }
    CHECK(fish.puffState == 0);
}

TEST_CASE("a puffed pufferfish stings and poisons a mob in reach but not a calm one") {
    ActionFixture fixture{scratch("pufferfish-sting")};
    ServerEntity& fish = fixture.spawn("pufferfish", 8.0, 64.0, 8.0);
    ServerEntity& cow = fixture.spawn("cow", 8.2, 64.0, 8.0);
    ServerEntity& turtle = fixture.spawn("turtle", 8.1, 64.0, 8.1);
    const std::vector<ServerEntity*> living{&fish, &cow, &turtle};
    const double cowHealth = cow.health;

    fish.puffState = 2;
    fixture.actions.tickPufferfish(fixture.world(), fixture.raid.spawn.base.watchers(), fish, living);

    // parity: `Pufferfish.touch`, one heart per puff level and a second of poison per level.
    CHECK(cow.health == doctest::Approx(cowHealth - 3.0));
    const auto poisoned = cow.effects.find(Identifier::vanilla("poison"));
    REQUIRE(poisoned != cow.effects.end());
    CHECK(poisoned->second.remainingTicks == 120);
    CHECK(poisoned->second.amplifier == 0);
    // parity: the turtle sits in `not_scary_for_pufferfish`, so the aura leaves it alone.
    CHECK(turtle.health == doctest::Approx(turtle.attributeWithModifiers("minecraft:max_health")));

    fish.puffState = 0;
    const double stung = cow.health;
    fixture.actions.tickPufferfish(fixture.world(), fixture.raid.spawn.base.watchers(), fish, living);
    CHECK(cow.health == doctest::Approx(stung));
}

TEST_CASE("a player touching a puffed pufferfish is stung, flashed and poisoned") {
    ActionFixture fixture{scratch("pufferfish-player")};
    ServerEntity& fish = fixture.spawn("pufferfish", 8.0, 64.0, 8.0);
    PlayerSession& player = *fixture.raid.spawn.base.player;
    player.x = 8.5;
    player.y = 64.0;
    player.z = 8.0;
    const std::vector<ServerEntity*> living{&fish};

    fish.puffState = 2;
    fixture.actions.tickPufferfish(fixture.world(), fixture.raid.spawn.base.watchers(), fish, living);

    // parity: the sting is `mob_attack` damage, so easy difficulty turns three into two and a half.
    CHECK(fixture.raid.spawn.base.hurt == doctest::Approx(2.5F));
    REQUIRE(fixture.raid.spawn.base.effects.size() == 1);
    CHECK(fixture.raid.spawn.base.effects[0] == std::pair{std::string{"poison"}, 120});
    // parity: `ClientboundGameEventPacket.PUFFER_FISH_STING`, the flash only the stung player gets.
    CHECK(std::ranges::count(fixture.raid.spawn.base.packets, GameEventPacket::kId) == 1);
}

TEST_CASE("a pufferfish's puff state survives a save and a load") {
    ServerEntity fish{1, Uuid{0x1234, 0x5678}, Identifier::vanilla("pufferfish"), 7, 1.5, 64.0, -2.5, 0.0F, 0.0F};
    fish.health = 20.0;
    fish.puffState = 2;

    const std::unique_ptr<ServerEntity> back = entityNbt::decode(entityNbt::encode(fish), 42, 9);
    REQUIRE(back != nullptr);
    CHECK(back->puffState == 2);

    fish.puffState = 5;
    const std::unique_ptr<ServerEntity> capped = entityNbt::decode(entityNbt::encode(fish), 42, 9);
    REQUIRE(capped != nullptr);
    // parity: `Pufferfish.readAdditionalSaveData`, `Math.min(PuffState, 2)`.
    CHECK(capped->puffState == 2);
}

TEST_CASE("a pufferfish's box follows how far it has puffed up") {
    ActionFixture fixture{scratch("pufferfish-box")};
    ServerEntity& fish = fixture.spawn("pufferfish", 8.0, 64.0, 8.0);
    // parity: `Pufferfish.getDefaultDimensions` over a base box of 0.7: half, seven tenths, whole.
    CHECK(fish.width() == doctest::Approx(0.35));
    fish.puffState = 1;
    CHECK(fish.width() == doctest::Approx(0.49));
    fish.puffState = 2;
    CHECK(fish.width() == doctest::Approx(0.7));
}

TEST_CASE("a pufferfish's puff state reaches the clients that can see it") {
    ActionFixture fixture{scratch("pufferfish-metadata")};
    ServerEntity& fish = fixture.spawn("pufferfish", 8.0, 64.0, 8.0);
    const std::vector<ServerEntity*> living{&fish};

    // A deflated fish carries the defined zero, so nothing is news until it puffs.
    const entity::RuntimeEntityMetadata deflated = entity::runtimeEntityMetadata(fish);
    for (const SetEntityDataPacket::Entry& entry : deflated.changesFrom(nullptr)) {
        CHECK(entry.index != 17);
    }

    fish.puffState = 2;
    const entity::RuntimeEntityMetadata puffed = entity::runtimeEntityMetadata(fish);
    const std::vector<SetEntityDataPacket::Entry> news = puffed.changesFrom(&deflated);
    const auto found =
        std::ranges::find_if(news, [](const SetEntityDataPacket::Entry& entry) { return entry.index == 17; });
    REQUIRE(found != news.end());
    CHECK(std::get<SetEntityDataPacket::IntValue>(found->value).value == 2);

    // parity: `Pufferfish.PUFF_STATE` on a saved fish rides the same index as the salmon's type.
    basalt::wire::nbt::NbtCompound saved;
    saved.putInt("PuffState", 1);
    const std::vector<SetEntityDataPacket::Entry> entries = entity::generatedEntityMetadata("pufferfish", saved);
    REQUIRE(!entries.empty());
    CHECK(entries.front().index == 17);
    CHECK(std::get<SetEntityDataPacket::IntValue>(entries.front().value).value == 1);
}
