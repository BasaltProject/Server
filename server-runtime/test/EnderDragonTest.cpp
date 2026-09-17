#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/entity/DragonNavigation.hpp>
#include <basalt/server/runtime/entity/EnderDragon.hpp>
#include <basalt/server/runtime/entity/EntityCombat.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/world/DragonFight.hpp>

#include <doctest.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <memory>

using basalt::server::random::RandomSource;
using basalt::server::runtime::entity::ServerEntity;
using basalt::wire::common::Identifier;
using basalt::wire::common::Uuid;
namespace dragons = basalt::server::runtime::entity::enderDragon;

namespace {

/// Where the ground of the test End stands, the podium's top included.
constexpr std::int32_t kGround = 64;

struct Flight {
    ServerEntity dragon{1, Uuid{1, 1}, Identifier::vanilla("ender_dragon"), 0, 0.0, 128.0, 0.0};
    dragons::DragonState state = dragons::freshState(dragon);
    dragons::DragonWorld world;

    explicit Flight(std::int64_t seed) {
        state.random = RandomSource::legacy(seed);
        world.random = &state.random;
        world.heightNoLeaves = [](std::int32_t, std::int32_t) { return kGround; };
        world.heightMotionBlocking = [](std::int32_t, std::int32_t) { return kGround; };
        world.nearestPlayer = [](double, double, double, const dragons::TargetQuery&) {
            return std::optional<dragons::Target>{};
        };
        world.playerById = [](std::int32_t) { return std::optional<dragons::Target>{}; };
        world.canSee = [](const dragons::Target&) { return true; };
        world.isEmptyBlock = [](std::int32_t, std::int32_t y, std::int32_t) { return y >= kGround; };
        world.aliveCrystals = 0;
        dragons::buildNodes(state, world.heightNoLeaves);
        dragons::setPhase(state, dragons::Phase::HoldingPattern, world);
    }

    void tick() {
        dragons::steer(dragon, state, world);
        dragons::placeBodyAndWings(dragon, state);
        dragons::placeRest(dragon, state);
    }
};

} // namespace

TEST_CASE("a dragon with no crystals left circles, comes down on the podium and sits there") {
    Flight flight{42};
    bool landed = false;
    for (std::int32_t tick = 0; tick < 20 * 60 * 10 && !landed; ++tick) {
        flight.tick();
        landed = flight.state.phase == dragons::Phase::SittingScanning;
    }
    REQUIRE(landed);
    // parity: `DragonLandingPhase` hands over within a block of the podium's top centre.
    CHECK(std::abs(flight.dragon.x - 0.5) < 1.0);
    CHECK(std::abs(flight.dragon.y - kGround) < 1.0);
    CHECK(std::abs(flight.dragon.z - 0.5) < 1.0);
    // The head is laid out ahead of the body, not left at the origin.
    const dragons::Vec3& head = flight.state.parts[static_cast<std::size_t>(dragons::Part::Head)];
    const double reach = std::hypot(head.x - flight.dragon.x, head.z - flight.dragon.z);
    CHECK(reach > 5.0);
    CHECK(reach < 7.0);

    // parity: `DragonSittingScanningPhase`, with nobody around, takes off after a hundred ticks.
    for (std::int32_t tick = 0; tick < 101; ++tick) {
        flight.tick();
    }
    CHECK(flight.state.phase == dragons::Phase::Takeoff);
}

TEST_CASE("the node graph keeps a dragon without crystals off the outer ring") {
    Flight flight{7};
    for (std::int32_t from = 12; from < 24; ++from) {
        for (std::int32_t to = 12; to < 24; ++to) {
            const std::optional<dragons::Path> path = dragons::findPath(flight.state, from, to, std::nullopt, false);
            REQUIRE(path.has_value());
            for (const auto& node : path->nodes) {
                // The outer ring is the twelve at radius sixty.
                CHECK(std::hypot(node[0], node[2]) < 50.0);
            }
        }
    }
}

