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

/// The sizes of the groups, which is what the ordering promises something about.
[[nodiscard]] std::vector<std::size_t> sizesOf(const std::vector<std::unordered_set<std::int64_t>>& groups) {
    std::vector<std::size_t> sizes;
    sizes.reserve(groups.size());
    for (const std::unordered_set<std::int64_t>& group : groups) {
        sizes.push_back(group.size());
    }
    return sizes;
}

/// Every chunk of a square of sections, so a repartition has something to group.
[[nodiscard]] std::vector<std::int64_t> chunksOfSection(std::int32_t sectionX, std::int32_t sectionZ) {
    return {chunkKey(sectionX * 16, sectionZ * 16), chunkKey(sectionX * 16 + 15, sectionZ * 16 + 15)};
}

} // namespace

TEST_CASE("the grid is sixteen chunks a side and a section packs into one number") {
    // parity: Folia's gridExponent, which is what makes two chunks that can interact in one tick
    // land in the same section.
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
    // Both halves survive the round trip, negative ones included, which is what the mask is for.
    const std::int64_t negative = regionMap::sectionKey(-3, -7);
    CHECK(regionMap::sectionX(negative) == -3);
    CHECK(regionMap::sectionZ(negative) == -7);
    CHECK(regionMap::sectionKey(1, 2) != regionMap::sectionKey(2, 1));

    CHECK(regionMap::sectionKeyOfChunk(17, -1) == regionMap::sectionKey(1, -1));
}

