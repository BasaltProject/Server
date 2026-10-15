#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/ai/GoalSelector.hpp>
#include <basalt/server/runtime/entity/ai/MobContext.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <doctest.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

using basalt::server::random::RandomSource;
using basalt::server::runtime::entity::ServerEntity;
using basalt::server::runtime::entity::ai::Goal;
using basalt::server::runtime::entity::ai::GoalFlag;
using basalt::server::runtime::entity::ai::GoalSelector;
using basalt::server::runtime::entity::ai::MobActions;
using basalt::server::runtime::entity::ai::MobContext;
using basalt::server::runtime::entity::ai::PlayerView;
using basalt::wire::common::Identifier;
using basalt::wire::common::Uuid;

namespace {

class Recording final : public Goal {
public:
    Recording(std::string name, std::set<GoalFlag> flags, bool interruptable = true)
        : name_(std::move(name)), flags_(std::move(flags)), interruptable_(interruptable) {}

    [[nodiscard]] std::set<GoalFlag> flags() const override { return flags_; }

    [[nodiscard]] bool canUse(MobContext&) override { return usable; }

    [[nodiscard]] bool isInterruptable() const override { return interruptable_; }

    [[nodiscard]] bool requiresUpdateEveryTick() const override { return everyTick; }

    void start(MobContext&) override { starts++; }

    void stop(MobContext&) override { stops++; }

    void tick(MobContext&) override { ticks++; }

    [[nodiscard]] const std::string& name() const { return name_; }

    bool usable = true;
    bool everyTick = false;
    std::int32_t starts = 0;
    std::int32_t stops = 0;
    std::int32_t ticks = 0;

private:
    std::string name_;
    std::set<GoalFlag> flags_;
    bool interruptable_;
};

class NoActions final : public MobActions {
public:
    void swing(ServerEntity&) override {}

    void hurtPlayer(ServerEntity&, std::int32_t, float) override {}

    void hurtMob(ServerEntity&, ServerEntity&, float) override {}

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

    void wardenRoar(ServerEntity&, std::int32_t) override {}
};

struct Fixture {
    basalt::server::runtime::world::WorldState world;
    std::unique_ptr<ServerEntity> mob;
    RandomSource random = RandomSource::legacy(1);
    std::vector<ServerEntity*> entities;
    std::vector<PlayerView> players;
    NoActions actions;

    Fixture() {
        const basalt::server::registry::EntityTypeEntry* type =
            basalt::server::registry::entityTypes::of(Identifier::vanilla("zombie"));
        REQUIRE(type != nullptr);
        mob = std::make_unique<ServerEntity>(1, Uuid{1, 1}, type->id, type->protocolId, 0.0, 64.0, 0.0);
        entities.push_back(mob.get());
    }

    [[nodiscard]] MobContext context() {
        return MobContext{.world = world,
                          .mob = *mob,
                          .random = random,
                          .entities = entities,
                          .players = players,
                          .actions = actions,
                          .speed = 0.23};
    }
};

} // namespace

TEST_CASE("the lower priority number wins a flag, and the loser is stopped") {
    Fixture fixture;
    GoalSelector selector;
    auto high = std::make_unique<Recording>("high", std::set{GoalFlag::Move});
    auto low = std::make_unique<Recording>("low", std::set{GoalFlag::Move});
    Recording& highRef = *high;
    Recording& lowRef = *low;
    selector.add(5, std::move(low));
    selector.add(1, std::move(high));

    const auto running = [&selector](const Recording& goal) {
        return selector.isRunning([&goal](const Goal& candidate) { return &candidate == &goal; });
    };

    MobContext context = fixture.context();
    selector.tick(context);
    // parity: Mob goal pass replaces in registration order, so the worse goal may start and stop in one tick.
    CHECK(running(highRef));
    CHECK_FALSE(running(lowRef));
    CHECK(lowRef.stops == 1);

    highRef.usable = false;
    selector.tick(context);
    CHECK(highRef.stops == 1);
    CHECK(running(lowRef));
}

TEST_CASE("two goals wanting different flags run together") {
    Fixture fixture;
    GoalSelector selector;
    auto move = std::make_unique<Recording>("move", std::set{GoalFlag::Move});
    auto look = std::make_unique<Recording>("look", std::set{GoalFlag::Look});
    Recording& moveRef = *move;
    Recording& lookRef = *look;
    selector.add(1, std::move(move));
    selector.add(2, std::move(look));

    MobContext context = fixture.context();
    selector.tick(context);
    CHECK(moveRef.starts == 1);
    CHECK(lookRef.starts == 1);
    CHECK(moveRef.ticks == 1);
    CHECK(lookRef.ticks == 1);
}

TEST_CASE("a goal that cannot be interrupted keeps its flag whatever outranks it") {
    Fixture fixture;
    GoalSelector selector;
    auto stubborn = std::make_unique<Recording>("panic", std::set{GoalFlag::Move}, false);
    auto better = std::make_unique<Recording>("better", std::set{GoalFlag::Move});
    Recording& stubbornRef = *stubborn;
    Recording& betterRef = *better;
    selector.add(5, std::move(stubborn));
    selector.add(1, std::move(better));

    MobContext context = fixture.context();
    selector.tick(context);
    selector.tick(context);
    CHECK(stubbornRef.starts == 1);
    CHECK(betterRef.starts == 0);
}

TEST_CASE("between decisions only the goals that ask for every tick are ticked") {
    Fixture fixture;
    GoalSelector selector;
    auto lazy = std::make_unique<Recording>("lazy", std::set{GoalFlag::Move});
    auto eager = std::make_unique<Recording>("eager", std::set{GoalFlag::Look});
    Recording& lazyRef = *lazy;
    Recording& eagerRef = *eager;
    eager->everyTick = true;
    selector.add(1, std::move(lazy));
    selector.add(2, std::move(eager));

    MobContext context = fixture.context();
    selector.tick(context);
    selector.tickRunning(context, false);
    selector.tickRunning(context, false);
    // parity: `Mob` alternates full decisions, so half the mobs only run their eager goals.
    CHECK(lazyRef.ticks == 1);
    CHECK(eagerRef.ticks == 3);
}