TEST_CASE("a dragon only minds players and blasts, takes a quarter off its wings and will not die in the air") {
    namespace combat = basalt::server::runtime::entity::entityCombat;
    ServerEntity dragon{1, Uuid{1, 1}, Identifier::vanilla("ender_dragon"), 0, 0.0, 100.0, 0.0};
    dragon.health = dragons::kMaxHealth;
    dragon.dragon = std::make_shared<dragons::DragonState>(dragons::freshState(dragon));
    dragon.dragon->phase = dragons::Phase::HoldingPattern;

    // parity: a skeleton's arrow lands and takes nothing off.
    CHECK(combat::hurt(dragon, 8.0F, std::nullopt, std::nullopt, {}, "projectile").landed);
    CHECK(dragon.health == doctest::Approx(200.0));

    // parity: anywhere but the head, `damage / 4 + min(damage, 1)`.
    dragon.dragon->hitPart = dragons::Part::Wing1;
    CHECK(combat::hurt(dragon, 8.0F, std::nullopt, std::nullopt, {}, "attack", "Steve", true).landed);
    CHECK(dragon.health == doctest::Approx(197.0));

    dragon.hurtCooldown = 0;
    dragon.dragon->hitPart = dragons::Part::Head;
    CHECK(combat::hurt(dragon, 8.0F, std::nullopt, std::nullopt, {}, "attack", "Steve", true).landed);
    CHECK(dragon.health == doctest::Approx(189.0));

    // parity: `handleKillingBlow` keeps a flying dragon on one health and sends it off to die.
    dragon.hurtCooldown = 0;
    dragon.dragon->hitPart = dragons::Part::Head;
    const combat::Result last = combat::hurt(dragon, 1000.0F, std::nullopt, std::nullopt, {}, "attack", "Steve", true);
    CHECK_FALSE(last.died);
    CHECK(dragon.health == doctest::Approx(1.0));
    CHECK(dragon.dragon->phase == dragons::Phase::Dying);
    // And a dying one is past being hurt.
    dragon.hurtCooldown = 0;
    CHECK_FALSE(combat::hurt(dragon, 1000.0F, std::nullopt, std::nullopt, {}, "explosion").landed);
}

TEST_CASE("the dragon fight survives a save under vanilla's keys") {
    namespace world = basalt::server::runtime::world;
    world::DragonFight fight;
    fight.shuffleGateways(12345);
    REQUIRE(fight.gateways.size() == 20);
    world::DragonFight again;
    again.shuffleGateways(12345);
    CHECK(fight.gateways == again.gateways);

    fight.needsStateScanning = false;
    fight.dragonKilled = true;
    fight.previouslyKilled = true;
    fight.respawnStage = world::RespawnStage::SummoningPillars;
    fight.respawnTime = 37;
    fight.dragonUuid = Uuid{0x0123456789ABCDEFULL, 0xFEDCBA9876543210ULL};
    fight.exitPortalLocation = std::array<std::int32_t, 3>{0, 61, 0};
    fight.respawnCrystals = {Uuid{1, 2}, Uuid{3, 4}};
    fight.gateways.pop_back();

    const basalt::wire::nbt::NbtCompound saved = fight.encode();
    CHECK(saved.getString("respawn_stage") == "summoning_pillars");
    CHECK(saved.getIntArray("dragon_uuid").size() == 4);
    const world::DragonFight read = world::DragonFight::decode(saved);
    CHECK_FALSE(read.needsStateScanning);
    CHECK(read.dragonKilled);
    CHECK(read.previouslyKilled);
    CHECK(read.respawnStage == world::RespawnStage::SummoningPillars);
    CHECK(read.respawnTime == 37);
    CHECK(read.dragonUuid == fight.dragonUuid);
    CHECK(read.exitPortalLocation == fight.exitPortalLocation);
    CHECK(read.gateways == fight.gateways);
    CHECK(read.respawnCrystals == fight.respawnCrystals);
    // parity: a fresh fight scans the world first.
    CHECK(world::DragonFight::decode({}).needsStateScanning);
}
