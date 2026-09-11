#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/BlockChangeBroadcaster.hpp>
#include <basalt/server/runtime/CombatSystem.hpp>
#include <basalt/server/runtime/ExplosionSystem.hpp>
#include <basalt/server/runtime/entity/Projectiles.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/RealLootTables.hpp>
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

/// One entity of that type at that spot, in the fixture's world.
ServerEntity& spawnAt(Fixture& fixture, std::int32_t entityId, std::string_view type, double x, double y, double z) {
    const auto* entry = basalt::server::registry::entityTypes::of(Identifier::vanilla(std::string{type}));
    REQUIRE(entry != nullptr);
    return fixture.world->state().spawnEntity(std::make_unique<ServerEntity>(
        entityId, Uuid{static_cast<std::uint64_t>(entityId), 1}, entry->id, entry->protocolId, x, y, z));
}

struct CombatFixture {
    Fixture base;
    RealLootTables tables{base.jar()};
    BlockChangeBroadcaster broadcaster{base.registry, tables, {}, {.nextSeed = [] { return std::int64_t{3}; }}};
    ExplosionSystem explosions{base.registry, tables, {}, {.nextSeed = [] { return std::int64_t{3}; }}};
    CombatSystem combat{broadcaster,
                        explosions,
                        {},
                        {.nextInt = [](std::int32_t) { return 0; }, .nextSeed = [] { return std::int64_t{3}; }}};

    explicit CombatFixture(const std::string& root) : base(root) {}
};

} // namespace

TEST_CASE("an arrow hurts what it flies through and sticks nothing back on itself") {
    CombatFixture fixture{scratch("combat-arrow")};
    ServerEntity& cow = spawnAt(fixture.base, 40, "cow", 8.0, 64.0, 10.0);
    ServerEntity& arrow = spawnAt(fixture.base, 41, "arrow", 8.0, 64.0, 8.0);
    arrow.ownerId = 1;
    arrow.velocityX = 0.0;
    arrow.velocityY = 0.0;
    arrow.velocityZ = 3.0;

    const std::vector<ServerEntity*> living{&cow};
    fixture.combat.tickProjectile(*fixture.base.world, fixture.base.watchers(), living, arrow,
                                  basalt::server::runtime::entity::projectiles::arrow());

    CHECK(cow.health < 10.0);
    CHECK(arrow.removed);
    // parity: `AbstractArrow.onHitEntity`: whatever it hit remembers who shot it and panics.
    REQUIRE(cow.hurtBy.has_value());
    CHECK(*cow.hurtBy == 1);
    CHECK(cow.panicTicks > 0);
}

TEST_CASE("an arrow that arrives inside the hurt cooldown bounces off") {
    CombatFixture fixture{scratch("combat-bounce")};
    ServerEntity& cow = spawnAt(fixture.base, 40, "cow", 8.0, 64.0, 10.0);
    // Already hurt this half second, and by more than this arrow is worth.
    cow.hurtCooldown = 15;
    cow.lastHurt = 100.0F;
    const double healthBefore = cow.health;

    ServerEntity& arrow = spawnAt(fixture.base, 41, "arrow", 8.0, 64.0, 8.0);
    arrow.velocityZ = 3.0;
    const std::vector<ServerEntity*> living{&cow};
    fixture.combat.tickProjectile(*fixture.base.world, fixture.base.watchers(), living, arrow,
                                  basalt::server::runtime::entity::projectiles::arrow());

    CHECK(cow.health == doctest::Approx(healthBefore));
    CHECK_FALSE(arrow.removed);
    // parity: the arrow keeps a fifth of the speed it had after the tick's own drag.
    CHECK(arrow.velocityZ == doctest::Approx(3.0 * 0.99 * 0.2));
}

TEST_CASE("a splash potion reaches less far than four blocks and no further") {
    CombatFixture fixture{scratch("combat-splash")};
    fixture.base.player->x = 8.0;
    fixture.base.player->y = 64.0;
    fixture.base.player->z = 9.5;

    // The kind is what decides a potion, not the entity type it travels as. It flies half a block
    // above the player's feet because a thrown projectile falls on the tick it is thrown, and one
    // started exactly on the bottom face of the box would dip under it before it got there.
    ServerEntity& bottle = spawnAt(fixture.base, 42, "snowball", 8.0, 64.5, 8.0);
    bottle.potionEffect = "poison";
    bottle.velocityZ = 3.0;

    fixture.combat.tickProjectile(*fixture.base.world, fixture.base.watchers(), {}, bottle,
                                  *basalt::server::runtime::entity::projectiles::kindOf(Identifier::vanilla("potion")));

    REQUIRE(fixture.base.effects.size() == 1);
    CHECK(fixture.base.effects.front().first == "poison");
    // parity: `applySplash`, twenty seconds at the centre and less the further away you stand.
    CHECK(fixture.base.effects.front().second > 0);
    CHECK(fixture.base.effects.front().second < 400);
    CHECK(bottle.removed);
}

TEST_CASE("a killing hit tells everyone watching and starts the death the entity plays out") {
    CombatFixture fixture{scratch("combat-death")};
    ServerEntity& cow = spawnAt(fixture.base, 40, "cow", 8.0, 64.0, 10.0);

    fixture.combat.announceEntityHit(fixture.base.watchers(), cow, Identifier::vanilla("arrow"), 1, true);
    CHECK(cow.killed);
    // parity: `LivingEntity.tickDeath` keeps the body for twenty ticks before it goes.
    CHECK(cow.dying);
    CHECK_FALSE(cow.removed);
    CHECK(fixture.base.sentCount(play::EntityEventPacket::kId) == 1);
    CHECK(fixture.base.sentCount(play::DamageEventPacket::kId) == 1);
}
