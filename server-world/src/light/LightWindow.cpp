#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/light/LightWindow.hpp>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <vector>

namespace basalt::server::world::light {
namespace {

constexpr std::size_t kBlocksPerSection = 16 * 16 * 16;
/// What a block that fills its cube takes out of the light.
constexpr std::int32_t kSolid = 15;

} // namespace

LightWindow::LightWindow(const Chunk& centre, const Neighbours& neighbours)
    : minY_(centre.verticalBounds().minY()), maxY_(centre.verticalBounds().maxY()),
      originX_((centre.chunkX() - 1) * 16), originZ_((centre.chunkZ() - 1) * 16), complete_(true) {
    // The nine chunks, looked up once. Per block it was a lookup a few hundred thousand times over,
    // which is the sort of thing that turns a light pass into a stall.
    for (std::size_t index = 0; index < chunks_.size(); ++index) {
        const auto offsetX = static_cast<std::int32_t>(index % 3) - 1;
        const auto offsetZ = static_cast<std::int32_t>(index / 3) - 1;
        chunks_[index] =
            offsetX == 0 && offsetZ == 0 ? &centre : neighbours(centre.chunkX() + offsetX, centre.chunkZ() + offsetZ);
        if (chunks_[index] == nullptr) {
            complete_ = false;
        }
    }
}

const Chunk* LightWindow::chunkAt(std::int32_t offsetX, std::int32_t offsetZ) const {
    return chunks_[static_cast<std::size_t>((offsetZ + 1) * 3 + offsetX + 1)];
}

void LightWindow::fillOpacity(std::span<std::uint8_t> into, std::int32_t bottomY, std::int32_t topY,
                              std::span<const std::int32_t> opacityOf) const {
    std::vector<std::int32_t> scratch(kBlocksPerSection);
    for (std::int32_t offsetZ = -1; offsetZ <= 1; ++offsetZ) {
        for (std::int32_t offsetX = -1; offsetX <= 1; ++offsetX) {
            const Chunk* chunk = chunkAt(offsetX, offsetZ);
            if (chunk == nullptr) {
                continue;
            }
            const std::int32_t baseX = (offsetX + 1) * 16;
            const std::int32_t baseZ = (offsetZ + 1) * 16;
            for (std::size_t index = 0; index < chunk->sectionCount(); ++index) {
                const std::int32_t sectionMinY = minY_ + static_cast<std::int32_t>(index) * 16;
                if (sectionMinY > topY || sectionMinY + 15 < bottomY) {
                    continue;
                }
                const ChunkSection& section = chunk->section(index);
                // Air takes nothing out of the light, and the array starts at zero.
                if (section.nonAirBlockCount() == 0) {
                    continue;
                }
                // Solid rock is most of a column and every block of it answers the same, so it is
                // written straight in rather than unpacked block by block.
                const bool solid = !section.mayContain([&opacityOf](std::int32_t stateId) {
                    return stateId >= 0 && static_cast<std::size_t>(stateId) < opacityOf.size() &&
                           opacityOf[static_cast<std::size_t>(stateId)] < kSolid;
                });
                if (!solid) {
                    section.copyBlockStateIdsInto(scratch);
                }
                for (std::int32_t localY = 0; localY < 16; ++localY) {
                    const std::int32_t y = sectionMinY + localY;
                    if (y < bottomY || y > topY) {
                        continue;
                    }
                    const auto row = static_cast<std::size_t>((y - bottomY) * kWidth * kWidth);
                    for (std::int32_t localZ = 0; localZ < 16; ++localZ) {
                        const std::size_t target = row + static_cast<std::size_t>((baseZ + localZ) * kWidth + baseX);
                        const auto source = static_cast<std::size_t>((localY * 16 + localZ) * 16);
                        if (solid) {
                            std::fill_n(into.begin() + static_cast<std::ptrdiff_t>(target), 16,
                                        static_cast<std::uint8_t>(kSolid));
                        } else {
                            for (std::size_t localX = 0; localX < 16; ++localX) {
                                into[target + localX] = static_cast<std::uint8_t>(
                                    opacityOf[static_cast<std::size_t>(scratch[source + localX])]);
                            }
                        }
                    }
                }
            }
        }
    }
}

std::int32_t LightWindow::stateAt(std::int32_t windowX, std::int32_t y, std::int32_t windowZ) const {
    if (y < minY_ || y > maxY_) {
        return registry::blocks::air().stateId;
    }
    const Chunk* chunk = chunkAt(windowX / 16 - 1, windowZ / 16 - 1);
    if (chunk == nullptr) {
        return registry::blocks::air().stateId;
    }
    return chunk->blockStateId((originX_ + windowX) & 15, y, (originZ_ + windowZ) & 15);
}

std::pair<std::int32_t, std::int32_t> LightWindow::surfaceRange() const {
    std::int32_t lowest = std::numeric_limits<std::int32_t>::max();
    std::int32_t highest = std::numeric_limits<std::int32_t>::min();
    for (const Chunk* chunk : chunks_) {
        if (chunk == nullptr) {
            continue;
        }
        for (const std::int32_t height : chunk->heightmap(HeightmapType::WorldSurface)) {
            lowest = std::min(lowest, height);
            highest = std::max(highest, height);
        }
    }
    if (highest == std::numeric_limits<std::int32_t>::min()) {
        return {minY_, minY_};
    }
    return {lowest, highest};
}

std::optional<std::pair<std::size_t, std::size_t>>
LightWindow::emitterSections(const std::function<std::int32_t(std::int32_t)>& emission) const {
    std::size_t lowest = std::numeric_limits<std::size_t>::max();
    std::size_t highest = 0;
    bool found = false;
    for (const Chunk* chunk : chunks_) {
        if (chunk == nullptr) {
            continue;
        }
        for (std::size_t index = 0; index < chunk->sectionCount(); ++index) {
            if (!chunk->section(index).mayContain(
                    [&emission](std::int32_t stateId) { return emission(stateId) > 0; })) {
                continue;
            }
            lowest = std::min(lowest, index);
            highest = std::max(highest, index);
            found = true;
        }
    }
    if (!found) {
        return std::nullopt;
    }
    return std::pair{lowest, highest};
}

} // namespace basalt::server::world::light
