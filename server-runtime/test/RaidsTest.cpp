#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/entity/IntHashOrder.hpp>
#include <basalt/server/runtime/entity/Raids.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <doctest.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

using basalt::server::runtime::entity::Raid;
using basalt::server::runtime::entity::RaiderType;
using basalt::server::runtime::entity::RaidManager;
using basalt::server::runtime::entity::RaidPos;
using basalt::server::runtime::entity::RaidStatus;
namespace raidWaves = basalt::server::runtime::entity::raidWaves;

TEST_CASE("each wave brings its own count of each kind of raider") {
    CHECK(raidWaves::groupsFor("peaceful") == 0);
    CHECK(raidWaves::groupsFor("easy") == 3);
    CHECK(raidWaves::groupsFor("normal") == 5);
    CHECK(raidWaves::groupsFor("hard") == 7);
    // parity: `Raid.RaiderType`, whose first wave is four pillagers and nothing else.
    CHECK(raidWaves::countFor(RaiderType::Pillager, 1) == 4);
    CHECK(raidWaves::countFor(RaiderType::Vindicator, 1) == 0);
    CHECK(raidWaves::countFor(RaiderType::Vindicator, 7) == 5);
    CHECK(raidWaves::countFor(RaiderType::Witch, 4) == 3);
    CHECK(raidWaves::countFor(RaiderType::Ravager, 3) == 1);
}

TEST_CASE("bonus spawns draw off the raid's own random only where vanilla draws") {
    Raid raid{RaidPos{0, 64, 0}, 5, 42};
    Raid twin{RaidPos{0, 64, 0}, 5, 42};
    // parity: an evoker and a witch before the fifth wave never touch the random.
    CHECK(raid.spawnsFor(RaiderType::Evoker, 5, "normal", false) == 1);
    CHECK(raid.spawnsFor(RaiderType::Witch, 4, "normal", false) == 3);
    CHECK(raid.random.nextInt(1000) == twin.random.nextInt(1000));
    // On normal a pillager gets zero or one more: one draw of `nextInt(2)`.
    const std::int32_t expected = 4 + twin.random.nextInt(2);
    CHECK(raid.spawnsFor(RaiderType::Pillager, 1, "normal", false) == expected);
    // A bonus wave counts from the last regular one.
    CHECK(raid.spawnsFor(RaiderType::Evoker, 6, "normal", true) == raidWaves::countFor(RaiderType::Evoker, 5));
}

TEST_CASE("a raid with a strong omen earns one bonus wave after its last") {
    Raid raid{RaidPos{0, 64, 0}, 3, 1};
    raid.absorbOmen(0);
    CHECK(raid.raidOmenLevel == 1);
    raid.groupsSpawned = 3;
    CHECK_FALSE(raid.hasMoreWaves());
    raid.absorbOmen(0);
    CHECK(raid.hasBonusWave());
    CHECK(raid.hasMoreWaves());
    CHECK(raid.shouldSpawnBonusGroup());
    raid.groupsSpawned = 4;
    CHECK_FALSE(raid.hasMoreWaves());
    // parity: `MAX_RAID_OMEN_LEVEL`.
    for (std::int32_t carried = 0; carried < 10; carried++) {
        raid.absorbOmen(4);
    }
    CHECK(raid.raidOmenLevel == Raid::kMaxOmenLevel);
}

TEST_CASE("only an active raid strictly inside the radius is nearby, and the closest wins") {
    RaidManager manager;
    const std::int32_t far = manager.add(std::make_unique<Raid>(RaidPos{60, 64, 0}, 5, 1));
    const std::int32_t near = manager.add(std::make_unique<Raid>(RaidPos{10, 64, 0}, 5, 2));
    // parity: `getUniqueId` bumps before it answers.
    CHECK(far == 2);
    CHECK(near == 3);
    CHECK(manager.nearby(RaidPos{0, 64, 0}) == manager.get(near));
    manager.get(near)->active = false;
    CHECK(manager.nearby(RaidPos{0, 64, 0}) == manager.get(far));
    // Ninety-six blocks is exactly the edge, and the edge is out.
    CHECK(manager.nearby(RaidPos{-36, 64, 0}) == nullptr);
}

TEST_CASE("the raids survive a restart with their ids, heroes and progress") {
    const std::filesystem::path root = std::filesystem::path{BASALT_TEST_SCRATCH_DIR} / "raids-saved";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);

    RaidManager manager;
    auto raid = std::make_unique<Raid>(RaidPos{8, 70, -3}, 7, 1);
    raid->started = true;
    raid->raidOmenLevel = 3;
    raid->groupsSpawned = 2;
    raid->cooldownTicks = 120;
    raid->ticksActive = 4000;
    raid->totalHealth = 88.5F;
    raid->status = RaidStatus::Loss;
    raid->heroes.emplace_back(0x0102030405060708ULL, 0x1112131415161718ULL);
    const std::int32_t id = manager.add(std::move(raid));
    manager.tick = 450;
    manager.save(root.string());

    RaidManager reloaded;
    reloaded.load(root.string());
    const Raid* read = reloaded.get(id);
    REQUIRE(read != nullptr);
    CHECK(read->center == RaidPos{8, 70, -3});
    CHECK(read->started);
    CHECK(read->raidOmenLevel == 3);
    CHECK(read->groupsSpawned == 2);
    CHECK(read->cooldownTicks == 120);
    CHECK(read->ticksActive == 4000);
    CHECK(read->totalHealth == 88.5F);
    CHECK(read->numGroups == 7);
    CHECK(read->status == RaidStatus::Loss);
    REQUIRE(read->heroes.size() == 1);
    CHECK(read->heroes.front().high() == 0x0102030405060708ULL);
    CHECK(reloaded.tick == 450);
    // The next raid does not reuse a saved id.
    CHECK(reloaded.add(std::make_unique<Raid>(RaidPos{0, 0, 0}, 3, 2)) == id + 1);
}

TEST_CASE("the raid table visits every raid once even when ticking removes some") {
    basalt::server::runtime::entity::IntHashOrder order;
    for (std::int32_t key = 2; key < 40; ++key) {
        order.insert(key);
    }
    std::vector<std::int32_t> seen;
    order.iterate([&seen](std::int32_t key) {
        seen.push_back(key);
        return key % 3 == 0;
    });
    std::ranges::sort(seen);
    std::vector<std::int32_t> all;
    for (std::int32_t key = 2; key < 40; ++key) {
        all.push_back(key);
    }
    CHECK(seen == all);
    for (const std::int32_t key : order.order()) {
        CHECK(key % 3 != 0);
    }
    CHECK(order.size() == static_cast<std::int32_t>(all.size() - 13));
}
