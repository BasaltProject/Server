#pragma once

#include <basalt/server/world/structure/StructureTemplatePool.hpp>

#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::world::structure {

/// parity: `JigsawJunction`.
struct PlacedJigsawJunction {
    std::int32_t sourceX = 0;
    std::int32_t sourceGroundY = 0;
    std::int32_t sourceZ = 0;
    std::int32_t deltaY = 0;
    StructureProjection destinationProjection = StructureProjection::Rigid;

    friend bool operator==(const PlacedJigsawJunction&, const PlacedJigsawJunction&) = default;
};

struct PlacedStructurePiece {
    StructurePoolElement element;
    StructurePos position;
    StructureRotation rotation = StructureRotation::None;
    StructureBoundingBox boundingBox;
    std::int32_t groundLevelDelta = 1;
    std::vector<PlacedJigsawJunction> junctions;
};

struct JigsawWorld {
    std::function<const StructureTemplatePool*(std::string_view)> poolLookup;
    PoolTemplateLoader templateLoader;
    /// parity: `WORLD_SURFACE_WG`, the first free height over a column.
    std::function<std::int32_t(std::int32_t, std::int32_t)> heightAt;
};

struct JigsawRequest {
    std::string startPool;
    StructurePos startPosition;
    std::int32_t maxDepth = 0;
    bool projectStartToHeightmap = false;
    std::int32_t maxDistanceFromCenter = 0;
    std::int32_t maxVerticalDistanceFromCenter = std::numeric_limits<std::int32_t>::max();
    /// parity: `JigsawStructure`'s `dimension_padding`.
    std::int32_t dimensionPaddingBottom = 0;
    std::int32_t dimensionPaddingTop = 0;
    std::int32_t worldMinY = std::numeric_limits<std::int32_t>::min();
    std::int32_t worldMaxY = std::numeric_limits<std::int32_t>::max();
    bool useExpansionHack = false;
    std::string startJigsawName;
    /// parity: `PoolAliasLookup`.
    std::map<std::string, std::string, std::less<>> poolAliases;
    /// parity: `Structure.isValidBiome`.
    std::function<bool(const StructurePos&)> acceptStart;
};

/// parity: `JigsawPlacement`.
[[nodiscard]] std::vector<PlacedStructurePiece> assembleJigsaw(const JigsawWorld& world, const JigsawRequest& request,
                                                               random::RandomSource& random);

} // namespace basalt::server::world::structure
