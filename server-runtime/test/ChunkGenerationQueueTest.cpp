#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/world/ChunkGenerationQueue.hpp>
#include <basalt/wire/io/Platform.hpp>

#include <doctest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <mutex>
#include <vector>

using namespace basalt::server::runtime::world;
using basalt::server::world::Chunk;
namespace io = basalt::wire::io;

namespace {

/// A world of empty chunks, counting what it was asked to make: the real generator is a whole chunk
/// of noise per call, and what is under test is the queue.
[[nodiscard]] WorldState countingWorld(std::atomic<std::int32_t>& generated) {
    return WorldState{{}, [&generated](std::int32_t chunkX, std::int32_t chunkZ) {
                          ++generated;
                          return Chunk{chunkX, chunkZ};
                      }};
}

/// Waits for a condition the workers reach on their own, so a slow machine does not fail a test
/// that a fast one passes.
[[nodiscard]] bool eventually(const std::function<bool()>& done, std::int64_t timeoutMillis = 5000) {
    for (std::int64_t waited = 0; waited < timeoutMillis; waited += 5) {
        if (done()) {
            return true;
        }
        io::sleepFor(std::chrono::milliseconds{5});
    }
    return done();
}

} // namespace

TEST_CASE("a requested chunk turns up in the world without the caller waiting") {
    std::atomic<std::int32_t> generated{0};
    WorldState world = countingWorld(generated);
    ChunkGenerationQueue queue{world, "overworld", 2};
    CHECK(queue.dimension() == "overworld");

    queue.request(4, 5, 0);
    CHECK(eventually([&world] { return world.isLoaded(4, 5); }));
    CHECK(generated == 1);
    CHECK(eventually([&queue] { return queue.pendingCount() == 0; }));

    // Asking for what is already there is not work.
    queue.request(4, 5, 0);
    CHECK(queue.pendingCount() == 0);
    CHECK(generated == 1);
}

TEST_CASE("the same position asked for twice is generated once") {
    std::atomic<std::int32_t> generated{0};
    WorldState world = countingWorld(generated);
    // No workers, so what is queued stays queued and can be counted.
    ChunkGenerationQueue queue{world, "overworld", 0};

    queue.request(1, 1, 5);
    queue.request(1, 1, 9);
    queue.request(1, 1, 2);
    CHECK(queue.pendingCount() == 1);
    CHECK(queue.pendingKeys().contains(ChunkGenerationQueue::pack(1, 1)));

    queue.request(2, 2, 1);
    CHECK(queue.pendingCount() == 2);
    CHECK(generated == 0);
}

TEST_CASE("the closest chunk is generated first") {
    std::atomic<std::int32_t> generated{0};
    std::vector<std::int32_t> order;
    std::mutex orderMutex;
    // One worker, so what it takes next is the order the queue decided rather than a race between
    // several of them.
    WorldState world{{}, [&generated, &order, &orderMutex](std::int32_t chunkX, std::int32_t chunkZ) {
                         ++generated;
                         const std::lock_guard<std::mutex> lock{orderMutex};
                         order.push_back(chunkX);
                         return Chunk{chunkX, chunkZ};
                     }};
    ChunkGenerationQueue queue{world, "overworld", 0};

    // Enqueued furthest first, so order of arrival cannot be what decides.
    queue.request(30, 0, 300);
    queue.request(20, 0, 200);
    queue.request(10, 0, 100);
    // A closer player asking for the far one moves it up, and nothing moves it back down again.
    queue.request(30, 0, 1);
    queue.request(30, 0, 500);
    CHECK(queue.pendingCount() == 3);

    // Taken one at a time on this thread, so the order is the queue's and not a race between
    // workers.
    CHECK(queue.generateNext());
    CHECK(queue.generateNext());
    CHECK(queue.generateNext());
    CHECK_FALSE(queue.generateNext());
    const std::lock_guard<std::mutex> lock{orderMutex};
    CHECK(order == std::vector<std::int32_t>{30, 10, 20});
}

TEST_CASE("a queue that is full loses the farthest request, not the nearest") {
    std::atomic<std::int32_t> generated{0};
    WorldState world = countingWorld(generated);
    ChunkGenerationQueue queue{world, "overworld", 0};

    // Filled to the ceiling, each request farther than the one before it.
    const std::int32_t capacity = static_cast<std::int32_t>(ChunkGenerationQueue::kCapacity);
    for (std::int32_t at = 0; at < capacity; ++at) {
        queue.request(at, 0, 100 + at);
    }
    CHECK(queue.pendingCount() == ChunkGenerationQueue::kCapacity);

    // A nearer one gets in, the queue does not grow, and what it costs is the farthest request,
    // not the nearest and not an arbitrary one.
    queue.request(-1, 0, 1);
    CHECK(queue.pendingCount() == ChunkGenerationQueue::kCapacity);
    const std::unordered_set<std::int64_t> pending = queue.pendingKeys();
    CHECK(pending.contains(ChunkGenerationQueue::pack(-1, 0)));
    CHECK_FALSE(pending.contains(ChunkGenerationQueue::pack(capacity - 1, 0)));
    CHECK(pending.contains(ChunkGenerationQueue::pack(0, 0)));

    // A farther one does not get in at all.
    queue.request(-2, 0, 5000);
    CHECK(queue.pendingCount() == ChunkGenerationQueue::kCapacity);
    CHECK_FALSE(queue.pendingKeys().contains(ChunkGenerationQueue::pack(-2, 0)));
}

