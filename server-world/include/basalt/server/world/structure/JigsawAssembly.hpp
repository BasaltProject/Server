#pragma once

#include <basalt/server/world/structure/StructureTemplatePool.hpp>

#include <cstdint>
#include <functional>
#include <limits>
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

/// A single assembled piece: which element, where, and how it is rotated.
struct PlacedStructurePiece {
    StructurePoolElement element;
    StructurePos position;
    StructureRotation rotation = StructureRotation::None;
    StructureBoundingBox boundingBox;
    std::int32_t groundLevelDelta = 1;
    std::vector<PlacedJigsawJunction> junctions;
};

/// What one assembly needs from the world around it.
struct JigsawWorld {
    /// A pool by its registry name, or null when the datapack does not have it.
    std::function<const StructureTemplatePool*(std::string_view)> poolLookup;
    PoolTemplateLoader templateLoader;
    /// parity: `WORLD_SURFACE_WG`, the first free height over a column.
    std::function<std::int32_t(std::int32_t, std::int32_t)> heightAt;
};

/// What one assembly is asked to build.
struct JigsawRequest {
    std::string startPool;
    StructurePos startPosition;
    std::int32_t maxDepth = 0;
    bool projectStartToHeightmap = false;
    /// Checked before collision, as vanilla does: without it an assembly sprawls to `maxDepth` and
    /// hangs the generator.
    std::int32_t maxDistanceFromCenter = 0;
    std::int32_t maxVerticalDistanceFromCenter = std::numeric_limits<std::int32_t>::max();
    /// Reproduces the legacy village-only collision-box height expansion. Bastions disable it.
    bool useExpansionHack = false;
    /// The jigsaw block the start piece is anchored by, such as the ancient city's `city_anchor`.
    std::string startJigsawName;
};

/// parity: `JigsawPlacement`, the breadth-first assembly of `Placer.tryPlacingChildren`, including
/// jigsaw orientation, joint types and both priorities.
///
/// Free space is inclusive integer boxes minus the placed ones, which is what vanilla's AABB
/// subtraction amounts to for the only query it makes.
[[nodiscard]] std::vector<PlacedStructurePiece> assembleJigsaw(const JigsawWorld& world, const JigsawRequest& request,
                                                               random::RandomSource& random);

} // namespace basalt::server::world::structure
