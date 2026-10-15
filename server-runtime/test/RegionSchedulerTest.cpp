#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/sched/RegionMap.hpp>
#include <basalt/server/runtime/sched/RegionScheduler.hpp>

#include <doctest.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

using namespace basalt::server::runtime::sched;

namespace {

[[nodiscard]] std::int64_t chunkKey(std::int32_t x, std::int32_t z) {
    return (static_cast<std::int64_t>(x) << 32) | (static_cast<std::int64_t>(z) & 0xFFFFFFFFLL);
}

[[nodiscard]] std::vector<std::size_t> sizesOf(const std::vector<std::unordered_set<std::int64_t>>& groups) {
    std::vector<std::size_t> sizes;
    sizes.reserve(groups.size());
    for (const std::unordered_set<std::int64_t>& group : groups) {
        sizes.push_back(group.size());
    }
    return sizes;
}

[[nodiscard]] std::vector<std::int64_t> chunksOfSection(std::int32_t sectionX, std::int32_t sectionZ) {
    return {chunkKey(sectionX * 16, sectionZ * 16), chunkKey(sectionX * 16 + 15, sectionZ * 16 + 15)};
}

} // namespace

TEST_CASE("the grid is sixteen chunks a side and a section packs into one number") {
    CHECK(regionMap::kGridExponent == 4);
    CHECK(regionMap::sectionOf(0) == 0);
    CHECK(regionMap::sectionOf(15) == 0);
    CHECK(regionMap::sectionOf(16) == 1);
    // A shift, not a division: chunk -1 belongs to section -1, not to section 0.
    CHECK(regionMap::sectionOf(-1) == -1);
    CHECK(regionMap::sectionOf(-16) == -1);
    CHECK(regionMap::sectionOf(-17) == -2);

    const std::int64_t key = regionMap::sectionKey(-3, 7);
    CHECK(regionMap::sectionX(key) == -3);
    CHECK(regionMap::sectionZ(key) == 7);
    const std::int64_t negative = regionMap::sectionKey(-3, -7);
    CHECK(regionMap::sectionX(negative) == -3);
    CHECK(regionMap::sectionZ(negative) == -7);
    CHECK(regionMap::sectionKey(1, 2) != regionMap::sectionKey(2, 1));

    CHECK(regionMap::sectionKeyOfChunk(17, -1) == regionMap::sectionKey(1, -1));
}

TEST_CASE("sections that touch are one group, diagonals included") {
    CHECK(regionMap::components({}).empty());

    CHECK(sizesOf(regionMap::components({regionMap::sectionKey(0, 0)})) == std::vector<std::size_t>{1});

    // Edge-sharing and corner-sharing sections are both one group: what is in one can reach into the other in a tick.
    CHECK(sizesOf(regionMap::components({regionMap::sectionKey(0, 0), regionMap::sectionKey(1, 0)})) ==
          std::vector<std::size_t>{2});
    CHECK(sizesOf(regionMap::components({regionMap::sectionKey(0, 0), regionMap::sectionKey(1, 1)})) ==
          std::vector<std::size_t>{2});

    CHECK(sizesOf(regionMap::components({regionMap::sectionKey(0, 0), regionMap::sectionKey(2, 0)})) ==
          std::vector<std::size_t>{1, 1});

    // A staircase says all eight directions are walked: a missing direction leaves a section behind.
    CHECK(sizesOf(regionMap::components({regionMap::sectionKey(0, 0), regionMap::sectionKey(1, 1),
                                         regionMap::sectionKey(0, 2)})) == std::vector<std::size_t>{3});
    CHECK(sizesOf(regionMap::components({regionMap::sectionKey(0, 0), regionMap::sectionKey(1, 1),
                                         regionMap::sectionKey(2, 0)})) == std::vector<std::size_t>{3});
    CHECK(sizesOf(regionMap::components({regionMap::sectionKey(0, 0), regionMap::sectionKey(-1, -1),
                                         regionMap::sectionKey(-2, 0)})) == std::vector<std::size_t>{3});

    std::unordered_set<std::int64_t> chain;
    for (std::int32_t x = 0; x < 6; ++x) {
        chain.insert(regionMap::sectionKey(x, x));
    }
    chain.insert(regionMap::sectionKey(20, 20));
    const std::vector<std::unordered_set<std::int64_t>> groups = regionMap::components(chain);
    CHECK(sizesOf(groups) == std::vector<std::size_t>{6, 1});
    CHECK(groups[0].contains(regionMap::sectionKey(0, 0)));
    CHECK(groups[1].contains(regionMap::sectionKey(20, 20)));
    std::size_t total = 0;
    for (const std::unordered_set<std::int64_t>& group : groups) {
        total += group.size();
    }
    CHECK(total == chain.size());
}

