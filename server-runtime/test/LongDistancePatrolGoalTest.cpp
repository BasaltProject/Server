#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/ai/LongDistancePatrolGoal.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <doctest.h>

#include <array>
#include <cstdint>
#include <cstdlib>
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

    void setCharging(ServerEntity&, bool) override {}

    void shootFireball(ServerEntity&, double, double, double, bool) override {}

    void setBeamTarget(ServerEntity&, std::optional<std::int32_t>) override {}

    void setCasting(ServerEntity&, bool) override {}

    void throwPotion(ServerEntity&, double, double, double, std::string_view) override {}

    void castFangs(ServerEntity&, double, double, double) override {}

    void summonVexes(ServerEntity&, std::int32_t) override {}

    void applyEffect(std::int32_t, std::string_view, std::int32_t, std::int32_t, std::optional<std::int32_t>) override {
    }

    void setSleeping(ServerEntity&, bool) override {}

    void teleported(ServerEntity&) override {}

    void shootArrow(ServerEntity&, double, double, double, float) override {}

    std::int32_t swings = 0;
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

TEST_CASE("a patrol only runs while there is a patrol to run") {
    Fixture fixture;
    ServerEntity& leader = fixture.add("pillager", 8.0, 8.0);

    ai::MobContext context = fixture.context(leader);
    ai::LongDistancePatrolGoal patrol;
    CHECK_FALSE(patrol.canUse(context));

    leader.patrolling = true;
    CHECK_FALSE(patrol.canUse(context));

    leader.patrolTarget = std::array<std::int32_t, 3>{500, 64, 500};
    CHECK(patrol.canUse(context));

    // A raider carrying somebody, or with somebody to hit, is not patrolling.
    leader.passengerIds.push_back(99);
    CHECK_FALSE(patrol.canUse(context));
    leader.passengerIds.clear();

    // parity: a failed path buys two hundred ticks of quiet.
    leader.patrolCooldownUntil = 100;
    CHECK_FALSE(patrol.canUse(context));
    context.gameTime = 100;
    CHECK(patrol.canUse(context));
}

TEST_CASE("a raider with nobody left to patrol with gives up patrolling") {
    Fixture fixture;
    ServerEntity& lone = fixture.add("pillager", 8.0, 8.0);
    lone.patrolling = true;
    lone.patrolTarget = std::array<std::int32_t, 3>{500, 64, 500};

    ai::MobContext context = fixture.context(lone);
    ai::LongDistancePatrolGoal patrol;
    patrol.tick(context);
    CHECK_FALSE(lone.patrolling);
}

TEST_CASE("the leader picks a new point once the patrol reaches the old one") {
    Fixture fixture;
    ServerEntity& leader = fixture.add("pillager", 8.0, 8.0);
    ServerEntity& follower = fixture.add("vindicator", 10.0, 8.0);
    leader.patrolling = true;
    leader.patrolLeader = true;
    leader.patrolTarget = std::array<std::int32_t, 3>{8, 64, 12};
    follower.patrolling = true;

    ai::MobContext context = fixture.context(leader);
    ai::LongDistancePatrolGoal patrol;
    patrol.tick(context);

    REQUIRE(leader.patrolTarget.has_value());
    // parity: five hundred blocks either way from where it stands.
    const std::array<std::int32_t, 3>& next = *leader.patrolTarget;
    CHECK(next != std::array<std::int32_t, 3>{8, 64, 12});
    CHECK(std::abs(next[0] - 8) <= 500);
    CHECK(std::abs(next[2] - 8) <= 500);
    CHECK(leader.patrolling);
}
