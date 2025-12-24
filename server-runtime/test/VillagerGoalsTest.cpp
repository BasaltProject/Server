#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/ai/VillagerGoals.hpp>
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

    void setSleeping(ServerEntity&, bool value) override {
        sleeping = value;
        sleepCalls++;
    }

    void teleported(ServerEntity&) override {}

    void shootArrow(ServerEntity&, double, double, double, float) override {}

    std::int32_t swings = 0;
    bool sleeping = false;
    std::int32_t sleepCalls = 0;
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

namespace {

/// The state id of a block by name, which is what the world stores.
std::int32_t stateOf(std::string_view name) {
    const basalt::server::registry::BlockState* block = blocks::named(std::string{name});
    REQUIRE(block != nullptr);
    return block->stateId;
}

} // namespace

TEST_CASE("a villager goes to bed at night and lies down once it is there") {
    Fixture fixture;
    ServerEntity& villager = fixture.add("villager", 8.0, 8.0);
    REQUIRE(fixture.world.setBlockState(9, 64, 8, stateOf("red_bed")));

    ai::MobContext context = fixture.context(villager);
    ai::SleepInBedGoal sleep{0.5};

    // Nothing doing during working hours.
    context.dayTime = 3000;
    CHECK_FALSE(sleep.canUse(context));

    context.dayTime = 13000;
    REQUIRE(sleep.canUse(context));
    sleep.start(context);
    sleep.tick(context);
    CHECK(fixture.actions.sleeping);

    sleep.stop(context);
    CHECK_FALSE(fixture.actions.sleeping);
}

TEST_CASE("a raid sends a villager to the same bed but keeps it on its feet") {
    Fixture fixture;
    ServerEntity& villager = fixture.add("villager", 8.0, 8.0);
    REQUIRE(fixture.world.setBlockState(9, 64, 8, stateOf("red_bed")));

    ai::MobContext context = fixture.context(villager);
    context.dayTime = 3000;
    context.raidNearby = true;

    ai::SleepInBedGoal sleep{0.5};
    REQUIRE(sleep.canUse(context));
    sleep.start(context);
    sleep.tick(context);
    // parity: hiding is standing in the house, not lying in the bed.
    CHECK_FALSE(fixture.actions.sleeping);
    CHECK(fixture.actions.sleepCalls == 1);
}

TEST_CASE("a raid pulls a villager off its workstation and away from the bell") {
    Fixture fixture;
    ServerEntity& villager = fixture.add("villager", 8.0, 8.0);
    REQUIRE(fixture.world.setBlockState(9, 64, 8, stateOf("composter")));
    REQUIRE(fixture.world.setBlockState(11, 64, 8, stateOf("bell")));

    ai::MobContext context = fixture.context(villager);
    ai::WorkAtSiteGoal work{0.5};
    ai::MeetAtBellGoal meet{0.5};

    context.dayTime = 3000;
    CHECK(work.canUse(context));
    CHECK_FALSE(meet.canUse(context));

    context.dayTime = 10000;
    CHECK(meet.canUse(context));

    context.raidNearby = true;
    CHECK_FALSE(meet.canUse(context));
    context.dayTime = 3000;
    CHECK_FALSE(work.canUse(context));
}

TEST_CASE("the walk picks the nearest block of its kind") {
    Fixture fixture;
    REQUIRE(fixture.world.setBlockState(12, 64, 8, stateOf("bell")));
    REQUIRE(fixture.world.setBlockState(10, 64, 8, stateOf("bell")));

    const std::optional<std::array<std::int32_t, 3>> nearest =
        ai::villageBlocks::findNearest(fixture.world, 8.0, 64.0, 8.0, ai::villageBlocks::bells(), 32, 4);
    REQUIRE(nearest.has_value());
    CHECK((*nearest)[0] == 10);
    CHECK((*nearest)[2] == 8);

    // A name it does not know is not a village block, however close it is.
    CHECK_FALSE(ai::villageBlocks::matches(fixture.world, 10, 64, 8, ai::villageBlocks::beds()));
    CHECK(ai::villageBlocks::matches(fixture.world, 10, 64, 8, ai::villageBlocks::bells()));
}
