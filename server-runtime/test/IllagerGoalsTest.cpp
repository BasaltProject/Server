#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/ai/IllagerGoals.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <doctest.h>

#include <array>
#include <memory>
#include <string>
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

    void shootFireball(ServerEntity&, double, double, double, bool) override {}

    void setBeamTarget(ServerEntity&, std::optional<std::int32_t>) override {}

    void setCasting(ServerEntity&, bool value) override { casting = value; }

    void throwPotion(ServerEntity&, double, double, double, std::string_view name) override {
        potions.emplace_back(name);
    }

    void castFangs(ServerEntity&, double, double, double) override { fangs++; }

    void summonVexes(ServerEntity&, std::int32_t count) override { vexes += count; }

    void applyEffect(std::int32_t, std::string_view, std::int32_t, std::int32_t) override {}

    void setSleeping(ServerEntity&, bool) override {}

    void teleported(ServerEntity&) override {}

    void shootArrow(ServerEntity&, double, double, double, float) override { arrows++; }

    std::int32_t swings = 0;
    std::int32_t arrows = 0;
    std::int32_t fangs = 0;
    std::int32_t vexes = 0;
    bool charging = false;
    bool casting = false;
    std::vector<std::string> potions;
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

TEST_CASE("a pillager winds its crossbow for a second and a quarter before it shoots") {
    Fixture fixture;
    ServerEntity& pillager = fixture.add("pillager", 8.0, 8.0);
    ServerEntity& cow = fixture.add("cow", 8.0, 12.0);
    pillager.targetId = cow.entityId();

    ai::MobContext context = fixture.context(pillager);
    ai::CrossbowAttackGoal crossbow{1.0, 8.0};
    REQUIRE(crossbow.canUse(context));

    for (std::int32_t tick = 0; tick < 24; tick++) {
        crossbow.tick(context);
    }
    CHECK(fixture.actions.arrows == 0);
    CHECK(fixture.actions.charging);

    crossbow.tick(context);
    CHECK(fixture.actions.arrows == 1);
    CHECK_FALSE(fixture.actions.charging);

    // parity: a rest of twenty to forty ticks, so twenty more never gets a second bolt away.
    for (std::int32_t tick = 0; tick < 20; tick++) {
        crossbow.tick(context);
    }
    CHECK(fixture.actions.arrows == 1);

    crossbow.stop(context);
    CHECK_FALSE(fixture.actions.charging);
}

TEST_CASE("a witch picks its potion by how far away you are") {
    Fixture fixture;
    ServerEntity& witch = fixture.add("witch", 8.0, 8.0);
    ServerEntity& cow = fixture.add("cow", 8.0, 17.0);
    witch.targetId = cow.entityId();

    ai::MobContext context = fixture.context(witch);
    ai::ThrowPotionGoal potion;
    REQUIRE(potion.canUse(context));

    // Nine blocks off is slowness.
    potion.tick(context);
    REQUIRE(fixture.actions.potions.size() == 1);
    CHECK(fixture.actions.potions.back() == "slowness");

    // Past ten it walks in rather than throwing.
    cow.z = 30.0;
    for (std::int32_t tick = 0; tick < 61; tick++) {
        potion.tick(context);
    }
    CHECK(fixture.actions.potions.size() == 1);

    // Up close it is never slowness.
    cow.z = 10.0;
    for (std::int32_t tick = 0; tick < 300; tick++) {
        potion.tick(context);
    }
    REQUIRE(fixture.actions.potions.size() > 1);
    for (std::size_t index = 1; index < fixture.actions.potions.size(); index++) {
        CHECK(fixture.actions.potions[index] != "slowness");
    }
}

TEST_CASE("an evoker casts after the wind-up and then waits out its cooldown") {
    Fixture fixture;
    ServerEntity& evoker = fixture.add("evoker", 8.0, 8.0);
    ServerEntity& cow = fixture.add("cow", 8.0, 12.0);
    evoker.targetId = cow.entityId();

    ai::MobContext context = fixture.context(evoker);
    ai::CastSpellGoal fangs{ai::CastSpellGoal::Spell::Fangs, 20, 100};
    REQUIRE(fangs.canUse(context));
    fangs.start(context);
    CHECK(fixture.actions.casting);

    for (std::int32_t tick = 0; tick < 19; tick++) {
        fangs.tick(context);
        CHECK(fangs.canContinueToUse(context));
    }
    CHECK(fixture.actions.fangs == 0);

    fangs.tick(context);
    CHECK(fixture.actions.fangs == 1);
    // The wind-up is spent, so the goal wants to be let go of.
    CHECK_FALSE(fangs.canContinueToUse(context));

    fangs.stop(context);
    CHECK_FALSE(fixture.actions.casting);
    // parity: a hundred ticks before it can start again.
    CHECK_FALSE(fangs.canUse(context));
}

TEST_CASE("an evoker's other spell summons three vexes") {
    Fixture fixture;
    ServerEntity& evoker = fixture.add("evoker", 8.0, 8.0);
    ServerEntity& cow = fixture.add("cow", 8.0, 12.0);
    evoker.targetId = cow.entityId();

    ai::MobContext context = fixture.context(evoker);
    ai::CastSpellGoal summon{ai::CastSpellGoal::Spell::SummonVexes, 2, 10};
    summon.start(context);
    summon.tick(context);
    summon.tick(context);
    CHECK(fixture.actions.vexes == 3);
}

TEST_CASE("a raider walks to its raid and stops once it is inside the village") {
    Fixture fixture;
    ServerEntity& pillager = fixture.add("pillager", 8.0, 8.0);

    ai::MobContext context = fixture.context(pillager);
    ai::PathfindToRaidGoal raid{0.7};
    // No raid to be at.
    CHECK_FALSE(raid.canUse(context));

    pillager.raidCenter = std::array<std::int32_t, 3>{100, 64, 100};
    CHECK(raid.canUse(context));

    // Twelve blocks is close enough to count as being there.
    pillager.raidCenter = std::array<std::int32_t, 3>{12, 64, 8};
    CHECK_FALSE(raid.canUse(context));

    // Something to hit outranks getting there.
    ServerEntity& cow = fixture.add("cow", 8.0, 12.0);
    pillager.raidCenter = std::array<std::int32_t, 3>{100, 64, 100};
    pillager.targetId = cow.entityId();
    CHECK_FALSE(raid.canUse(context));
}
