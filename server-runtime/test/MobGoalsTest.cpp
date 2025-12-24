#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/ai/GoalSelector.hpp>
#include <basalt/server/runtime/entity/ai/MobContext.hpp>
#include <basalt/server/runtime/entity/ai/MobGoals.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <doctest.h>

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

TEST_CASE("a mob with nothing written for it gets no goals at all") {
    // parity: a boss runs on its own phases rather than goals.
    CHECK(ai::mobGoals::forType(Identifier::vanilla("wither")) == nullptr);
    CHECK(ai::mobGoals::forType(Identifier::vanilla("ender_dragon")) == nullptr);
    CHECK(ai::mobGoals::forType(Identifier::vanilla("dolphin")) == nullptr);
    CHECK(ai::mobGoals::forType(Identifier::vanilla("armor_stand")) == nullptr);
    CHECK(ai::mobGoals::forType(Identifier::vanilla("not_a_mob")) == nullptr);
}

TEST_CASE("every mob this build gives an AI to comes back with goals") {
    const std::vector<std::string> types{"cow",
                                         "pig",
                                         "chicken",
                                         "sheep",
                                         "rabbit",
                                         "goat",
                                         "wolf",
                                         "cat",
                                         "parrot",
                                         "polar_bear",
                                         "iron_golem",
                                         "zombie",
                                         "husk",
                                         "zombified_piglin",
                                         "warden",
                                         "ravager",
                                         "silverfish",
                                         "spider",
                                         "enderman",
                                         "creeper",
                                         "skeleton",
                                         "witch",
                                         "pillager",
                                         "evoker",
                                         "illusioner",
                                         "ghast",
                                         "blaze",
                                         "phantom",
                                         "vex",
                                         "guardian",
                                         "bat",
                                         "villager",
                                         "wandering_trader",
                                         "squid",
                                         "cod",
                                         "slime",
                                         "trader_llama",
                                         "allay",
                                         "endermite",
                                         "vindicator"};
    for (const std::string& type : types) {
        const std::unique_ptr<ai::GoalSelector> selector = ai::mobGoals::forType(Identifier::vanilla(type));
        CAPTURE(type);
        REQUIRE(selector != nullptr);
        CHECK(selector->size() > 0);
    }
}

TEST_CASE("a raider carries the patrol goal every other raider carries") {
    // parity: `PatrollingMonster.registerGoals` runs before each raider adds its own, so a pillager
    // has one more goal than the same list without it.
    const std::unique_ptr<ai::GoalSelector> pillager = ai::mobGoals::forType(Identifier::vanilla("pillager"));
    const std::unique_ptr<ai::GoalSelector> witch = ai::mobGoals::forType(Identifier::vanilla("witch"));
    const std::unique_ptr<ai::GoalSelector> zombie = ai::mobGoals::forType(Identifier::vanilla("zombie"));
    REQUIRE(pillager != nullptr);
    REQUIRE(witch != nullptr);
    REQUIRE(zombie != nullptr);
    // A zombie is not a raider, so it never gets one.
    CHECK(zombie->size() == 7);
    CHECK(pillager->size() == 10);
    CHECK(witch->size() == 9);
}

TEST_CASE("a wolf told to sit holds still under its own goal selector") {
    Fixture fixture;
    ServerEntity& wolf = fixture.add("wolf", 8.0, 8.0);
    wolf.sitting = true;
    wolf.velocityX = 0.4;

    const std::unique_ptr<ai::GoalSelector> selector = ai::mobGoals::forType(wolf.type());
    REQUIRE(selector != nullptr);

    ai::MobContext context = fixture.context(wolf);
    selector->tick(context);
    // The sit goal holds the move flag, so nothing else gets to walk it anywhere.
    CHECK(selector->isRunning([](const ai::Goal& goal) { return !goal.isInterruptable(); }));
}
