#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/entity/EntityCombat.hpp>
#include <basalt/server/runtime/entity/MobCombat.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/ai/TargetGoals.hpp>
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
namespace entityCombat = basalt::server::runtime::entity::entityCombat;
namespace mobCombat = basalt::server::runtime::entity::mobCombat;

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

TEST_CASE("a spider only notices you in the dark, and a zombie whatever the light") {
    Fixture fixture;
    ServerEntity& spider = fixture.add("spider", 8.5, 8.5);
    fixture.players.push_back(ai::PlayerView{.entityId = 100, .x = 9.5, .y = 64.0, .z = 8.5, .targetable = true});

    ai::NearestAttackablePlayerGoal shy{0, true};
    ai::MobContext bright = fixture.context(spider, 15);
    CHECK_FALSE(shy.canUse(bright));
    ai::MobContext dark = fixture.context(spider, 0);
    CHECK(shy.canUse(dark));

    ServerEntity& zombie = fixture.add("zombie", 8.5, 8.5);
    ai::NearestAttackablePlayerGoal always{0};
    ai::MobContext zombieBright = fixture.context(zombie, 15);
    CHECK(always.canUse(zombieBright));
}

TEST_CASE("a mob after somebody does not go looking for somebody else") {
    Fixture fixture;
    ServerEntity& zombie = fixture.add("zombie", 8.5, 8.5);
    fixture.players.push_back(ai::PlayerView{.entityId = 100, .x = 9.5, .y = 64.0, .z = 8.5, .targetable = true});
    ai::MobContext context = fixture.context(zombie);
    ai::NearestAttackablePlayerGoal goal{0};
    REQUIRE(goal.canUse(context));

    goal.start(context);
    CHECK(zombie.targetId == 100);
    CHECK_FALSE(goal.canUse(context));

    // A player who goes creative stops being one to chase.
    fixture.players[0].targetable = false;
    CHECK_FALSE(goal.canContinueToUse(context));
}

TEST_CASE("whoever hit it is what it goes after, once") {
    Fixture fixture;
    ServerEntity& cow = fixture.add("cow", 8.5, 8.5);
    fixture.players.push_back(ai::PlayerView{.entityId = 100, .x = 9.5, .y = 64.0, .z = 8.5, .targetable = true});
    ai::MobContext context = fixture.context(cow);
    ai::HurtByTargetGoal goal;
    CHECK_FALSE(goal.canUse(context));

    cow.hurtBy = 100;
    CHECK(goal.canUse(context));
    goal.start(context);
    CHECK(cow.targetId == 100);
    // The memory of being hit is consumed, so it does not re-target every tick.
    CHECK_FALSE(cow.hurtBy.has_value());
    CHECK_FALSE(goal.canUse(context));
}

TEST_CASE("a melee mob swings once its boxes touch, and only once a second") {
    Fixture fixture;
    ServerEntity& zombie = fixture.add("zombie", 8.5, 8.5);
    fixture.players.push_back(ai::PlayerView{
        .entityId = 100, .x = 8.7, .y = 64.0, .z = 8.5, .width = 0.6, .height = 1.8, .targetable = true});
    zombie.targetId = 100;
    ai::MobContext context = fixture.context(zombie);
    ai::MeleeAttackGoal goal{1.0};
    REQUIRE(goal.canUse(context));

    goal.tick(context);
    CHECK(fixture.actions.swings == 1);
    // parity: twenty ticks between swings, so a second tick right after does nothing.
    goal.tick(context);
    CHECK(fixture.actions.swings == 1);

    for (std::int32_t tick = 0; tick < mobCombat::kAttackInterval; tick++) {
        entityCombat::tick(zombie);
    }
    goal.tick(context);
    CHECK(fixture.actions.swings == 2);
}

TEST_CASE("a hit inside the invulnerability window only lands if it is bigger") {
    Fixture fixture;
    ServerEntity& cow = fixture.add("cow", 8.5, 8.5);
    cow.health = 20.0;

    const entityCombat::Result first = entityCombat::hurt(cow, 4.0F);
    CHECK(first.landed);
    CHECK(cow.health == doctest::Approx(16.0));

    // parity: the same hit again inside the window does nothing at all.
    CHECK_FALSE(entityCombat::hurt(cow, 4.0F).landed);
    CHECK(cow.health == doctest::Approx(16.0));

    // A bigger one lands, and only for the difference.
    const entityCombat::Result bigger = entityCombat::hurt(cow, 6.0F);
    CHECK(bigger.landed);
    CHECK(cow.health == doctest::Approx(14.0));
}

TEST_CASE("a plugin can stop a hit or change what it is worth") {
    Fixture fixture;
    ServerEntity& cow = fixture.add("cow", 8.5, 8.5);
    cow.health = 20.0;

    const entityCombat::Hooks refuse{
        .damaged = [](const ServerEntity&, float, std::string_view, const std::optional<std::string>&) {
            return std::optional<float>{};
        }};
    CHECK_FALSE(entityCombat::hurt(cow, 5.0F, std::nullopt, std::nullopt, refuse).landed);
    CHECK(cow.health == doctest::Approx(20.0));

    const entityCombat::Hooks halve{
        .damaged = [](const ServerEntity&, float amount, std::string_view, const std::optional<std::string>&) {
            return std::optional{amount / 2.0F};
        }};
    CHECK(entityCombat::hurt(cow, 8.0F, std::nullopt, std::nullopt, halve).damage == doctest::Approx(4.0F));
}
