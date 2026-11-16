#include <basalt/metamorph/region/LinearRegion.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/ChunkDataCodec.hpp>
#include <basalt/server/world/region/ChunkNbt.hpp>
#include <basalt/server/world/terrain/decoration/tree/LeafDistances.hpp>
#include <basalt/wire/io/FileIO.hpp>
#include <basalt/wire/nbt/NbtIO.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/protocol/play/WorldPackets.hpp>

#include <cstdio>
#include <ctime>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;
using namespace basalt;

struct Clock {
    using time_point = double;

    static double now() {
        timespec at{};
        clock_gettime(CLOCK_THREAD_CPUTIME_ID, &at);
        return static_cast<double>(at.tv_sec) * 1e6 + static_cast<double>(at.tv_nsec) / 1e3;
    }
};

namespace {

double since(Clock::time_point t) { return Clock::now() - t; }

std::uint64_t digest(const server::world::chunkDataCodec::LightData& light) {
    std::uint64_t hash = 1469598103934665603ULL;
    const auto mix = [&hash](std::uint64_t value) { hash = (hash ^ value) * 1099511628211ULL; };
    for (const auto& list :
         {&light.skyLightMask, &light.blockLightMask, &light.emptySkyLightMask, &light.emptyBlockLightMask}) {
        for (const std::int64_t word : *list) {
            mix(static_cast<std::uint64_t>(word));
        }
    }
    for (const auto* layer : {&light.skyLight, &light.blockLight}) {
        for (const auto& bytes : *layer) {
            for (const std::uint8_t byte : bytes) {
                mix(byte);
            }
        }
    }
    return hash;
}

} // namespace

