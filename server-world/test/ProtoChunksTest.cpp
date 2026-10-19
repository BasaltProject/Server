#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/ProtoChunks.hpp>
#include <basalt/server/world/terrain/ProtoRegion.hpp>

#include <doctest.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <mutex>
#include <optional>
#include <thread>
#include <utility>
#include <vector>

using namespace basalt::server::world;
using namespace basalt::server::world::terrain;
namespace registry = basalt::server::registry;

namespace {

struct Recorder {
    std::mutex mutex;
    std::vector<std::pair<std::int32_t, std::int32_t>> decorated;
    std::int32_t terrains = 0;

    [[nodiscard]] ProtoChunks::Hooks hooks() {
        ProtoChunks::Hooks hooks;
        hooks.terrain = [this](std::int32_t chunkX, std::int32_t chunkZ, const blending::Blender*) {
            const std::lock_guard lock{mutex};
            terrains++;
            return realTerrainGenerator::GeneratedTerrain{Chunk{chunkX, chunkZ}, nullptr, nullptr};
        };
        hooks.adopt = [](Chunk chunk) {
            return realTerrainGenerator::GeneratedTerrain{std::move(chunk), nullptr, nullptr};
        };
        hooks.decorate = [this](realTerrainGenerator::GeneratedTerrain& terrain, ProtoRegion& region) {
            const Chunk& chunk = terrain.chunk();
            region.setBlockStateId((chunk.chunkX() * 16) + 16, 0, chunk.chunkZ() * 16,
                                   registry::blocks::stone().stateId);
            const std::lock_guard lock{mutex};
            decorated.emplace_back(chunk.chunkX(), chunk.chunkZ());
        };
        return hooks;
    }
};

[[nodiscard]] std::vector<Chunk> runDry(ProtoChunks& chunks) {
    while (chunks.step()) {
    }
    return chunks.takeFinished();
}

} // namespace

TEST_CASE("a chunk is full once its eight neighbours decorated, and theirs wrote into it") {
    Recorder recorder;
    ProtoChunks chunks{recorder.hooks()};
    chunks.demand(0, 0, 0);
    const std::vector<Chunk> finished = runDry(chunks);

    REQUIRE(finished.size() == 1);
    CHECK(finished[0].stage() == ChunkStage::Full);
    CHECK(recorder.terrains == 25);
    CHECK(finished[0].blockStateId(0, 0, 0) == registry::blocks::stone().stateId);
    // parity: the 3x3 is scheduled x outermost.
    const std::vector<std::pair<std::int32_t, std::int32_t>> expected{{-1, -1}, {-1, 0}, {-1, 1}, {0, -1}, {0, 0},
                                                                      {0, 1},   {1, -1}, {1, 0},  {1, 1}};
    CHECK(recorder.decorated == expected);
}

TEST_CASE("the same requests decorate in the same order however many threads step") {
    std::vector<std::pair<std::int32_t, std::int32_t>> alone;
    {
        Recorder recorder;
        ProtoChunks chunks{recorder.hooks()};
        for (std::int32_t x = 0; x < 4; ++x) {
            for (std::int32_t z = 0; z < 4; ++z) {
                chunks.demand(x, z, 0);
            }
        }
        CHECK(runDry(chunks).size() == 16);
        alone = recorder.decorated;
    }
    Recorder recorder;
    ProtoChunks chunks{recorder.hooks()};
    for (std::int32_t x = 0; x < 4; ++x) {
        for (std::int32_t z = 0; z < 4; ++z) {
            chunks.demand(x, z, 0);
        }
    }
    std::vector<std::jthread> workers;
    for (std::int32_t index = 0; index < 4; ++index) {
        workers.emplace_back([&chunks] {
            std::int32_t idle = 0;
            while (idle < 200) {
                idle = chunks.step() ? 0 : idle + 1;
                std::this_thread::yield();
            }
        });
    }
    workers.clear();
    CHECK(chunks.takeFinished().size() == 16);
    const auto turnOf = [](const auto& order, std::pair<std::int32_t, std::int32_t> chunk) {
        return std::ranges::find(order, chunk) - order.begin();
    };
    for (const auto& first : alone) {
        for (const auto& second : alone) {
            if (std::max(std::abs(first.first - second.first), std::abs(first.second - second.second)) > 2) {
                continue;
            }
            CHECK((turnOf(alone, first) < turnOf(alone, second)) ==
                  (turnOf(recorder.decorated, first) < turnOf(recorder.decorated, second)));
        }
    }
}

TEST_CASE("a proto chunk read back from disk decorates without being made again") {
    Recorder recorder;
    ProtoChunks::Hooks hooks = recorder.hooks();
    hooks.load = [](std::int32_t chunkX, std::int32_t chunkZ) -> std::optional<ProtoChunks::Stored> {
        if (chunkX != 0 || chunkZ != 0) {
            return std::nullopt;
        }
        Chunk stored{0, 0};
        stored.setBlockStateId(5, 5, 5, registry::blocks::bedrock().stateId);
        return ProtoChunks::Stored{.chunk = std::move(stored), .stage = ChunkStage::Terrain};
    };
    ProtoChunks chunks{std::move(hooks)};
    chunks.demand(0, 0, 0);
    const std::vector<Chunk> finished = runDry(chunks);
    REQUIRE(finished.size() == 1);
    CHECK(recorder.terrains == 24);
    CHECK(finished[0].blockStateId(5, 5, 5) == registry::blocks::bedrock().stateId);
}

TEST_CASE("a forced chunk is full with two rings round it and decorated five out") {
    Recorder recorder;
    ProtoChunks chunks{recorder.hooks()};
    chunks.demandArea(0, 0, 2, 5, 0);
    const std::vector<Chunk> finished = runDry(chunks);

    CHECK(finished.size() == 25);
    CHECK(recorder.decorated.size() == 121);
    // parity: the full square's 7x7 goes before the rest of the area wherever the two overlap.
    const auto turnOf = [&recorder](std::pair<std::int32_t, std::int32_t> chunk) {
        return std::ranges::find(recorder.decorated, chunk) - recorder.decorated.begin();
    };
    for (const auto& inner : recorder.decorated) {
        for (const auto& outer : recorder.decorated) {
            const bool innerInBlock = std::abs(inner.first) <= 3 && std::abs(inner.second) <= 3;
            const bool outerInBlock = std::abs(outer.first) <= 3 && std::abs(outer.second) <= 3;
            if (innerInBlock && !outerInBlock &&
                std::max(std::abs(inner.first - outer.first), std::abs(inner.second - outer.second)) <= 2) {
                CHECK(turnOf(inner) < turnOf(outer));
            }
        }
    }
}
