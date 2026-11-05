#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/entity/brain/BrainState.hpp>

#include <doctest.h>

using namespace basalt::server::runtime::entity::brain;

TEST_CASE("memories expire and activities fall back to the default") {
    BrainState brain;
    brain.registerMemory("home");
    brain.registerMemory("walk_target");
    Value flag;
    flag.kind = Value::Kind::Bool;
    flag.flag = true;
    CHECK(brain.set("walk_target", flag, 2));
    CHECK_FALSE(brain.set("unknown", flag));
    CHECK(brain.timeToLive("walk_target") == 2);
    brain.tick(1, 0, false);
    brain.tick(1, 0, false);
    brain.tick(2, 0, false);
    CHECK(brain.has("walk_target"));
    brain.tick(3, 0, false);
    brain.tick(4, 0, false);
    CHECK_FALSE(brain.has("walk_target"));

    brain.defineActivity("work", {{"home", MemoryStatus::Present}}, {});
    brain.defineActivity("idle", {}, {});
    brain.setActiveIfPossible("work");
    CHECK(brain.isActive("idle"));
    brain.set("home", flag);
    brain.setActiveIfPossible("work");
    CHECK(brain.activeNonCore() == "work");
}

TEST_CASE("a native-owned brain keeps its activity and runs behaviors once per tick") {
    BrainState brain;
    brain.defineActivity("fight", {}, {});
    brain.setNativeOwned(true);
    brain.setActive("fight");
    brain.tick(100, 6000, false);
    CHECK(brain.isActive("fight"));
    CHECK(brain.claimRun(100));
    CHECK_FALSE(brain.claimRun(100));
    CHECK(brain.claimRun(101));
}
