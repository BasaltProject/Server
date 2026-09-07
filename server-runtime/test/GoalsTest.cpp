#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/entity/Breeding.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/ai/Goals.hpp>
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

class NoActions final : public ai::MobActions {
public:
    void swing(ServerEntity&) override {}

    void hurtPlayer(ServerEntity&, std::int32_t, float) override {}

    void hurtMob(ServerEntity&, ServerEntity&, float) override {}

    void breed(ServerEntity&, ServerEntity&) override {}

    void eatGrass(ServerEntity&) override { grassEaten++; }

    void primeCreeper(ServerEntity&) override { primed++; }

    void explodeCreeper(ServerEntity&) override { exploded++; }

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

    void shootArrow(ServerEntity&, double, double, double, float) override { arrows++; }

    std::int32_t grassEaten = 0;
    std::int32_t primed = 0;
    std::int32_t exploded = 0;
    std::int32_t arrows = 0;
};

struct Fixture {
    WorldState world;
    std::vector<std::unique_ptr<ServerEntity>> owned;
    std::vector<ServerEntity*> entities;
    std::vector<ai::PlayerView> players;
    RandomSource random = RandomSource::legacy(7);
    NoActions actions;

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

    [[nodiscard]] ai::MobContext context(ServerEntity& mob) {
        return ai::MobContext{.world = world,
                              .mob = mob,
                              .random = random,
                              .entities = entities,
                              .players = players,
                              .actions = actions,
                              .speed = 0.25};
    }
};

} // namespace

TEST_CASE("a mob standing in water floats, and one on dry land does not") {
    Fixture fixture;
    ServerEntity& cow = fixture.add("cow", 8.5, 8.5);
    ai::MobContext context = fixture.context(cow);
    ai::FloatGoal goal;
    CHECK_FALSE(goal.canUse(context));

    REQUIRE(fixture.world.setBlockState(8, 64, 8, blocks::named("water")->stateId));
    CHECK(goal.canUse(context));
    REQUIRE(fixture.world.setBlockState(8, 64, 8, blocks::named("lava")->stateId));
    // parity: `FloatGoal` bobs in lava too, which is why a zombie in it does not simply sink.
    CHECK(goal.canUse(context));
}

TEST_CASE("tempting follows only a player holding the right thing") {
    Fixture fixture;
    ServerEntity& cow = fixture.add("cow", 8.5, 8.5);
    fixture.players.push_back(ai::PlayerView{
        .entityId = 100, .x = 10.5, .y = 64.0, .z = 8.5, .targetable = true, .heldItem = Identifier::vanilla("stone")});
    ai::MobContext context = fixture.context(cow);
    ai::TemptGoal goal{1.0, {"wheat"}};
    CHECK_FALSE(goal.canUse(context));

    fixture.players[0].heldItem = Identifier::vanilla("wheat");
    CHECK(goal.canUse(context));

    // A player in creative or spectator is no temptation at all.
    fixture.players[0].targetable = false;
    CHECK_FALSE(goal.canUse(context));

    // Nor is one holding it twenty blocks away.
    fixture.players[0].targetable = true;
    fixture.players[0].x = 40.0;
    CHECK_FALSE(goal.canUse(context));
}

TEST_CASE("a calf follows a grown one of its own kind, and never another species") {
    Fixture fixture;
    ServerEntity& calf = fixture.add("cow", 8.5, 8.5);
    calf.age = basalt::server::runtime::entity::breeding::kBabyAge;
    ServerEntity& pig = fixture.add("pig", 9.5, 8.5);
    static_cast<void>(pig);
    ai::MobContext context = fixture.context(calf);
    ai::FollowParentGoal goal{1.0};
    CHECK_FALSE(goal.canUse(context));

    ServerEntity& parent = fixture.add("cow", 12.5, 8.5);
    static_cast<void>(parent);
    CHECK(goal.canUse(context));

    // A grown cow follows nobody.
    calf.age = 0;
    CHECK_FALSE(goal.canUse(context));
}

TEST_CASE("a creeper's fuse burns down when it loses you and goes off when it does not") {
    Fixture fixture;
    ServerEntity& creeper = fixture.add("creeper", 8.5, 8.5);
    fixture.players.push_back(ai::PlayerView{.entityId = 100, .x = 9.0, .y = 64.0, .z = 8.5, .targetable = true});
    creeper.targetId = 100;
    ai::MobContext context = fixture.context(creeper);
    ai::SwellGoal goal;
    REQUIRE(goal.canUse(context));
    goal.start(context);

    for (std::int32_t tick = 0; tick < 30; tick++) {
        goal.tick(context);
    }
    CHECK(fixture.actions.primed == 1);
    CHECK(fixture.actions.exploded >= 1);

    // Walk away and the fuse burns back down rather than staying wherever it was.
    ServerEntity& second = fixture.add("creeper", 8.5, 8.5);
    second.targetId = 100;
    second.swell = 10;
    fixture.players[0].x = 100.0;
    ai::MobContext other = fixture.context(second);
    goal.tick(other);
    CHECK(second.swell == 9);
}

TEST_CASE("a bow mob shoots on its own clock and closes when it is too far") {
    Fixture fixture;
    ServerEntity& skeleton = fixture.add("skeleton", 8.5, 8.5);
    fixture.players.push_back(
        ai::PlayerView{.entityId = 100, .x = 12.5, .y = 64.0, .z = 8.5, .height = 1.8, .targetable = true});
    skeleton.targetId = 100;
    ai::MobContext context = fixture.context(skeleton);
    ai::RangedAttackGoal goal{1.0, 20, 40, 15.0};
    REQUIRE(goal.canUse(context));

    for (std::int32_t tick = 0; tick < 60; tick++) {
        goal.tick(context);
    }
    CHECK(fixture.actions.arrows >= 1);
}
