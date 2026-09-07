#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/entity/EntityCombat.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/ai/FlyingGoals.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <doctest.h>

#include <cstdint>
#include <memory>
#include <vector>

using basalt::server::random::RandomSource;
using basalt::server::runtime::entity::ServerEntity;
using basalt::server::runtime::world::WorldState;
using basalt::wire::common::Identifier;
using basalt::wire::common::Uuid;
namespace ai = basalt::server::runtime::entity::ai;
namespace blocks = basalt::server::registry::blocks;

namespace {

class Recording final : public ai::MobActions {
public:
    void swing(ServerEntity&) override { swings++; }

    void hurtPlayer(ServerEntity&, std::int32_t, float damage) override { dealt += damage; }

    void hurtMob(ServerEntity&, ServerEntity&, float damage) override { dealt += damage; }

    void breed(ServerEntity&, ServerEntity&) override {}

    void eatGrass(ServerEntity&) override {}

    void primeCreeper(ServerEntity&) override {}

    void explodeCreeper(ServerEntity&) override {}

    void setCharging(ServerEntity&, bool value) override { charging = value; }

    void shootFireball(ServerEntity&, double, double, double, bool large) override {
        fireballs++;
        lastLarge = large;
    }

    void setBeamTarget(ServerEntity&, std::optional<std::int32_t> target) override { beam = target; }

    void setCasting(ServerEntity&, bool) override {}

    void throwPotion(ServerEntity&, double, double, double, std::string_view) override {}

    void castFangs(ServerEntity&, double, double, double) override {}

    void summonVexes(ServerEntity&, std::int32_t) override {}

    void applyEffect(std::int32_t, std::string_view, std::int32_t, std::int32_t) override {}

    void setSleeping(ServerEntity&, bool) override {}

    void teleported(ServerEntity&) override {}

    void shootArrow(ServerEntity&, double, double, double, float) override {}

    std::int32_t swings = 0;
    std::int32_t fireballs = 0;
    bool lastLarge = false;
    bool charging = false;
    std::optional<std::int32_t> beam;
    float dealt = 0.0F;
};

struct Fixture {
    WorldState world;
    std::vector<std::unique_ptr<ServerEntity>> owned;
    std::vector<ServerEntity*> entities;
    std::vector<ai::PlayerView> players;
    RandomSource random = RandomSource::legacy(3);
    Recording actions;

    Fixture() {
        world.getOrGenerateChunk(0, 0);
        const std::int32_t stone = blocks::named("stone")->stateId;
        for (std::int32_t x = 0; x < 16; x++) {
            for (std::int32_t z = 0; z < 16; z++) {
                REQUIRE(world.setBlockState(x, 63, z, stone));
            }
        }
    }

    ServerEntity& add(std::string_view type, double x, double z) {
        const basalt::server::registry::EntityTypeEntry* entry =
            basalt::server::registry::entityTypes::of(Identifier::vanilla(std::string{type}));
        REQUIRE(entry != nullptr);
        owned.push_back(std::make_unique<ServerEntity>(static_cast<std::int32_t>(owned.size() + 1),
                                                       Uuid{owned.size() + 1, 1}, entry->id, entry->protocolId, x, 64.0,
                                                       z));
        entities.push_back(owned.back().get());
        return *owned.back();
    }

    [[nodiscard]] ai::MobContext context(ServerEntity& mob, std::int32_t light = 0) {
        return ai::MobContext{.world = world,
                              .mob = mob,
                              .random = random,
                              .entities = entities,
                              .players = players,
                              .actions = actions,
                              .light = light,
                              .speed = 0.23};
    }
};

} // namespace

TEST_CASE("a ghast charges for a second before it throws anything") {
    Fixture fixture;
    ServerEntity& ghast = fixture.add("ghast", 8.0, 8.0);
    ServerEntity& cow = fixture.add("cow", 8.0, 12.0);
    ghast.targetId = cow.entityId();

    ai::MobContext context = fixture.context(ghast);
    ai::FireballAttackGoal fireball;
    REQUIRE(fireball.canUse(context));

    for (std::int32_t tick = 0; tick < 19; tick++) {
        fireball.tick(context);
    }
    CHECK(fixture.actions.fireballs == 0);
    // parity: the tell shows well before the shot does.
    CHECK(fixture.actions.charging);

    fireball.tick(context);
    CHECK(fixture.actions.fireballs == 1);
    CHECK(fixture.actions.lastLarge);
    CHECK_FALSE(fixture.actions.charging);

    // parity: forty ticks of rest, then another twenty of charge.
    for (std::int32_t tick = 0; tick < 59; tick++) {
        fireball.tick(context);
    }
    CHECK(fixture.actions.fireballs == 1);
    fireball.tick(context);
    CHECK(fixture.actions.fireballs == 2);
}

