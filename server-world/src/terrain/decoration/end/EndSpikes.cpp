#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/BlockStateFamilies.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/end/EndSpikes.hpp>

#include <cmath>
#include <cstdlib>
#include <map>
#include <mutex>
#include <numbers>
#include <utility>

namespace basalt::server::world::terrain::decoration::end::endSpikes {
namespace {

constexpr double kSpikeDistance = 42.0;

} // namespace

const std::vector<EndSpike>& forSeed(std::int64_t seed) {
    // The reference outlives the lock because map nodes never move and nothing erases.
    static std::mutex mutex;
    static std::map<std::int64_t, std::vector<EndSpike>> cache;
    const std::scoped_lock lock{mutex};
    const auto found = cache.find(seed);
    if (found != cache.end()) {
        return found->second;
    }
    // parity: `SpikeConfiguration` shuffles the ten sizes seeded from the world seed's first legacy draw.
    random::RandomSource keySource = random::RandomSource::legacy(seed);
    const std::int64_t key = keySource.nextLong() & 65535LL;
    std::vector<std::int32_t> sizes(kCount);
    for (std::int32_t index = 0; index < kCount; ++index) {
        sizes[static_cast<std::size_t>(index)] = index;
    }
    random::RandomSource random = random::RandomSource::legacy(key);
    for (std::int32_t size = kCount; size >= 2; --size) {
        const std::int32_t swapTo = random.nextInt(size);
        std::swap(sizes[static_cast<std::size_t>(size - 1)], sizes[static_cast<std::size_t>(swapTo)]);
    }
    std::vector<EndSpike> spikes;
    spikes.reserve(kCount);
    for (std::int32_t index = 0; index < kCount; ++index) {
        const double angle = 2.0 * (-std::numbers::pi + std::numbers::pi / kCount * index);
        const std::int32_t size = sizes[static_cast<std::size_t>(index)];
        spikes.push_back(EndSpike{.centerX = static_cast<std::int32_t>(std::floor(kSpikeDistance * std::cos(angle))),
                                  .centerZ = static_cast<std::int32_t>(std::floor(kSpikeDistance * std::sin(angle))),
                                  .radius = 2 + size / 3,
                                  .height = 76 + size * 3,
                                  .guarded = size == 1 || size == 2});
    }
    return cache.emplace(seed, std::move(spikes)).first->second;
}

void placeBlocks(const EndSpike& spike, std::int32_t minY, const SetBlock& set) {
    const registry::BlockState* obsidianBlock = registry::blocks::named("obsidian");
    const std::int32_t obsidian = obsidianBlock == nullptr ? registry::blocks::air().stateId : obsidianBlock->stateId;
    const std::int32_t air = registry::blocks::air().stateId;
    const std::int32_t radiusSquared = (spike.radius * spike.radius) + 1;
    for (std::int32_t z = spike.centerZ - spike.radius; z <= spike.centerZ + spike.radius; ++z) {
        for (std::int32_t x = spike.centerX - spike.radius; x <= spike.centerX + spike.radius; ++x) {
            const std::int32_t dx = x - spike.centerX;
            const std::int32_t dz = z - spike.centerZ;
            const bool insidePillar = (dx * dx) + (dz * dz) <= radiusSquared;
            for (std::int32_t y = minY; y <= spike.height + 10; ++y) {
                if (insidePillar && y < spike.height) {
                    set(x, y, z, obsidian);
                } else if (y > 65) {
                    set(x, y, z, air);
                }
            }
        }
    }
    if (!spike.guarded) {
        return;
    }
    for (std::int32_t dx = -2; dx <= 2; ++dx) {
        for (std::int32_t dz = -2; dz <= 2; ++dz) {
            for (std::int32_t dy = 0; dy <= 3; ++dy) {
                const bool isXSide = std::abs(dx) == 2;
                const bool isZSide = std::abs(dz) == 2;
                const bool top = dy == 3;
                if (!isXSide && !isZSide && !top) {
                    continue;
                }
                const bool xEdge = dx == -2 || dx == 2 || top;
                const bool zEdge = dz == -2 || dz == 2 || top;
                const std::int32_t stateId = registry::blockStateFamilies::ironBarsStateId(
                    xEdge && dz != -2, zEdge && dx != 2, xEdge && dz != 2, zEdge && dx != -2);
                set(spike.centerX + dx, spike.height + dy, spike.centerZ + dz, stateId);
            }
        }
    }
}

} // namespace basalt::server::world::terrain::decoration::end::endSpikes