TEST_CASE("a chunk being made right now counts as pending, and is not asked for again") {
    ChunkGenerationQueue* queue = nullptr;
    std::size_t countedDuring = 0;
    bool keyDuring = false;
    bool requeuedDuring = false;

    // The generator asks the queue about itself while it is running, which is the only moment a
    // chunk is in flight rather than queued.
    WorldState world{{}, [&](std::int32_t chunkX, std::int32_t chunkZ) {
                         countedDuring = queue->pendingCount();
                         keyDuring = queue->pendingKeys().contains(ChunkGenerationQueue::pack(chunkX, chunkZ));
                         queue->request(chunkX, chunkZ, 0);
                         requeuedDuring = queue->pendingCount() > countedDuring;
                         return Chunk{chunkX, chunkZ};
                     }};
    ChunkGenerationQueue made{world, "overworld", 0};
    queue = &made;

    made.request(8, 9, 0);
    CHECK(made.generateNext());
    // In flight is still pending: a sweep that unloaded it now would be unloading it mid-generation.
    CHECK(countedDuring == 1);
    CHECK(keyDuring);
    // And asking for it again while it is being made queues nothing.
    CHECK_FALSE(requeuedDuring);
    CHECK(made.pendingCount() == 0);
}

TEST_CASE("what a player left behind is forgotten") {
    std::atomic<std::int32_t> generated{0};
    WorldState world = countingWorld(generated);
    ChunkGenerationQueue queue{world, "overworld", 0};

    for (std::int32_t at = 0; at < 10; ++at) {
        queue.request(at, 0, at);
    }
    CHECK(queue.retainWhere([](std::int32_t chunkX, std::int32_t) { return chunkX < 4; }) == 6);
    CHECK(queue.pendingCount() == 4);
    CHECK(queue.pendingKeys().contains(ChunkGenerationQueue::pack(3, 0)));
    CHECK_FALSE(queue.pendingKeys().contains(ChunkGenerationQueue::pack(4, 0)));
    // Nothing to forget is not an error.
    CHECK(queue.retainWhere([](std::int32_t, std::int32_t) { return true; }) == 0);
}

TEST_CASE("waiting for a batch answers every chunk of it") {
    std::atomic<std::int32_t> generated{0};
    WorldState world = countingWorld(generated);
    ChunkGenerationQueue queue{world, "overworld", 3};

    std::vector<std::pair<std::int32_t, std::int32_t>> positions;
    for (std::int32_t at = 0; at < 12; ++at) {
        positions.emplace_back(at, -at);
    }
    const std::vector<Chunk*> ready = queue.awaitAll(positions);
    REQUIRE(ready.size() == positions.size());
    // In the order asked for, whatever order they were made in.
    for (std::size_t at = 0; at < positions.size(); ++at) {
        CAPTURE(at);
        REQUIRE(ready[at] != nullptr);
        CHECK(ready[at]->chunkX() == positions[at].first);
        CHECK(ready[at]->chunkZ() == positions[at].second);
    }
    CHECK(generated == 12);
    // Waiting for nothing is nothing.
    CHECK(queue.awaitAll({}).empty());
}

TEST_CASE("a batch nobody is generating is generated by whoever asked") {
    std::atomic<std::int32_t> generated{0};
    WorldState world = countingWorld(generated);
    // No workers at all: the wait times out and the caller makes them itself rather than hanging.
    ChunkGenerationQueue queue{world, "overworld", 0, 50};

    const std::vector<Chunk*> ready = queue.awaitAll({{7, 7}});
    REQUIRE(ready.size() == 1);
    CHECK(ready[0]->chunkX() == 7);
    CHECK(world.isLoaded(7, 7));
    // The batch was handed to the pool first and only made here when the wait ran out, so the
    // request it never took is still sitting in the queue.
    CHECK(queue.pendingKeys().contains(ChunkGenerationQueue::pack(7, 7)));
}

TEST_CASE("a stopped queue takes no more work") {
    std::atomic<std::int32_t> generated{0};
    WorldState world = countingWorld(generated);
    ChunkGenerationQueue queue{world, "overworld", 2};

    queue.request(100, 100, 0);
    queue.stop();
    queue.awaitIdle(5000);
    CHECK(queue.pendingCount() == 0);

    // Whatever was queued is dropped, and nothing new is taken.
    const std::int32_t madeSoFar = generated;
    queue.request(200, 200, 0);
    CHECK(queue.pendingCount() == 0);
    io::sleepFor(std::chrono::milliseconds{50});
    CHECK(generated == madeSoFar);
    CHECK_FALSE(world.isLoaded(200, 200));
}

TEST_CASE("the pool leaves a core for the tick loop and one for the connections") {
    const std::int32_t workers = ChunkGenerationQueue::defaultWorkers();
    CHECK(workers >= 1);
    CHECK(workers <= 8);

    const std::optional<unsigned int> cores = io::cpuCoreCount();
    if (cores.has_value() && *cores >= 4) {
        // Two cores go to the tick loop and the connection threads, whatever is left is the pool.
        CHECK(workers == std::min(static_cast<std::int32_t>(*cores) - 2, 8));
    }
}

TEST_CASE("a position packs and unpacks, including west of the origin") {
    CHECK(ChunkGenerationQueue::unpackX(ChunkGenerationQueue::pack(-1, 0)) == -1);
    CHECK(ChunkGenerationQueue::unpackZ(ChunkGenerationQueue::pack(-1, 0)) == 0);
    CHECK(ChunkGenerationQueue::unpackX(ChunkGenerationQueue::pack(5, -9)) == 5);
    CHECK(ChunkGenerationQueue::unpackZ(ChunkGenerationQueue::pack(5, -9)) == -9);
    // The mask is what keeps the two halves apart.
    CHECK(ChunkGenerationQueue::pack(-1, 0) != ChunkGenerationQueue::pack(-1, -1));
}