TEST_CASE("a world in one piece is one region that owns everything") {
    RegionScheduler scheduler;
    CHECK_FALSE(scheduler.regional());
    CHECK(scheduler.regions().empty());

    Region& first = scheduler.regionFor("world");
    CHECK(first.worldName() == "world");
    CHECK(first.isPrimary());
    CHECK(first.sections().empty());
    CHECK(first.owner().name() == "world");
    CHECK(&scheduler.regionFor("world", 1000, -1000) == &first);
    CHECK(scheduler.regions().size() == 1);

    CHECK(first.owns(0, 0));
    CHECK(first.owns(-9999, 9999));
    CHECK(first.ownsPosition(0.5, -0.5));

    Region& nether = scheduler.regionFor("nether");
    CHECK(nether.id() != first.id());
    CHECK(scheduler.regions().size() == 2);
    CHECK(scheduler.regionsOf("world").size() == 1);
    CHECK(scheduler.regionsOf("nether").size() == 1);
    CHECK(scheduler.regionsOf("the_end").empty());
}

TEST_CASE("which chunk a position is in, at the borders where it is easy to be one out") {
    RegionScheduler scheduler{true};
    Region& region = scheduler.regionFor("world");
    scheduler.repartition("world", chunksOfSection(0, 0));
    REQUIRE_FALSE(region.sections().empty());

    CHECK(region.ownsPosition(0.0, 0.0));
    CHECK(region.ownsPosition(255.9, 255.9));
    CHECK_FALSE(region.ownsPosition(256.0, 0.0));
    CHECK_FALSE(region.ownsPosition(-0.5, 0.0));
    CHECK_FALSE(region.ownsPosition(-16.0, 0.0));
    CHECK_FALSE(region.ownsPosition(0.0, -16.0));
}

TEST_CASE("with regions off nothing is ever repartitioned") {
    RegionScheduler scheduler;
    Region& only = scheduler.regionFor("world");
    std::int32_t changes = 0;

    std::vector<std::int64_t> far = chunksOfSection(0, 0);
    for (const std::int64_t chunk : chunksOfSection(40, 40)) {
        far.push_back(chunk);
    }
    scheduler.repartition("world", far, [&changes](std::int32_t, std::int32_t) { ++changes; });

    // Two far-apart groups are still one region: this is the vanilla order of tick, and the default.
    CHECK(scheduler.regionsOf("world").size() == 1);
    CHECK(only.sections().empty());
    CHECK(changes == 0);
}

