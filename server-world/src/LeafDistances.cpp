#include <basalt/server/registry/BlockConnections.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/world/LeafDistances.hpp>

#include <algorithm>
#include <array>
#include <limits>
#include <map>
#include <string>
#include <vector>

namespace basalt::server::world {
namespace {

/// parity: `LeavesBlock.DECAY_DISTANCE`.
constexpr std::int32_t kDecayDistance = 7;
constexpr std::int32_t kNotLeaf = -1;

constexpr std::array<std::int32_t, 6> kFaceX{0, 0, 0, 0, -1, 1};
constexpr std::array<std::int32_t, 6> kFaceY{-1, 1, 0, 0, 0, 0};
constexpr std::array<std::int32_t, 6> kFaceZ{0, 0, -1, 1, 0, 0};

using DistanceStates = std::array<std::int32_t, kDecayDistance + 1>;

[[nodiscard]] DistanceStates withEveryDistance(std::int32_t stateId) {
    DistanceStates states{};
    states.fill(stateId);
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    if (block == nullptr) {
        return states;
    }
    for (std::int32_t distance = 1; distance <= kDecayDistance; ++distance) {
        states[static_cast<std::size_t>(distance)] =
            registry::blockStateDefinitions::stateIdOf(*block, {{"distance", std::to_string(distance)}}, stateId)
                .value_or(stateId);
    }
    return states;
}

} // namespace

std::int32_t leafDistances::assign(Chunk& chunk) {
    const ChunkHeight bounds = chunk.verticalBounds();
    std::int32_t lowest = std::numeric_limits<std::int32_t>::max();
    std::int32_t highest = std::numeric_limits<std::int32_t>::min();
    for (std::size_t index = 0; index < chunk.sectionCount(); ++index) {
        if (!chunk.section(index).mayContain(
                [](std::int32_t stateId) { return registry::blockConnections::isLeaves(stateId); })) {
            continue;
        }
        const auto section = static_cast<std::int32_t>(index);
        lowest = std::min(lowest, section);
        highest = std::max(highest, section);
    }
    if (lowest > highest) {
        return 0;
    }

    // One section either way: the trunk a canopy hangs from can sit below the lowest leaf.
    const std::int32_t fromSection = std::max(0, lowest - 1);
    const std::int32_t toSection = std::min(static_cast<std::int32_t>(chunk.sectionCount()) - 1, highest + 1);
    const std::int32_t baseY = bounds.minY() + fromSection * 16;
    const std::int32_t height = (toSection - fromSection + 1) * 16;

    std::vector<std::int32_t> cells(static_cast<std::size_t>(height) << 8U, kNotLeaf);
    std::vector<std::int32_t> sources;
    for (std::int32_t y = 0; y < height; ++y) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t x = 0; x < 16; ++x) {
                const std::int32_t stateId = chunk.blockStateId(x, baseY + y, z);
                const auto cell = static_cast<std::size_t>((y << 8) | (z << 4) | x);
                if (registry::blockConnections::preventsLeafDecay(stateId)) {
                    cells[cell] = 0;
                    sources.push_back(static_cast<std::int32_t>(cell));
                } else if (registry::blockConnections::isLeaves(stateId)) {
                    cells[cell] = kDecayDistance;
                }
            }
        }
    }
    if (sources.empty()) {
        return 0;
    }

    std::vector<std::int32_t> current = std::move(sources);
    std::vector<std::int32_t> next;
    for (std::int32_t distance = 1; distance < kDecayDistance; ++distance) {
        next.clear();
        for (const std::int32_t cell : current) {
            const std::int32_t y = cell >> 8;
            const std::int32_t z = (cell >> 4) & 15;
            const std::int32_t x = cell & 15;
            for (std::size_t face = 0; face < kFaceX.size(); ++face) {
                const std::int32_t nx = x + kFaceX[face];
                const std::int32_t ny = y + kFaceY[face];
                const std::int32_t nz = z + kFaceZ[face];
                if (nx < 0 || nx > 15 || nz < 0 || nz > 15 || ny < 0 || ny >= height) {
                    continue;
                }
                const auto neighbour = static_cast<std::size_t>((ny << 8) | (nz << 4) | nx);
                if (cells[neighbour] != kDecayDistance) {
                    continue;
                }
                cells[neighbour] = distance;
                next.push_back(static_cast<std::int32_t>(neighbour));
            }
        }
        if (next.empty()) {
            break;
        }
        current = next;
    }

    // A canopy is a handful of distinct states, so resolving each one once covers the chunk.
    std::map<std::int32_t, DistanceStates> statesByDistance;
    std::int32_t corrected = 0;
    for (std::size_t cell = 0; cell < cells.size(); ++cell) {
        const std::int32_t distance = cells[cell];
        if (distance <= 0) {
            continue;
        }
        const auto index = static_cast<std::int32_t>(cell);
        const std::int32_t y = baseY + (index >> 8);
        const std::int32_t z = (index >> 4) & 15;
        const std::int32_t x = index & 15;
        const std::int32_t stateId = chunk.blockStateId(x, y, z);
        auto found = statesByDistance.find(stateId);
        if (found == statesByDistance.end()) {
            found = statesByDistance.emplace(stateId, withEveryDistance(stateId)).first;
        }
        const std::int32_t wanted = found->second[static_cast<std::size_t>(distance)];
        if (wanted == stateId) {
            continue;
        }
        chunk.setBlockStateId(x, y, z, wanted);
        ++corrected;
    }
    return corrected;
}

} // namespace basalt::server::world