TEST_CASE("a ghast out of range winds its charge back down") {
    Fixture fixture;
    ServerEntity& ghast = fixture.add("ghast", 8.0, 8.0);
    ServerEntity& cow = fixture.add("cow", 8.0, 12.0);
    ghast.targetId = cow.entityId();

    ai::MobContext context = fixture.context(ghast);
    ai::FireballAttackGoal fireball;
    for (std::int32_t tick = 0; tick < 15; tick++) {
        fireball.tick(context);
    }
    CHECK(fixture.actions.charging);

    // Past sixty-four blocks it gives up rather than holding the charge.
    cow.z = 200.0;
    for (std::int32_t tick = 0; tick < 15; tick++) {
        fireball.tick(context);
    }
    CHECK(fixture.actions.fireballs == 0);
    CHECK_FALSE(fixture.actions.charging);
}

TEST_CASE("a blaze throws three small fireballs and then rests") {
    Fixture fixture;
    ServerEntity& blaze = fixture.add("blaze", 8.0, 8.0);
    ServerEntity& cow = fixture.add("cow", 8.0, 14.0);
    blaze.targetId = cow.entityId();

    ai::MobContext context = fixture.context(blaze);
    ai::BlazeAttackGoal attack;
    REQUIRE(attack.canUse(context));

    // The first step is the tell alone: sixty ticks of it, no shot.
    attack.tick(context);
    CHECK(fixture.actions.charging);
    CHECK(fixture.actions.fireballs == 0);

    for (std::int32_t tick = 0; tick < 400; tick++) {
        attack.tick(context);
    }
    CHECK(fixture.actions.fireballs > 0);
    CHECK_FALSE(fixture.actions.lastLarge);
}

TEST_CASE("a blaze standing next to you punches instead of shooting") {
    Fixture fixture;
    ServerEntity& blaze = fixture.add("blaze", 8.0, 8.0);
    ServerEntity& cow = fixture.add("cow", 8.0, 9.0);
    blaze.targetId = cow.entityId();

    ai::MobContext context = fixture.context(blaze);
    ai::BlazeAttackGoal attack;
    attack.tick(context);
    CHECK(fixture.actions.swings == 1);
    CHECK(fixture.actions.fireballs == 0);
    CHECK(fixture.actions.dealt > 0.0F);

    // parity: one punch a second, no faster.
    for (std::int32_t tick = 0; tick < 19; tick++) {
        attack.tick(context);
    }
    CHECK(fixture.actions.swings == 1);
    attack.tick(context);
    CHECK(fixture.actions.swings == 2);
}

TEST_CASE("a guardian's beam takes eighty ticks and then lands") {
    Fixture fixture;
    ServerEntity& guardian = fixture.add("guardian", 8.0, 8.0);
    ServerEntity& cow = fixture.add("cow", 8.0, 12.0);
    guardian.targetId = cow.entityId();

    ai::MobContext context = fixture.context(guardian);
    ai::BeamAttackGoal beam;
    REQUIRE(beam.canUse(context));
    beam.start(context);
    CHECK(fixture.actions.beam == cow.entityId());

    for (std::int32_t tick = 0; tick < 159; tick++) {
        beam.tick(context);
    }
    CHECK(fixture.actions.dealt == doctest::Approx(0.0));

    beam.tick(context);
    // parity: the beam adds two points on top of the guardian's own attack.
    CHECK(fixture.actions.dealt ==
          doctest::Approx(basalt::server::runtime::entity::entityCombat::attackDamageOf(guardian) + 2.0F));
    // It lets go of what it hit rather than beaming it again straight away.
    CHECK_FALSE(guardian.targetId.has_value());

    beam.stop(context);
    CHECK_FALSE(fixture.actions.beam.has_value());
}

TEST_CASE("a phantom climbs above what it is after and dives through it") {
    Fixture fixture;
    ServerEntity& phantom = fixture.add("phantom", 8.0, 8.0);
    ServerEntity& cow = fixture.add("cow", 8.0, 8.0);
    phantom.y = 78.0;
    phantom.targetId = cow.entityId();

    ai::MobContext context = fixture.context(phantom);
    ai::SwoopAttackGoal swoop;
    REQUIRE(swoop.canUse(context));

    // Fourteen blocks up is past the circling height, so it commits to the dive.
    const double before = phantom.y;
    swoop.tick(context);
    CHECK(phantom.velocityY < 0.0);
    CHECK(phantom.y <= before);
}