TEST_CASE("a world that falls apart is split, and one that grows together is merged") {
    RegionScheduler scheduler{true};
    Region& first = scheduler.regionFor("world");
    const std::int32_t firstId = first.id();
    std::vector<std::pair<std::int32_t, std::int32_t>> changes;
    const auto record = [&changes](std::int32_t merged, std::int32_t split) { changes.emplace_back(merged, split); };

    std::vector<std::int64_t> apart = chunksOfSection(0, 0);
    for (const std::int64_t chunk : chunksOfSection(40, 40)) {
        apart.push_back(chunk);
    }
    scheduler.repartition("world", apart, record);
    REQUIRE(scheduler.regionsOf("world").size() == 2);
    CHECK(changes == std::vector<std::pair<std::int32_t, std::int32_t>>{{0, 1}});
    CHECK(scheduler.regionsOf("world")[0]->id() == firstId);
    CHECK(scheduler.regionsOf("world")[0]->isPrimary());
    CHECK_FALSE(scheduler.regionsOf("world")[1]->isPrimary());

    Region& west = *scheduler.regionsOf("world")[0];
    Region& east = *scheduler.regionsOf("world")[1];
    CHECK(west.owns(0, 0));
    CHECK_FALSE(west.owns(40 * 16, 40 * 16));
    CHECK(east.owns(40 * 16, 40 * 16));
    CHECK(&scheduler.regionFor("world", 40 * 16, 40 * 16) == &east);

    std::vector<std::int64_t> joined;
    for (std::int32_t section = 0; section <= 40; ++section) {
        for (const std::int64_t chunk : chunksOfSection(section, section)) {
            joined.push_back(chunk);
        }
    }
    changes.clear();
    scheduler.repartition("world", joined, record);
    CHECK(scheduler.regionsOf("world").size() == 1);
    CHECK(changes == std::vector<std::pair<std::int32_t, std::int32_t>>{{1, 0}});
    CHECK(scheduler.regionsOf("world")[0]->isPrimary());
    CHECK(scheduler.regionsOf("world")[0]->owns(0, 0));
    CHECK(scheduler.regionsOf("world")[0]->owns(40 * 16, 40 * 16));

    changes.clear();
    scheduler.repartition("world", {}, record);
    CHECK(scheduler.regionsOf("world").size() == 1);
    CHECK(changes.empty());
}

TEST_CASE("a merge takes the work of the region that goes with it") {
    RegionScheduler scheduler{true};
    scheduler.regionFor("world");
    std::vector<std::int64_t> apart = chunksOfSection(0, 0);
    for (const std::int64_t chunk : chunksOfSection(40, 40)) {
        apart.push_back(chunk);
    }
    scheduler.repartition("world", apart);
    REQUIRE(scheduler.regionsOf("world").size() == 2);

    std::int32_t ran = 0;
    Region& doomed = *scheduler.regionsOf("world")[1];
    doomed.owner().bindToCurrentThread();
    TaskHandle queued;
    std::thread{[&] { queued = doomed.owner().submit([&ran] { ++ran; }); }}.join();
    CHECK(doomed.owner().pending() == 1);

    std::vector<std::int64_t> joined;
    for (std::int32_t section = 0; section <= 40; ++section) {
        for (const std::int64_t chunk : chunksOfSection(section, section)) {
            joined.push_back(chunk);
        }
    }
    scheduler.repartition("world", joined);
    REQUIRE(scheduler.regionsOf("world").size() == 1);

    Region& survivor = *scheduler.regionsOf("world")[0];
    CHECK_FALSE(queued.isCancelled());
    CHECK(survivor.owner().pending() == 1);
    survivor.owner().bindToCurrentThread();
    CHECK(survivor.owner().runPending() == 1);
    CHECK(ran == 1);
}

