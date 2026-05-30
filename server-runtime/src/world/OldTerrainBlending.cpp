#include <basalt/server/runtime/world/OldTerrainBlending.hpp>
#include <basalt/server/world/Chunk.hpp>

#include <vector>

namespace basalt::server::runtime::world {
namespace {

using server::world::blending::Blender;
using server::world::blending::BlendingData;
using server::world::blending::Side;
using server::world::blending::SideSet;

constexpr std::int32_t kHeightBlendingRangeChunks = 7;
constexpr std::int32_t kDensityBlendingRangeChunks = 1;

[[nodiscard]] std::int64_t keyOf(std::int32_t chunkX, std::int32_t chunkZ) {
    return (static_cast<std::int64_t>(chunkX) & 0xFFFFFFFFLL) | (static_cast<std::int64_t>(chunkZ) << 32);
}

constexpr std::array<Side, server::world::blending::kSideCount> kSides{
    Side::North, Side::NorthEast, Side::East, Side::SouthEast,
    Side::South, Side::SouthWest, Side::West, Side::NorthWest};

} // namespace

OldTerrainBlending::Entry& OldTerrainBlending::classify(std::int32_t chunkX, std::int32_t chunkZ) {
    Entry& entry = entries_[keyOf(chunkX, chunkZ)];
    if (entry.classified) {
        return entry;
    }
    entry.classified = true;
    const std::int64_t key = keyOf(chunkX, chunkZ);
    auto age = ages_.find(key);
    if (age == ages_.end()) {
        age = ages_.emplace(key, regions_->ageOf(chunkX, chunkZ)).first;
    }
    if (age->second != RegionStore::Age::MaybeOld) {
        return entry;
    }
    if (std::optional<server::world::Chunk> chunk = regions_->loadChunk(chunkX, chunkZ); chunk.has_value()) {
        entry.data = chunk->blendingData();
    }
    return entry;
}

std::shared_ptr<BlendingData> OldTerrainBlending::calculated(std::int32_t chunkX, std::int32_t chunkZ) {
    Entry& entry = classify(chunkX, chunkZ);
    if (entry.data == nullptr) {
        return nullptr;
    }
    if (!entry.data->hasCalculated()) {
        SideSet newSides{};
        for (const Side side : kSides) {
            newSides[static_cast<std::size_t>(side)] = classify(chunkX + server::world::blending::sideStepX(side),
                                                                chunkZ + server::world::blending::sideStepZ(side))
                                                           .data == nullptr;
        }
        if (std::optional<server::world::Chunk> chunk = regions_->loadChunk(chunkX, chunkZ); chunk.has_value()) {
            // The data of the copy just read is the same object's twin; only its columns are wanted.
            entry.data->calculate(*chunk, newSides);
        }
    }
    return entry.data;
}

std::shared_ptr<const Blender> OldTerrainBlending::blenderFor(std::int32_t chunkX, std::int32_t chunkZ) {
    const std::lock_guard lock{mutex_};
    // Verdicts for chunks that are not old are cheap to recompute, so they do not pile up as the world grows.
    constexpr std::size_t kMaxVerdicts = 262144;
    if (ages_.size() > kMaxVerdicts) {
        ages_.clear();
        std::erase_if(entries_, [](const auto& entry) { return entry.second.data == nullptr; });
    }
    bool anyOld = false;
    for (std::int32_t dx = -kHeightBlendingRangeChunks; dx <= kHeightBlendingRangeChunks && !anyOld; ++dx) {
        for (std::int32_t dz = -kHeightBlendingRangeChunks; dz <= kHeightBlendingRangeChunks; ++dz) {
            if (classify(chunkX + dx, chunkZ + dz).data != nullptr) {
                anyOld = true;
                break;
            }
        }
    }
    if (!anyOld) {
        return nullptr;
    }
    std::vector<Blender::Source> heightAndBiome;
    std::vector<Blender::Source> density;
    constexpr std::int32_t maxDistanceSquared = (kHeightBlendingRangeChunks + 1) * (kHeightBlendingRangeChunks + 1);
    for (std::int32_t dx = -kHeightBlendingRangeChunks; dx <= kHeightBlendingRangeChunks; ++dx) {
        for (std::int32_t dz = -kHeightBlendingRangeChunks; dz <= kHeightBlendingRangeChunks; ++dz) {
            if (dx * dx + dz * dz > maxDistanceSquared) {
                continue;
            }
            const std::shared_ptr<BlendingData> data = calculated(chunkX + dx, chunkZ + dz);
            if (data == nullptr) {
                continue;
            }
            heightAndBiome.push_back({chunkX + dx, chunkZ + dz, data});
            if (dx >= -kDensityBlendingRangeChunks && dx <= kDensityBlendingRangeChunks &&
                dz >= -kDensityBlendingRangeChunks && dz <= kDensityBlendingRangeChunks) {
                density.push_back({chunkX + dx, chunkZ + dz, data});
            }
        }
    }
    std::vector<Blender::OldSide> sides;
    if (const auto& center = classify(chunkX, chunkZ).data; center != nullptr) {
        sides.push_back({std::nullopt, center});
    }
    for (const Side side : kSides) {
        if (const auto& neighbour = classify(chunkX + server::world::blending::sideStepX(side),
                                             chunkZ + server::world::blending::sideStepZ(side))
                                        .data;
            neighbour != nullptr) {
            sides.push_back({side, neighbour});
        }
    }
    Blender::CarvingFilter filter =
        sides.empty() ? Blender::CarvingFilter{} : Blender::makeCarvingFilter(std::move(sides));
    auto blender = std::make_shared<const Blender>(std::move(heightAndBiome), std::move(density), std::move(filter));
    return blender->isEmpty() ? nullptr : blender;
}

} // namespace basalt::server::runtime::world
