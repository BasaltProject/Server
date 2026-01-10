#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/ai/SpecialGoals.hpp>
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

TEST_CASE("an enderman notices a stare only after a few ticks of it") {
    Fixture fixture;
    ServerEntity& enderman = fixture.add("enderman", 8.0, 8.0);
    // Thirty-two blocks north of it, facing straight back down the line.
    fixture.players.push_back(
        ai::PlayerView{.entityId = 100, .uuid = "watcher", .x = 8.0, .y = 64.0, .z = 40.0, .yaw = 180.0F});

    ai::MobContext context = fixture.context(enderman);
    ai::StareTriggerGoal stare;
    CHECK(stare.canUse(context));

    stare.start(context);
    // parity: five ticks, halved because the goal does not ask to run every tick.
    CHECK(stare.canContinueToUse(context));
    CHECK(stare.canContinueToUse(context));
    CHECK(stare.canContinueToUse(context));
    CHECK_FALSE(enderman.targetId.has_value());
    CHECK_FALSE(stare.canContinueToUse(context));
    CHECK(enderman.targetId == 100);
}

TEST_CASE("an enderman ignores a player who is not looking at it") {
    Fixture fixture;
    ServerEntity& enderman = fixture.add("enderman", 8.0, 8.0);
    fixture.players.push_back(
        ai::PlayerView{.entityId = 100, .uuid = "watcher", .x = 8.0, .y = 64.0, .z = 40.0, .yaw = 0.0F});

    ai::MobContext context = fixture.context(enderman);
    ai::StareTriggerGoal stare;
    CHECK_FALSE(stare.canUse(context));

    // Out past sixty-four blocks the stare does not reach either.
    fixture.players.back().yaw = 180.0F;
    fixture.players.back().z = 100.0;
    CHECK_FALSE(stare.canUse(context));

    // And a mob that already has something to chase does not go looking.
    fixture.players.back().z = 40.0;
    enderman.targetId = 7;
    CHECK_FALSE(stare.canUse(context));
}

TEST_CASE("standing in water hurts an enderman and moves it on") {
    Fixture fixture;
    ServerEntity& enderman = fixture.add("enderman", 8.0, 8.0);
    REQUIRE(fixture.world.setBlockState(8, 64, 8, blocks::named("water")->stateId));

    ai::MobContext context = fixture.context(enderman);
    ai::TeleportAwayGoal teleport;
    CHECK(teleport.canUse(context));
    teleport.tick(context);
    CHECK(fixture.actions.dealt == doctest::Approx(1.0));
}

TEST_CASE("an enderman in the dark with a target stays where it is") {
    Fixture fixture;
    ServerEntity& enderman = fixture.add("enderman", 8.0, 8.0);
    enderman.targetId = 7;

    ai::MobContext context = fixture.context(enderman);
    context.night = true;
    ai::TeleportAwayGoal teleport;
    teleport.tick(context);
    CHECK(fixture.actions.dealt == doctest::Approx(0.0));
    CHECK(enderman.x == doctest::Approx(8.0));
    CHECK(enderman.z == doctest::Approx(8.0));
}

TEST_CASE("a spider walking into a wall climbs it instead of falling") {
    Fixture fixture;
    ServerEntity& spider = fixture.add("spider", 8.0, 8.0);
    spider.onGround = true;

    ai::MobContext context = fixture.context(spider);
    ai::ClimbWallsGoal climb;
    CHECK_FALSE(climb.canUse(context));

    spider.horizontallyBlocked = true;
    CHECK(climb.canUse(context));
    climb.tick(context);
    CHECK(spider.velocityY == doctest::Approx(0.2));
    CHECK_FALSE(spider.onGround);
}
