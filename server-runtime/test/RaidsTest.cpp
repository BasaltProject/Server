#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/entity/Raids.hpp>

#include <doctest.h>

using basalt::server::runtime::entity::Raid;
using basalt::server::runtime::entity::RaidManager;
using basalt::server::runtime::entity::RaidStatus;
namespace raidWaves = basalt::server::runtime::entity::raidWaves;

TEST_CASE("how many waves a raid brings is the difficulty and nothing else") {
    CHECK(raidWaves::groupsFor("peaceful") == 0);
    CHECK(raidWaves::groupsFor("easy") == 3);
    CHECK(raidWaves::groupsFor("normal") == 5);
    CHECK(raidWaves::groupsFor("hard") == 7);
    // The difficulty arrives from a config file, so its spelling is not to be trusted.
    CHECK(raidWaves::groupsFor("HARD") == 7);
    CHECK(raidWaves::groupsFor("nonsense") == 5);
}

TEST_CASE("each wave brings its own count of each kind of raider") {
    // parity: `Raid.RaiderType`, whose first wave is four pillagers and nothing else.
    CHECK(raidWaves::countFor("pillager", 1) == 4);
    CHECK(raidWaves::countFor("vindicator", 1) == 0);
    CHECK(raidWaves::countFor("vindicator", 2) == 2);
    CHECK(raidWaves::countFor("evoker", 5) == 1);
    CHECK(raidWaves::countFor("ravager", 3) == 1);
    CHECK(raidWaves::countFor("witch", 4) == 3);
    // Past the last wave the last column keeps being used, and an unknown raider brings none.
    CHECK(raidWaves::countFor("pillager", 99) == raidWaves::countFor("pillager", 7));
    CHECK(raidWaves::countFor("zombie", 1) == 0);
}

TEST_CASE("a second omen carried into a running raid makes it harder rather than starting another") {
    RaidManager manager;
    Raid* first = manager.createOrExtend(0, 64, 0, 0, "normal");
    REQUIRE(first != nullptr);
    CHECK(first->omenLevel == 1);
    CHECK(first->totalGroups() == 5);

    Raid* again = manager.createOrExtend(10, 64, 10, 1, "normal");
    CHECK(again == first);
    CHECK(first->omenLevel == 3);
    CHECK(manager.all().size() == 1);

    // parity: `MAX_RAID_OMEN_LEVEL`, which no number of omens gets past.
    for (std::int32_t carried = 0; carried < 10; carried++) {
        manager.createOrExtend(0, 64, 0, 4, "normal");
    }
    CHECK(first->omenLevel == Raid::kMaxOmenLevel);

    // Far enough away is its own raid.
    Raid* elsewhere = manager.createOrExtend(1000, 64, 1000, 0, "easy");
    REQUIRE(elsewhere != nullptr);
    CHECK(elsewhere != first);
    CHECK(manager.all().size() == 2);
}

TEST_CASE("a peaceful world has no raids at all") {
    RaidManager manager;
    CHECK(manager.createOrExtend(0, 64, 0, 0, "peaceful") == nullptr);
    CHECK(manager.all().empty());
}

TEST_CASE("a raid that is over stops being nearby anything") {
    RaidManager manager;
    Raid* raid = manager.createOrExtend(0, 64, 0, 0, "normal");
    REQUIRE(raid != nullptr);
    CHECK(manager.nearby(0.5, 64.5, 0.5) == raid);

    raid->status = RaidStatus::Victory;
    CHECK(manager.nearby(0.5, 64.5, 0.5) == nullptr);
    // And a won raid is what the next omen extends into a new one.
    Raid* next = manager.createOrExtend(0, 64, 0, 0, "normal");
    CHECK(next != raid);

    manager.forget({raid});
    CHECK(manager.all().size() == 1);
}
