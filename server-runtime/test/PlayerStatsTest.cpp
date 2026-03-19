#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/player/PlayerStats.hpp>

#include <doctest.h>

#include <cstdint>
#include <thread>
#include <vector>

using basalt::server::runtime::player::PlayerStats;
namespace storage = basalt::server::storage;

TEST_CASE("a counter nobody has touched reads as nothing") {
    const PlayerStats stats;

    CHECK(stats.get(storage::stats::jump()) == 0);
}

TEST_CASE("increments add up, and an increment of nothing writes nothing") {
    PlayerStats stats;

    stats.increment(storage::stats::jump());
    stats.increment(storage::stats::jump(), 4);
    CHECK(stats.get(storage::stats::jump()) == 5);

    stats.increment(storage::stats::mobKills(), 0);
    CHECK(stats.snapshot(0).counters.contains(storage::stats::mobKills()) == false);
}

TEST_CASE("a counter can be taken back down as well as up") {
    PlayerStats stats;
    stats.increment(storage::stats::deaths(), 3);

    stats.increment(storage::stats::deaths(), -1);

    CHECK(stats.get(storage::stats::deaths()) == 2);
}

TEST_CASE("resetting a counter forgets it altogether") {
    PlayerStats stats;
    stats.increment(storage::stats::timeSinceRest(), 1000);

    stats.reset(storage::stats::timeSinceRest());

    CHECK(stats.get(storage::stats::timeSinceRest()) == 0);
    CHECK(stats.snapshot(0).counters.contains(storage::stats::timeSinceRest()) == false);
    // Resetting one nobody has touched is not an error.
    stats.reset(storage::stats::jump());
    CHECK(stats.get(storage::stats::jump()) == 0);
}

TEST_CASE("falling is counted in whole centimetres and drops the rest") {
    PlayerStats stats;

    stats.addFallen(3.567);
    CHECK(stats.get(storage::stats::fallOneCm()) == 356);

    stats.addFallen(0.0);
    stats.addFallen(-1.0);
    CHECK(stats.get(storage::stats::fallOneCm()) == 356);
}

TEST_CASE("damage is counted in tenths of a heart, rounded") {
    PlayerStats stats;

    stats.addDamageTaken(1.0F);
    CHECK(stats.get(storage::stats::damageTaken()) == 10);

    // parity: a half rounds up, which is what `roundToLong` does.
    stats.addDamageTaken(0.15F);
    CHECK(stats.get(storage::stats::damageTaken()) == 12);

    stats.addDamageDealt(2.44F);
    CHECK(stats.get(storage::stats::damageDealt()) == 24);
}

TEST_CASE("a snapshot carries the play time, and restoring it does not") {
    PlayerStats stats;
    stats.increment(storage::stats::jump(), 7);

    const storage::StoredStats saved = stats.snapshot(1234);
    CHECK(saved.get(storage::stats::playTime()) == 1234);
    CHECK(saved.get(storage::stats::jump()) == 7);

    PlayerStats restored;
    restored.restore(saved);
    CHECK(restored.get(storage::stats::jump()) == 7);
    // The play time has a column of its own, so a second copy here would let the two disagree.
    CHECK(restored.get(storage::stats::playTime()) == 0);
    CHECK(restored.snapshot(9).get(storage::stats::playTime()) == 9);
}

TEST_CASE("restoring replaces whatever was counted before") {
    PlayerStats stats;
    stats.increment(storage::stats::deaths(), 5);

    storage::StoredStats stored;
    stored.counters.emplace(storage::stats::jump(), 2);
    stats.restore(stored);

    CHECK(stats.get(storage::stats::deaths()) == 0);
    CHECK(stats.get(storage::stats::jump()) == 2);
}

TEST_CASE("two threads counting at once lose nothing") {
    PlayerStats stats;
    std::vector<std::jthread> threads;
    constexpr std::int32_t kEach = 5000;

    for (std::int32_t worker = 0; worker < 4; worker++) {
        threads.emplace_back([&stats] {
            for (std::int32_t step = 0; step < kEach; step++) {
                stats.increment(storage::stats::jump());
                stats.increment(storage::stats::walkOneCm());
            }
        });
    }
    threads.clear();

    CHECK(stats.get(storage::stats::jump()) == 4 * kEach);
    CHECK(stats.get(storage::stats::walkOneCm()) == 4 * kEach);
}
