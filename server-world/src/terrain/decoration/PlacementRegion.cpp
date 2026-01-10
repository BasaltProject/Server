#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/OreBlockRules.hpp>
#include <basalt/server/world/terrain/decoration/PlacementRegion.hpp>

#include <algorithm>

namespace basalt::server::world::terrain::decoration {

std::optional<std::int32_t> PlacementRegion::oceanFloorHeight(std::int32_t x, std::int32_t z) const {
    if ((x >> 4) == chunk_->chunkX() && (z >> 4) == chunk_->chunkZ()) {
        return chunk_->worldgenHeightAt(x & 15, z & 15, HeightmapType::OceanFloor);
    }
    if (externalSurfaceHeight_) {
        return externalSurfaceHeight_(x, z);
    }
    return std::nullopt;
}

bool PlacementRegion::hasOceanFloorAtOrAbove(std::int32_t xStart, std::int32_t zStart, std::int32_t sizeXZ,
                                             std::int32_t yStart) const {
    const std::int32_t chunkMinX = chunk_->chunkX() * 16;
    const std::int32_t chunkMinZ = chunk_->chunkZ() * 16;
    const std::int32_t localMinX = std::max(xStart, chunkMinX);
    const std::int32_t localMaxX = std::min(xStart + sizeXZ, chunkMinX + 15);
    const std::int32_t localMinZ = std::max(zStart, chunkMinZ);
    const std::int32_t localMaxZ = std::min(zStart + sizeXZ, chunkMinZ + 15);
    for (std::int32_t x = localMinX; x <= localMaxX; ++x) {
        for (std::int32_t z = localMinZ; z <= localMaxZ; ++z) {
            if (yStart <= chunk_->worldgenHeightAt(x & 15, z & 15, HeightmapType::OceanFloor)) {
                return true;
            }
        }
    }
    if (!externalSurfaceHeight_) {
        return false;
    }
    for (std::int32_t x = xStart; x <= xStart + sizeXZ; ++x) {
        for (std::int32_t z = zStart; z <= zStart + sizeXZ; ++z) {
            if (x >= chunkMinX && x <= chunkMinX + 15 && z >= chunkMinZ && z <= chunkMinZ + 15) {
                continue;
            }
            if (const std::optional<std::int32_t> height = externalSurfaceHeight_(x, z);
                height.has_value() && yStart <= *height) {
                return true;
            }
        }
    }
    return false;
}

std::optional<std::int32_t> PlacementRegion::blockStateId(std::int32_t x, std::int32_t y, std::int32_t z) const {
    const ChunkHeight bounds = chunk_->verticalBounds();
    if (y < bounds.minY() || y > bounds.maxY()) {
        return registry::blocks::air().stateId;
    }
    if ((x >> 4) == chunk_->chunkX() && (z >> 4) == chunk_->chunkZ()) {
        return chunk_->blockStateId(x & 15, y, z & 15);
    }
    const std::array<std::int32_t, 3> key = {x, y, z};
    for (const auto& [position, stateId] : externalWrites_) {
        if (position == key) {
            return stateId;
        }
    }
    // parity: a feature of an earlier step may already have queued a write here, and vanilla would
    // be reading the block it put down. Its own buffer is not this one: a geode keeps its writes
    // elsewhere, and only the chunk holds every step's.
    if (const std::optional<std::int32_t> queued = chunk_->queuedDecorationStateId(x, y, z); queued.has_value()) {
        return *queued;
    }
    if (externalBlockState_) {
        return externalBlockState_(x, y, z);
    }
    return std::nullopt;
}

void PlacementRegion::setBlockStateId(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId,
                                      const std::set<std::string, std::less<>>* replaceable) {
    if ((x >> 4) == chunk_->chunkX() && (z >> 4) == chunk_->chunkZ()) {
        chunk_->setBlockStateId(x & 15, y, z & 15, stateId);
        return;
    }

    const std::array<std::int32_t, 3> key = {x, y, z};
    std::int32_t observed = registry::blocks::air().stateId;
    bool known = false;
    for (auto& [position, value] : externalWrites_) {
        if (position == key) {
            observed = value;
            value = stateId;
            known = true;
            break;
        }
    }
    if (!known) {
        if (externalBlockState_) {
            if (const std::optional<std::int32_t> outside = externalBlockState_(x, y, z); outside.has_value()) {
                observed = *outside;
            }
        }
        externalWrites_.emplace_back(key, stateId);
    }

    const GeneratedBlockWritePolicy policy =
        replaceable != nullptr ? oreBlockRules::policyFor(*replaceable) : GeneratedBlockWritePolicy::Exact;
    const Chunk::WritePolicyScope scope{*chunk_, policy};
    chunk_->setDecorationBlockStateId(x, y, z, stateId, observed);
}

} // namespace basalt::server::world::terrain::decoration