TEST_CASE("the survivor of a merge is whoever already owns most of the group") {
    RegionScheduler scheduler{true};
    scheduler.regionFor("world");

    std::vector<std::int64_t> islands;
    for (const auto& [section, count] : std::vector<std::pair<std::int32_t, std::int32_t>>{{0, 1}, {40, 3}, {80, 2}}) {
        for (std::int32_t step = 0; step < count; ++step) {
            for (const std::int64_t chunk : chunksOfSection(section + step, 0)) {
                islands.push_back(chunk);
            }
        }
    }
    scheduler.repartition("world", islands);
    REQUIRE(scheduler.regionsOf("world").size() == 3);
    const std::int32_t biggest = scheduler.regionsOf("world")[0]->id();
    const std::int32_t middle = scheduler.regionsOf("world")[1]->id();
    const std::int32_t smallest = scheduler.regionsOf("world")[2]->id();
    CHECK(scheduler.regionsOf("world")[0]->sections().size() == 3);
    CHECK(scheduler.regionsOf("world")[1]->sections().size() == 2);
    CHECK(scheduler.regionsOf("world")[2]->sections().size() == 1);

    // The second region has more of the group than the first (one section), so it keeps it and fewer tasks move.
    std::vector<std::int64_t> joined;
    for (std::int32_t section = 42; section <= 82; ++section) {
        for (const std::int64_t chunk : chunksOfSection(section, 0)) {
            joined.push_back(chunk);
        }
    }
    scheduler.repartition("world", joined);
    REQUIRE(scheduler.regionsOf("world").size() == 1);
    CHECK(scheduler.regionsOf("world")[0]->id() == middle);
    CHECK(scheduler.regionsOf("world")[0]->id() != biggest);
    CHECK(scheduler.regionsOf("world")[0]->id() != smallest);
}

TEST_CASE("a region whose chunks are all unloaded hands its work over rather than dropping it") {
    RegionScheduler scheduler{true};
    scheduler.regionFor("world");
    std::vector<std::int64_t> islands;
    for (const std::int32_t section : {0, 40}) {
        for (const std::int64_t chunk : chunksOfSection(section, 0)) {
            islands.push_back(chunk);
        }
    }
    scheduler.repartition("world", islands);
    REQUIRE(scheduler.regionsOf("world").size() == 2);

    Region& leaving = *scheduler.regionsOf("world")[1];
    leaving.owner().bindToCurrentThread();
    std::int32_t ran = 0;
    TaskHandle queued;
    std::thread{[&] { queued = leaving.owner().submit([&ran] { ++ran; }); }}.join();

    scheduler.repartition("world", chunksOfSection(0, 0));
    REQUIRE(scheduler.regionsOf("world").size() == 1);
    Region& survivor = *scheduler.regionsOf("world")[0];
    CHECK_FALSE(queued.isCancelled());
    CHECK(survivor.owner().pending() == 1);
    survivor.owner().bindToCurrentThread();
    CHECK(survivor.owner().runPending() == 1);
    CHECK(ran == 1);
}

TEST_CASE("a repartition that changes nothing says nothing") {
    RegionScheduler scheduler{true};
    scheduler.regionFor("world");
    std::int32_t reports = 0;
    const auto record = [&reports](std::int32_t, std::int32_t) { ++reports; };

    std::vector<std::int64_t> islands;
    for (const std::int32_t section : {0, 40}) {
        for (const std::int64_t chunk : chunksOfSection(section, 0)) {
            islands.push_back(chunk);
        }
    }
    scheduler.repartition("world", islands, record);
    CHECK(reports == 1);

    scheduler.repartition("world", islands, record);
    scheduler.repartition("world", islands, record);
    CHECK(reports == 1);
    CHECK(scheduler.regionsOf("world").size() == 2);
}

TEST_CASE("a chunk nobody owns is answered by a region rather than by a new one") {
    RegionScheduler scheduler{true};
    scheduler.regionFor("world");
    std::vector<std::int64_t> islands;
    for (const std::int32_t section : {0, 40}) {
        for (const std::int64_t chunk : chunksOfSection(section, 0)) {
            islands.push_back(chunk);
        }
    }
    scheduler.repartition("world", islands);
    REQUIRE(scheduler.regionsOf("world").size() == 2);

    // A chunk that is not loaded belongs to whoever asks first: making a region for it would leave one nothing ticks.
    Region& answer = scheduler.regionFor("world", 9000 * 16, 9000 * 16);
    CHECK(&answer == scheduler.regionsOf("world")[0]);
    CHECK(scheduler.regionsOf("world").size() == 2);
}

