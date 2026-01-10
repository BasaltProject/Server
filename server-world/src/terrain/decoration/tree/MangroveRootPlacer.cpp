#include <basalt/server/registry/BlockFluids.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/tree/MangroveRootPlacer.hpp>
#include <basalt/server/world/terrain/decoration/tree/TreeHelpers.hpp>

#include <cstdint>
#include <cstdlib>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>

namespace basalt::server::world::terrain::decoration::tree {
namespace {

// parity: MangroveRootPlacement(8, 15, 0.2) and AboveRootPlacement(moss_carpet, 0.5), the exact
// values both mangrove tree configs use.
constexpr std::int32_t kMaxRootWidth = 8;
constexpr std::size_t kMaxRootLength = 15;
constexpr float kRandomSkewChance = 0.2F;
constexpr float kAboveRootChance = 0.5F;

using PathSet = std::set<std::string, std::less<>>;

[[nodiscard]] std::int32_t stateIdOf(std::string_view path) {
    const registry::BlockState* found = registry::blocks::named(path);
    if (found == nullptr) {
        throw std::runtime_error{"missing vanilla block minecraft:" + std::string{path}};
    }
    return found->stateId;
}

[[nodiscard]] std::string pathOf(const std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block == nullptr ? std::string{} : block->path();
}

/// parity: block tag `mangrove_roots_can_grow_through`.
const PathSet& canGrowThroughPaths() {
    static const PathSet paths = {
        "mud", "muddy_mangrove_roots", "mangrove_roots", "moss_carpet", "vine", "mangrove_propagule", "snow"};
    return paths;
}

const PathSet& muddyRootsInPaths() {
    static const PathSet paths = {"mud", "muddy_mangrove_roots"};
    return paths;
}

[[nodiscard]] bool canPlaceRoot(const Chunk& chunk, const std::int32_t x, const std::int32_t y, const std::int32_t z) {
    const ChunkHeight bounds = chunk.verticalBounds();
    if (y < bounds.minY() || y > bounds.maxY()) {
        return false;
    }
    return isValidTreePos(chunk, x, y, z) ||
           canGrowThroughPaths().contains(pathOf(chunk.decorationBlockStateId(x, y, z)));
}

[[nodiscard]] std::int32_t potentiallyWaterlogged(const std::int32_t stateId, const std::int32_t replacedStateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    if (block == nullptr) {
        return stateId;
    }
    std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    if (!properties.contains("waterlogged")) {
        return stateId;
    }
    properties["waterlogged"] = registry::blockFluids::hasWaterFluid(replacedStateId) ? "true" : "false";
    return registry::blocks::stateIdOf(*block, properties).value_or(stateId);
}

void placeRoot(Chunk& chunk, random::RandomSource& random, const std::int32_t x, const std::int32_t y,
               const std::int32_t z) {
    const ChunkHeight bounds = chunk.verticalBounds();
    if (y < bounds.minY() || y > bounds.maxY()) {
        return;
    }
    const std::int32_t current = chunk.decorationBlockStateId(x, y, z);
    // parity: MangroveRootPlacer.placeRoot turns mud and muddy roots into muddy_mangrove_roots
    // whatever canPlaceRoot says; everything else goes through the real RootPlacer.placeRoot.
    if (muddyRootsInPaths().contains(pathOf(current))) {
        chunk.setDecorationBlockStateId(x, y, z, potentiallyWaterlogged(stateIdOf("muddy_mangrove_roots"), current));
        return;
    }
    if (!canPlaceRoot(chunk, x, y, z)) {
        return;
    }
    chunk.setDecorationBlockStateId(x, y, z, potentiallyWaterlogged(stateIdOf("mangrove_roots"), current));
    if (random.nextFloat() < kAboveRootChance && y + 1 <= bounds.maxY() &&
        registry::blocks::isAir(chunk.decorationBlockStateId(x, y + 1, z))) {
        chunk.setDecorationBlockStateId(x, y + 1, z, stateIdOf("moss_carpet"));
    }
}

/// parity: `MangroveRootPlacer.potentialRootPositions`. The draws differ per branch, so the width
/// bands are not interchangeable.
[[nodiscard]] std::vector<std::tuple<std::int32_t, std::int32_t, std::int32_t>>
potentialRootPositions(random::RandomSource& random, const std::int32_t x, const std::int32_t y, const std::int32_t z,
                       const HorizontalDirection dir, const std::int32_t originX, const std::int32_t originY,
                       const std::int32_t originZ) {
    const std::tuple<std::int32_t, std::int32_t, std::int32_t> below{x, y - 1, z};
    const std::tuple<std::int32_t, std::int32_t, std::int32_t> nextTo{x + dir.dx, y, z + dir.dz};
    const std::int32_t width = std::abs(x - originX) + std::abs(y - originY) + std::abs(z - originZ);
    if (width > kMaxRootWidth - 3 && width <= kMaxRootWidth) {
        if (random.nextFloat() < kRandomSkewChance) {
            return {below, {std::get<0>(nextTo), std::get<1>(nextTo) - 1, std::get<2>(nextTo)}};
        }
        return {below};
    }
    if (width > kMaxRootWidth) {
        return {below};
    }
    if (random.nextFloat() < kRandomSkewChance) {
        return {below};
    }
    return random.nextBoolean() ? std::vector{nextTo} : std::vector{below};
}

} // namespace

std::int32_t MangroveRootPlacer::trunkOriginY(random::RandomSource& random, const std::int32_t originY) const {
    return originY + trunkOffsetY_.sample(random);
}

bool MangroveRootPlacer::simulateRoots(const Chunk& chunk, random::RandomSource& random, const std::int32_t rootX,
                                       const std::int32_t rootY, const std::int32_t rootZ,
                                       const HorizontalDirection dir, const std::int32_t originX,
                                       const std::int32_t originY, const std::int32_t originZ,
                                       std::vector<RootPos>& rootPositions, const std::int32_t layer) const {
    if (layer == static_cast<std::int32_t>(kMaxRootLength) || rootPositions.size() > kMaxRootLength) {
        return false;
    }
    for (const RootPos& pos : potentialRootPositions(random, rootX, rootY, rootZ, dir, originX, originY, originZ)) {
        const auto [x, y, z] = pos;
        if (!canPlaceRoot(chunk, x, y, z)) {
            continue;
        }
        rootPositions.push_back(pos);
        if (!simulateRoots(chunk, random, x, y, z, dir, originX, originY, originZ, rootPositions, layer + 1)) {
            return false;
        }
    }
    return true;
}

bool MangroveRootPlacer::placeRoots(Chunk& chunk, random::RandomSource& random, const std::int32_t originX,
                                    const std::int32_t originY, const std::int32_t originZ,
                                    const std::int32_t trunkOriginY) const {
    for (std::int32_t columnY = originY; columnY < trunkOriginY; ++columnY) {
        if (!canPlaceRoot(chunk, originX, columnY, originZ)) {
            return false;
        }
    }
    std::vector<RootPos> rootPositions;
    rootPositions.emplace_back(originX, trunkOriginY - 1, originZ);
    for (const HorizontalDirection dir : kHorizontalDirections) {
        const std::int32_t posX = originX + dir.dx;
        const std::int32_t posZ = originZ + dir.dz;
        std::vector<RootPos> positionsInDirection;
        if (!simulateRoots(chunk, random, posX, trunkOriginY, posZ, dir, originX, trunkOriginY, originZ,
                           positionsInDirection, 0)) {
            return false;
        }
        rootPositions.insert(rootPositions.end(), positionsInDirection.begin(), positionsInDirection.end());
        rootPositions.emplace_back(posX, trunkOriginY, posZ);
    }
    for (const auto& [x, y, z] : rootPositions) {
        placeRoot(chunk, random, x, y, z);
    }
    return true;
}

} // namespace basalt::server::world::terrain::decoration::tree
