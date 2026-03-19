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
constexpr std::int32_t kSpan = 18;

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
        const bool interesting = chunk.section(index).mayContain([](std::int32_t stateId) {
            return registry::blockStateFamilies::isLeavesState(stateId) || treeBlockRules::isLogState(stateId);
        });
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
    const std::int32_t firstY = std::max(bounds.minY(), bounds.minY() + lowSection * 16 - 1);
    const std::int32_t lastY = std::min(bounds.maxY(), bounds.minY() + highSection * 16 + 16);
    const std::int32_t height = lastY - firstY + 1;
    const std::int32_t baseX = chunk.chunkX() * 16;
    const std::int32_t baseZ = chunk.chunkZ() * 16;
    const auto index = [height](std::int32_t x, std::int32_t y, std::int32_t z) {
        return ((x + 1) * kSpan + (z + 1)) * height + y;
    };

    // Only leaves and logs matter, and only they are kept: a full copy of the column would be a
    // megabyte per chunk on the install thread.
    std::vector<std::uint8_t> kind(static_cast<std::size_t>(kSpan) * kSpan * static_cast<std::size_t>(height), 0);
    // parity: LeavesBlock.updateDistanceAt relaxes against whatever the neighbour already says, so a
    // leaf in the ring carries the reach of a trunk that stands further out than this window sees.
    std::vector<std::uint8_t> outside(kind.size(), kMaxLeafDistance);
    bool anyLeaf = false;
    for (std::int32_t x = -1; x <= 16; ++x) {
        for (std::int32_t z = -1; z <= 16; ++z) {
            const bool inside = x >= 0 && x < 16 && z >= 0 && z < 16;
            for (std::int32_t y = 0; y < height; ++y) {
                const std::int32_t worldY = firstY + y;
                const std::int32_t stateId =
                    inside ? chunk.blockStateId(x, worldY, z) : blockAt(baseX + x, worldY, baseZ + z);
                if (registry::blockStateFamilies::isLeavesState(stateId)) {
                    kind[static_cast<std::size_t>(index(x, y, z))] = 1;
                    anyLeaf = true;
                    if (const std::optional<std::int32_t> carried = registry::blockStateFamilies::leafDistance(stateId);
                        !inside && carried.has_value()) {
                        outside[static_cast<std::size_t>(index(x, y, z))] = static_cast<std::uint8_t>(*carried);
                    }
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
    for (std::int32_t x = -1; x <= 16; ++x) {
        for (std::int32_t z = -1; z <= 16; ++z) {
            for (std::int32_t y = 0; y < height; ++y) {
                const auto slot = static_cast<std::size_t>(index(x, y, z));
                if (kind[slot] == 2) {
                    reached[slot] = 0;
                    pending.push_back({x, y, z});
                } else if (kind[slot] == 1 && outside[slot] < kMaxLeafDistance) {
                    reached[slot] = outside[slot];
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
            if (nx < -1 || nx > 16 || nz < -1 || nz > 16 || ny < 0 || ny >= height) {
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