TEST_CASE("a turn runs the global owner around the regions") {
    RegionScheduler scheduler;
    scheduler.regionFor("world");
    scheduler.regionFor("nether");
    std::vector<std::string> ran;

    scheduler.global().schedule(0, [&ran] { ran.emplace_back("global"); });
    scheduler.global().scheduleAtEnd([&ran] { ran.emplace_back("global end"); });
    scheduler.tick(rethrowOnFailure(), [&ran](Region& region) { ran.push_back(region.worldName()); });

    // Loaded order, not sorted: "nether" is before "world" alphabetically and after it on a server.
    CHECK(ran == std::vector<std::string>{"global", "world", "nether", "global end"});

    for (const Region* region : scheduler.regions()) {
        CHECK(region->owner().isClaimed());
    }

    scheduler.close();
    CHECK(scheduler.global().isClosed());
    for (const Region* region : scheduler.regions()) {
        CHECK(region->owner().isClosed());
    }
}

TEST_CASE("with several worker threads every region still finishes before the next turn") {
    RegionScheduler scheduler{true, 4};
    for (const std::string& world : {"a", "b", "c", "d", "e"}) {
        scheduler.regionFor(world);
    }
    std::atomic<std::int32_t> ticked{0};
    std::atomic<std::int32_t> insideAtOnce{0};
    std::atomic<std::int32_t> mostAtOnce{0};

    for (std::int32_t turn = 0; turn < 20; ++turn) {
        const std::int32_t before = ticked.load();
        scheduler.tick(rethrowOnFailure(), [&](Region&) {
            const std::int32_t inside = insideAtOnce.fetch_add(1) + 1;
            std::int32_t seen = mostAtOnce.load();
            while (seen < inside && !mostAtOnce.compare_exchange_weak(seen, inside)) {
            }
            ticked.fetch_add(1);
            insideAtOnce.fetch_sub(1);
        });
        CHECK(ticked.load() == before + 5);
        CHECK(insideAtOnce.load() == 0);
    }
    CHECK(ticked.load() == 100);
    // Not asserted as a number: how many actually overlap is up to the machine.
    CHECK(mostAtOnce.load() >= 1);
    scheduler.close();
}

TEST_CASE("one region does not go to the pool, and neither does one worker or one world") {
    const std::thread::id caller = std::this_thread::get_id();

    RegionScheduler pooled{true, 4};
    Region& only = pooled.regionFor("world");
    std::int32_t ran = 0;
    pooled.tick(rethrowOnFailure(), [&](Region&) {
        ++ran;
        CHECK(std::this_thread::get_id() == caller);
    });
    CHECK(ran == 1);
    CHECK(only.owner().isCurrent());
    pooled.close();

    RegionScheduler single{true, 1};
    single.regionFor("a");
    single.regionFor("b");
    single.tick(rethrowOnFailure(), [&](Region&) { CHECK(std::this_thread::get_id() == caller); });
    single.close();

    RegionScheduler plain{false, 4};
    plain.regionFor("a");
    plain.regionFor("b");
    plain.tick(rethrowOnFailure(), [&](Region&) { CHECK(std::this_thread::get_id() == caller); });
    plain.close();
}

TEST_CASE("a region that throws costs its own turn and not the rest") {
    RegionScheduler scheduler;
    scheduler.regionFor("a");
    scheduler.regionFor("b");
    std::vector<std::string> failures;

    CHECK_THROWS_AS(scheduler.tick(rethrowOnFailure(), [](Region&) { throw std::runtime_error{"boom"}; }),
                    std::runtime_error);

    std::thread{[&] { scheduler.global().submit([] { throw std::runtime_error{"global boom"}; }); }}.join();
    scheduler.global().bindToCurrentThread();
    scheduler.tick([&failures](const std::exception& failure) { failures.emplace_back(failure.what()); },
                   [](Region&) {});
    CHECK(failures == std::vector<std::string>{"global boom"});
    scheduler.close();
}