int main(int argc, char** argv) {
    const std::string folder = argc > 1 ? argv[1] : "region";
    const std::size_t limit = argc > 2 ? std::stoul(argv[2]) : 256;
    std::vector<std::unique_ptr<server::world::Chunk>> chunks;
    for (const auto& entry : fs::directory_iterator(folder)) {
        if (entry.path().extension() != ".linear") {
            continue;
        }
        const auto bytes = wire::io::fileIO::readBytes(entry.path().string());
        auto region = metamorph::region::LinearRegion::read(*bytes);
        if (!region) {
            continue;
        }
        for (int z = 0; z < 32 && chunks.size() < limit; ++z) {
            for (int x = 0; x < 32 && chunks.size() < limit; ++x) {
                const auto blob = region->chunkAt(x, z);
                if (blob.empty()) {
                    continue;
                }
                auto tag = wire::nbt::nbtIO::read(blob);
                auto chunk = server::world::region::chunkNbt::read(*tag->tag.get<wire::nbt::NbtCompound>(),
                                                                   server::world::kOverworldHeight);
                if (chunk) {
                    chunks.push_back(std::make_unique<server::world::Chunk>(std::move(*chunk)));
                }
            }
        }
    }
    if (argc > 3) {
        const auto log = *server::registry::blocks::defaultStateIdOf(wire::common::Identifier{"minecraft", "oak_log"});
        const auto leaf =
            *server::registry::blocks::defaultStateIdOf(wire::common::Identifier{"minecraft", "oak_leaves"});
        std::uint64_t seed = 12345;
        const auto next = [&seed](std::uint32_t bound) {
            seed = seed * 6364136223846793005ULL + 1442695040888963407ULL;
            return static_cast<std::int32_t>((seed >> 33) % bound);
        };
        for (auto& chunk : chunks) {
            for (int tree = 0; tree < 6; ++tree) {
                const std::int32_t x = next(16);
                const std::int32_t z = next(16);
                const std::int32_t base = 64 + next(8);
                for (std::int32_t y = base; y < base + 5; ++y) {
                    chunk->setBlockStateId(x, y, z, log);
                }
                for (std::int32_t dx = -2; dx <= 2; ++dx) {
                    for (std::int32_t dz = -2; dz <= 2; ++dz) {
                        for (std::int32_t y = base + 3; y < base + 7; ++y) {
                            const std::int32_t lx = x + dx;
                            const std::int32_t lz = z + dz;
                            if (lx >= 0 && lx < 16 && lz >= 0 && lz < 16 && next(5) != 0 && (dx != 0 || dz != 0)) {
                                chunk->setBlockStateId(lx, y, lz, leaf);
                            }
                        }
                    }
                }
            }
        }
    }
    std::printf("chunks %zu\n", chunks.size());
    std::unordered_map<std::int64_t, server::world::Chunk*> byPos;
    for (auto& chunk : chunks) {
        byPos[(static_cast<std::int64_t>(chunk->chunkX()) << 32) ^ static_cast<std::uint32_t>(chunk->chunkZ())] =
            chunk.get();
    }
    const auto chunkAt = [&byPos](std::int32_t x, std::int32_t z) -> const server::world::Chunk* {
        const auto found = byPos.find((static_cast<std::int64_t>(x) << 32) ^ static_cast<std::uint32_t>(z));
        return found == byPos.end() ? nullptr : found->second;
    };
    const auto blockAt = [&chunkAt](std::int32_t x, std::int32_t y, std::int32_t z) -> std::int32_t {
        const server::world::Chunk* found = chunkAt(x >> 4, z >> 4);
        if (found == nullptr || y < found->verticalBounds().minY() || y > found->verticalBounds().maxY()) {
            return 0;
        }
        return found->blockStateId(x & 15, y, z & 15);
    };
    const auto settle = [&](auto& chunk) {
        namespace leaf = server::world::terrain::decoration::tree::leafDistances;
        if constexpr (requires { leaf::settle(chunk, blockAt); }) {
            leaf::settle(chunk, blockAt);
        } else {
            leaf::settle(chunk, chunkAt);
        }
    };
    std::uint64_t total = 0;
    double cold = 0;
    double warm = 0;
    double leaf = 0;
    for (auto& chunk : chunks) {
        auto t = Clock::now();
        total = total * 31 + digest(server::world::chunkDataCodec::encodeLight(*chunk, true));
        cold += since(t);
    }
    for (int round = 0; round < 5; ++round) {
        for (auto& chunk : chunks) {
            auto t = Clock::now();
            total = total * 31 + digest(server::world::chunkDataCodec::encodeLight(*chunk, true));
            warm += since(t);
        }
    }
    double warmBytes = 0;
    for (int round = 0; round < 5; ++round) {
        for (auto& chunk : chunks) {
            const server::world::light::LightWindow window{*chunk, chunkAt};
            auto t = Clock::now();
            total += server::world::chunkDataCodec::encodeLightBytes(*chunk, true, window)->size();
            warmBytes += since(t);
        }
    }
    std::printf("encodeLightBytes warm %.1f us/chunk\n", warmBytes / (5 * static_cast<double>(chunks.size())));
    for (int round = 0; round < 3; ++round) {
        for (auto& chunk : chunks) {
            auto t = Clock::now();
            settle(*chunk);
            leaf += since(t);
        }
    }
    {
        namespace play = wire::protocol::play;
        std::size_t mismatches = 0;
        play::LightData emptyLight;
        wire::io::ByteBuffer emptyBuffer;
        emptyLight.write(emptyBuffer);
        for (auto& chunk : chunks) {
            for (const bool sky : {true, false}) {
                const auto window = server::world::light::LightWindow{*chunk, chunkAt};
                const auto data = server::world::chunkDataCodec::encodeLight(*chunk, sky, window);
                const play::LightData wireLight{.skyLightMask = data.skyLightMask,
                                                .blockLightMask = data.blockLightMask,
                                                .emptySkyLightMask = data.emptySkyLightMask,
                                                .emptyBlockLightMask = data.emptyBlockLightMask,
                                                .skyLight = data.skyLight,
                                                .blockLight = data.blockLight};
                const auto bytes = server::world::chunkDataCodec::encodeLightBytes(*chunk, sky, window);
                play::MapChunkPacket packet{.x = chunk->chunkX(), .z = chunk->chunkZ(), .chunkData = {1, 2, 3}};
                packet.light = wireLight;
                wire::io::ByteBuffer expected;
                packet.write(expected);
                packet.light = {};
                wire::io::ByteBuffer actual;
                packet.write(actual);
                actual.trimTail(emptyBuffer.readableBytes());
                actual.writeBytes(*bytes);
                mismatches += expected.toVector() == actual.toVector() ? 0 : 1;
                wire::io::ByteBuffer expectedUpdate;
                play::UpdateLightPacket{.chunkX = 3, .chunkZ = -4, .light = wireLight}.write(expectedUpdate);
                wire::io::ByteBuffer actualUpdate;
                wire::protocol::varInt::write(actualUpdate, 3);
                wire::protocol::varInt::write(actualUpdate, -4);
                actualUpdate.writeBytes(*bytes);
                mismatches += expectedUpdate.toVector() == actualUpdate.toVector() ? 0 : 1;
            }
        }
        std::printf("packet byte mismatches %zu\n", mismatches);
    }
    std::uint64_t states = 1469598103934665603ULL;
    std::vector<std::int32_t> ids(4096);
    for (auto& chunk : chunks) {
        for (std::size_t i = 0; i < chunk->sectionCount(); ++i) {
            chunk->section(i).copyBlockStateIdsInto(ids);
            for (const std::int32_t id : ids) {
                states = (states ^ static_cast<std::uint32_t>(id)) * 1099511628211ULL;
            }
        }
    }
    std::printf("states %016llx\n", static_cast<unsigned long long>(states));
    const auto n = static_cast<double>(chunks.size());
    std::printf("encodeLight cold %.1f us/chunk, warm %.1f us/chunk, leaf settle %.1f us/chunk, digest %016llx\n",
                cold / n, warm / (5 * n), leaf / (3 * n), static_cast<unsigned long long>(total));
}