TEST_CASE("sections that touch are one group, diagonals included") {
    CHECK(regionMap::components({}).empty());

    // One section on its own.
    CHECK(sizesOf(regionMap::components({regionMap::sectionKey(0, 0)})) == std::vector<std::size_t>{1});

    // Two that share an edge, and two that share only a corner: both are one group, because what
    // is in one can reach into the other inside a tick.
    CHECK(sizesOf(regionMap::components({regionMap::sectionKey(0, 0), regionMap::sectionKey(1, 0)})) ==
          std::vector<std::size_t>{2});
    CHECK(sizesOf(regionMap::components({regionMap::sectionKey(0, 0), regionMap::sectionKey(1, 1)})) ==
          std::vector<std::size_t>{2});

    // Two sections a gap apart are two groups.
    CHECK(sizesOf(regionMap::components({regionMap::sectionKey(0, 0), regionMap::sectionKey(2, 0)})) ==
          std::vector<std::size_t>{1, 1});

    // A staircase, which is what says every one of the eight directions is walked: each step needs
    // a different pair of offsets, and the walk starts from the lowest section key, so a direction
    // that is missing leaves a section behind rather than depending on where the walk began.
    CHECK(sizesOf(regionMap::components({regionMap::sectionKey(0, 0), regionMap::sectionKey(1, 1),
                                         regionMap::sectionKey(0, 2)})) == std::vector<std::size_t>{3});
    CHECK(sizesOf(regionMap::components({regionMap::sectionKey(0, 0), regionMap::sectionKey(1, 1),
                                         regionMap::sectionKey(2, 0)})) == std::vector<std::size_t>{3});
    CHECK(sizesOf(regionMap::components({regionMap::sectionKey(0, 0), regionMap::sectionKey(-1, -1),
                                         regionMap::sectionKey(-2, 0)})) == std::vector<std::size_t>{3});

    // A chain: everything reachable through neighbours ends up together however long it is.
    std::unordered_set<std::int64_t> chain;
    for (std::int32_t x = 0; x < 6; ++x) {
        chain.insert(regionMap::sectionKey(x, x));
    }
    chain.insert(regionMap::sectionKey(20, 20));
    const std::vector<std::unordered_set<std::int64_t>> groups = regionMap::components(chain);
    // Biggest first, so the survivor of a merge is the one that already owns most of the map.
    CHECK(sizesOf(groups) == std::vector<std::size_t>{6, 1});
    CHECK(groups[0].contains(regionMap::sectionKey(0, 0)));
    CHECK(groups[1].contains(regionMap::sectionKey(20, 20)));
    // Every section given is in exactly one group.
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
    // Asking again is the same region, wherever it is asked about.
    CHECK(&scheduler.regionFor("world", 1000, -1000) == &first);
    CHECK(scheduler.regions().size() == 1);

    // Empty means everything: an unsplit region owns whatever is loaded.
    CHECK(first.owns(0, 0));
    CHECK(first.owns(-9999, 9999));
    CHECK(first.ownsPosition(0.5, -0.5));

    // A second world is a second region, with its own owner and its own id.
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
    // Section 0 only, so the answer to ownsPosition is exactly the chunk the position is in.
    scheduler.repartition("world", chunksOfSection(0, 0));
    REQUIRE_FALSE(region.sections().empty());

    CHECK(region.ownsPosition(0.0, 0.0));
    CHECK(region.ownsPosition(255.9, 255.9));
    CHECK_FALSE(region.ownsPosition(256.0, 0.0));
    // parity: a floor and then a shift. The first block west of zero is chunk -1, and so is the
    // block at exactly -16.0, which is where the frozen Kotlin answered -2.
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

    // Two groups of chunks a long way apart, and still one region: this is the vanilla order of
    // tick, and it is the default.
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

    // Two islands: the region that was already there keeps one, and the other is new.
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

    // Each owns its own island and not the other's.
    Region& west = *scheduler.regionsOf("world")[0];
    Region& east = *scheduler.regionsOf("world")[1];
    CHECK(west.owns(0, 0));
    CHECK_FALSE(west.owns(40 * 16, 40 * 16));
    CHECK(east.owns(40 * 16, 40 * 16));
    CHECK(&scheduler.regionFor("world", 40 * 16, 40 * 16) == &east);

    // The islands grow into each other: one region survives and the other goes.
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
    // The survivor is the one that already owned most of it, so the fewest tasks moved.
    CHECK(scheduler.regionsOf("world")[0]->isPrimary());
    CHECK(scheduler.regionsOf("world")[0]->owns(0, 0));
    CHECK(scheduler.regionsOf("world")[0]->owns(40 * 16, 40 * 16));

    // Nothing loaded at all leaves the regions as they were: an empty world is not a reason to
    // throw away the region that owns it.
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

    // The task moved rather than being cancelled: it was asked against a piece of the world that
    // still exists, only under a new owner.
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

    // Three islands, so there are three regions to argue over the next one. The groups come out
    // biggest first, so the region that was already there takes the biggest.
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

    // Now a group that both the first and the second region overlap, but the first one only by a
    // single section: most of what it owned has been unloaded. The second one has more of this
    // group, so it is the one that keeps it and the fewest tasks move.
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

    // Only the first island is loaded now, so the second region has nothing left to own.
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

    // The same chunks again: the regions are already right, so there is nothing to report and no
    // log line saying zero regions merged.
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

    // Nowhere near either island: a chunk that is not loaded belongs to whoever asks first, and
    // making a region for it would be a region with nothing in it that nothing ever ticks.
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

    // The global owner goes first, the regions in the order their worlds were loaded, and what the
    // global owner left for the end of the turn goes last. Loaded, not sorted: "nether" comes
    // before "world" in an alphabet and after it on a server, and the order of tick is observable.
    CHECK(ran == std::vector<std::string>{"global", "world", "nether", "global end"});

    // Every region has been claimed by the ticking thread, which is what lets it run its own work
    // inline from here on.
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
        // The barrier: every region of this turn has finished before tick returns.
        CHECK(ticked.load() == before + 5);
        CHECK(insideAtOnce.load() == 0);
    }
    CHECK(ticked.load() == 100);
    // Not asserted as a number: how many actually overlap is up to the machine, and the point of
    // the pool is that they may, not that they must.
    CHECK(mostAtOnce.load() >= 1);
    scheduler.close();
}

TEST_CASE("one region does not go to the pool, and neither does one worker or one world") {
    const std::thread::id caller = std::this_thread::get_id();

    // With one region there is nothing to overlap, so the caller's own thread runs it either way.
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

    // One thread is not the same as none: it would still be somewhere else, which is measurable
    // and pointless, so a single thread means the caller does it.
    RegionScheduler single{true, 1};
    single.regionFor("a");
    single.regionFor("b");
    single.tick(rethrowOnFailure(), [&](Region&) { CHECK(std::this_thread::get_id() == caller); });
    single.close();

    // And with regions off there is no pool however many threads were asked for: the default mode
    // is the vanilla order of tick, on the thread that asked for it.
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

    // The handler is the tick loop's: what a region throws is reported, and the turn goes on.
    CHECK_THROWS_AS(scheduler.tick(rethrowOnFailure(), [](Region&) { throw std::runtime_error{"boom"}; }),
                    std::runtime_error);

    // Through an owner, which is where the handler is actually used.
    std::thread{[&] { scheduler.global().submit([] { throw std::runtime_error{"global boom"}; }); }}.join();
    scheduler.global().bindToCurrentThread();
    scheduler.tick([&failures](const std::exception& failure) { failures.emplace_back(failure.what()); },
                   [](Region&) {});
    CHECK(failures == std::vector<std::string>{"global boom"});
    scheduler.close();
}
