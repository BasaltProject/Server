#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/ai/TameGoals.hpp>
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

TEST_CASE("a sitting pet stays put and lets nothing else move it") {
    Fixture fixture;
    ServerEntity& wolf = fixture.add("wolf", 8.0, 8.0);
    wolf.owner = "owner-uuid";
    fixture.players.push_back(ai::PlayerView{.entityId = 100, .uuid = "owner-uuid", .x = 8.0, .y = 64.0, .z = 40.0});

    ai::MobContext context = fixture.context(wolf);
    ai::SitWhenOrderedToGoal sit;
    ai::FollowOwnerGoal follow{1.0};

    // Thirty-two blocks away is far enough to follow, but not while it is sat.
    CHECK(follow.canUse(context));
    CHECK_FALSE(sit.canUse(context));

    wolf.sitting = true;
    CHECK(sit.canUse(context));
    CHECK_FALSE(follow.canUse(context));
    CHECK_FALSE(sit.isInterruptable());
}

TEST_CASE("a pet follows its own owner and no one else") {
    Fixture fixture;
    ServerEntity& wolf = fixture.add("wolf", 8.0, 8.0);
    wolf.owner = "owner-uuid";
    fixture.players.push_back(ai::PlayerView{.entityId = 100, .uuid = "stranger", .x = 8.0, .y = 64.0, .z = 40.0});

    ai::MobContext context = fixture.context(wolf);
    ai::FollowOwnerGoal follow{1.0};
    CHECK_FALSE(follow.canUse(context));

    fixture.players.back().uuid = "owner-uuid";
    CHECK(follow.canUse(context));

    // Two blocks is the distance it settles at: it starts at ten and gives up at two.
    fixture.players.back().z = 9.5;
    CHECK_FALSE(follow.canUse(context));
    CHECK_FALSE(follow.canContinueToUse(context));
}

TEST_CASE("a pet takes over the owner's fight in both directions") {
    Fixture fixture;
    ServerEntity& wolf = fixture.add("wolf", 8.0, 8.0);
    ServerEntity& skeleton = fixture.add("skeleton", 9.0, 9.0);
    wolf.owner = "owner-uuid";

    ai::MobContext context = fixture.context(wolf);
    ai::OwnerHurtByTargetGoal hurtBy;
    ai::OwnerHurtTargetGoal hurt;
    CHECK_FALSE(hurtBy.canUse(context));
    CHECK_FALSE(hurt.canUse(context));

    wolf.ownerHurtBy = skeleton.entityId();
    CHECK(hurtBy.canUse(context));
    hurtBy.start(context);
    CHECK(wolf.targetId == skeleton.entityId());
    // Consumed: the same attacker does not re-arm the goal on the next pass.
    CHECK_FALSE(wolf.ownerHurtBy.has_value());
    hurtBy.stop(context);
    CHECK_FALSE(wolf.targetId.has_value());

    wolf.ownerHurt = skeleton.entityId();
    CHECK(hurt.canUse(context));
    hurt.start(context);
    CHECK(wolf.targetId == skeleton.entityId());
    CHECK_FALSE(wolf.ownerHurt.has_value());

    // A sat pet stays out of it.
    wolf.sitting = true;
    wolf.ownerHurt = skeleton.entityId();
    CHECK_FALSE(hurt.canUse(context));
}

TEST_CASE("a wolf begs at whoever is holding a bone and stops when they put it away") {
    Fixture fixture;
    ServerEntity& wolf = fixture.add("wolf", 8.0, 8.0);
    fixture.players.push_back(ai::PlayerView{
        .entityId = 100, .uuid = "holder", .x = 8.0, .y = 64.0, .z = 10.0, .heldItem = Identifier::vanilla("bone")});

    ai::MobContext context = fixture.context(wolf);
    ai::BegGoal beg{8.0, {"bone"}};
    CHECK(beg.canUse(context));
    beg.start(context);
    CHECK(wolf.begging);
    CHECK(beg.canContinueToUse(context));

    fixture.players.back().heldItem = Identifier::vanilla("stick");
    CHECK_FALSE(beg.canContinueToUse(context));
    beg.stop(context);
    CHECK_FALSE(wolf.begging);
    CHECK_FALSE(beg.canUse(context));
}
