#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/ai/WanderingTraderGoals.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <doctest.h>

#include <array>
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

    void applyEffect(std::int32_t, std::string_view, std::int32_t, std::int32_t) override {}

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

TEST_CASE("a wandering trader walks to its meeting point and forgets it once there") {
    Fixture fixture;
    ServerEntity& trader = fixture.add("wandering_trader", 8.0, 8.0);

    ai::MobContext context = fixture.context(trader);
    ai::WanderingTraderWanderGoal wander;
    // Nowhere to be.
    CHECK_FALSE(wander.canUse(context));

    trader.wanderTarget = std::array<std::int32_t, 3>{8, 64, 30};
    CHECK(wander.canUse(context));

    // Two blocks is close enough to count as arrived.
    trader.wanderTarget = std::array<std::int32_t, 3>{8, 64, 9};
    CHECK_FALSE(wander.canUse(context));

    trader.wanderTarget = std::array<std::int32_t, 3>{8, 64, 30};
    wander.stop(context);
    CHECK_FALSE(trader.wanderTarget.has_value());
    CHECK_FALSE(wander.canUse(context));
}

TEST_CASE("a wandering trader stays inside the home its spawner gave it") {
    Fixture fixture;
    ServerEntity& trader = fixture.add("wandering_trader", 8.0, 8.0);

    ai::MobContext context = fixture.context(trader);
    ai::MoveTowardsTraderHomeGoal home;
    // No home, or a home with no radius, is no restriction at all.
    CHECK_FALSE(home.canUse(context));

    trader.homePosition = std::array<std::int32_t, 3>{8, 64, 8};
    CHECK_FALSE(home.canUse(context));

    trader.homeRadius = 16;
    CHECK_FALSE(home.canUse(context));

    // Fifty blocks out is past it, so it looks for a way back.
    trader.homePosition = std::array<std::int32_t, 3>{8, 64, 58};
    CHECK(home.canUse(context));
}
