#include <basalt/server/registry/BlockStateFamilies.hpp>
#include <basalt/server/world/terrain/decoration/tree/LeafDistances.hpp>
#include <basalt/server/world/terrain/decoration/tree/TreeBlockRules.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <deque>
#include <optional>
#include <vector>

namespace basalt::server::world::terrain::decoration::tree {
namespace {

namespace registry = basalt::server::registry;

constexpr std::int32_t kMaxLeafDistance = 7;
constexpr std::int32_t kMargin = kMaxLeafDistance - 1;
constexpr std::int32_t kSpan = 16 + (2 * kMargin);

constexpr std::array<std::array<std::int32_t, 3>, 6> kSides{
    {{0, -1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}, {-1, 0, 0}, {1, 0, 0}}};

} // namespace

void leafDistances::settle(Chunk& chunk, const BlockAt& blockAt) {
    const ChunkHeight bounds = chunk.verticalBounds();
    // Only the sections that hold a leaf or a log are worth walking: a chunk keeps two dozen and a
    // forest touches two of them, so the whole column would be twenty times the work for nothing.
    std::int32_t lowSection = -1;
    std::int32_t highSection = -1;
    for (std::size_t index = 0; index < chunk.sectionCount(); ++index) {
        const bool interesting = chunk.section(index).mayContain(
            [](std::int32_t stateId) { return registry::blockStateFamilies::isLeavesState(stateId); });
        if (!interesting) {
            continue;
        }
        if (lowSection < 0) {
            lowSection = static_cast<std::int32_t>(index);
        }
        highSection = static_cast<std::int32_t>(index);
    }
    if (lowSection < 0) {
        return;
    }
    // A leaf short of seven has a log within six steps, so six blocks round the chunk hold every path
    // that decides one of its leaves. Measured from the logs alone and never from what a neighbour's
    // leaf already says, the answer depends on the blocks and not on which chunk arrived first.
    const std::int32_t firstY = std::max(bounds.minY(), bounds.minY() + lowSection * 16 - kMargin);
    const std::int32_t lastY = std::min(bounds.maxY(), bounds.minY() + highSection * 16 + 15 + kMargin);
    const std::int32_t height = lastY - firstY + 1;
    const std::int32_t baseX = chunk.chunkX() * 16;
    const std::int32_t baseZ = chunk.chunkZ() * 16;
    const auto index = [height](std::int32_t x, std::int32_t y, std::int32_t z) {
        return ((x + kMargin) * kSpan + (z + kMargin)) * height + y;
    };

    std::vector<std::uint8_t> kind(static_cast<std::size_t>(kSpan) * kSpan * static_cast<std::size_t>(height), 0);
    bool anyLeaf = false;
    for (std::int32_t x = -kMargin; x < 16 + kMargin; ++x) {
        for (std::int32_t z = -kMargin; z < 16 + kMargin; ++z) {
            const bool inside = x >= 0 && x < 16 && z >= 0 && z < 16;
            for (std::int32_t y = 0; y < height; ++y) {
                const std::int32_t worldY = firstY + y;
                const std::int32_t stateId =
                    inside ? chunk.blockStateId(x, worldY, z) : blockAt(baseX + x, worldY, baseZ + z);
                if (registry::blockStateFamilies::isLeavesState(stateId)) {
                    kind[static_cast<std::size_t>(index(x, y, z))] = 1;
                    anyLeaf = anyLeaf || inside;
                } else if (treeBlockRules::isLogState(stateId)) {
                    kind[static_cast<std::size_t>(index(x, y, z))] = 2;
                }
            }
        }
    }
    if (!anyLeaf) {
        return;
    }

    std::vector<std::uint8_t> reached(kind.size(), kMaxLeafDistance);
    std::deque<std::array<std::int32_t, 3>> pending;
    for (std::int32_t x = -kMargin; x < 16 + kMargin; ++x) {
        for (std::int32_t z = -kMargin; z < 16 + kMargin; ++z) {
            for (std::int32_t y = 0; y < height; ++y) {
                const auto slot = static_cast<std::size_t>(index(x, y, z));
                if (kind[slot] == 2) {
                    reached[slot] = 0;
                    pending.push_back({x, y, z});
                }
            }
        }
    }
    while (!pending.empty()) {
        const std::array<std::int32_t, 3> cell = pending.front();
        pending.pop_front();
        const std::int32_t so = reached[static_cast<std::size_t>(index(cell[0], cell[1], cell[2]))];
        if (so >= kMaxLeafDistance - 1) {
            continue;
        }
        for (const std::array<std::int32_t, 3>& side : kSides) {
            const std::int32_t nx = cell[0] + side[0];
            const std::int32_t ny = cell[1] + side[1];
            const std::int32_t nz = cell[2] + side[2];
            if (nx < -kMargin || nx >= 16 + kMargin || nz < -kMargin || nz >= 16 + kMargin || ny < 0 || ny >= height) {
                continue;
            }
            const auto slot = static_cast<std::size_t>(index(nx, ny, nz));
            if (kind[slot] != 1 || reached[slot] <= so + 1) {
                continue;
            }
            reached[slot] = static_cast<std::uint8_t>(so + 1);
            pending.push_back({nx, ny, nz});
        }
    }

    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t y = 0; y < height; ++y) {
                const auto slot = static_cast<std::size_t>(index(x, y, z));
                if (kind[slot] != 1) {
                    continue;
                }
                const std::int32_t worldY = firstY + y;
                const std::int32_t stateId = chunk.blockStateId(x, worldY, z);
                if (registry::blockStateFamilies::leafDistance(stateId) == std::int32_t{reached[slot]}) {
                    continue;
                }
                if (const std::optional<std::int32_t> settled =
                        registry::blockStateFamilies::withLeafDistance(stateId, reached[slot]);
                    settled.has_value()) {
                    chunk.setBlockStateId(x, worldY, z, *settled);
                }
            }
        }
    }
}

} // namespace basalt::server::world::terrain::decoration::tree
