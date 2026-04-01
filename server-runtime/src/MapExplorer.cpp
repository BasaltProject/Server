#include <basalt/server/registry/Tags.hpp>
#include <basalt/server/runtime/MapSystem.hpp>
#include <basalt/server/runtime/config/WorldsConfig.hpp>
#include <basalt/server/runtime/world/BiomeTagLoader.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/maps/MapStorage.hpp>
#include <basalt/server/world/structure/StructureQueries.hpp>
#include <basalt/server/world/terrain/OverworldBiomeSource.hpp>
#include <basalt/server/world/terrain/VillageStructureGenerator.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <algorithm>
#include <limits>
#include <string>

namespace basalt::server::runtime {
namespace {

using wire::common::Identifier;

struct Found {
    Identifier structure;
    std::int32_t chunkX = 0;
    std::int32_t chunkZ = 0;
};

[[nodiscard]] std::string claimKey(const world::LoadedWorld& world, const Identifier& structure, std::int32_t chunkX,
                                   std::int32_t chunkZ) {
    return world.levelKey() + "|" + structure.toString() + "|" + std::to_string(chunkX) + "|" + std::to_string(chunkZ);
}

} // namespace

bool MapSystem::explore(world::LoadedWorld& world, wire::nbt::NbtCompound& components,
                        const server::world::loot::ExplorationMapRequest& request,
                        const std::array<double, 3>& origin) {
    std::vector<Identifier> wanted;
    for (const std::string& destination : request.destination) {
        if (destination.starts_with('#')) {
            const std::vector<Identifier> members = structuresIn(std::string_view{destination}.substr(1));
            wanted.insert(wanted.end(), members.begin(), members.end());
        } else if (const auto parsed = Identifier::parse(destination); parsed.has_value()) {
            wanted.push_back(*parsed);
        }
    }
    const std::int32_t originX = wire::common::mth::floor(origin[0]);
    const std::int32_t originY = wire::common::mth::floor(origin[1]);
    const std::int32_t originZ = wire::common::mth::floor(origin[2]);
    const std::int64_t seed = world.metadata().seed;
    std::optional<Found> nearest;
    double nearestDistance = std::numeric_limits<double>::max();
    const std::unique_lock<std::mutex> lock = storage_->lock();
    // parity: `ExplorationMapFunction.claimStructureAt`: a structure the loot stands in is claimed
    // first, so a cartographer never sells the map to its own village.
    if (request.skipExistingChunks) {
        const server::world::structure::StructureQueries structures{
            [&world](std::int32_t chunkX, std::int32_t chunkZ) -> const server::world::structure::ChunkStructures* {
                const server::world::Chunk* chunk = world.state().chunkIfLoaded(chunkX, chunkZ);
                return chunk == nullptr ? nullptr : &chunk->structures();
            }};
        const server::world::structure::StructurePos at{originX, originY, originZ};
        for (const Identifier& structure : wanted) {
            if (const server::world::structure::StructureStartRecord* start = structures.startAt(at, structure.path());
                start != nullptr) {
                storage_->claim(claimKey(world, structure, start->chunkX, start->chunkZ));
                break;
            }
        }
    }
    for (const Identifier& structure : wanted) {
        // parity: `skip_existing_chunks` passes over a start an earlier map already claimed.
        const world::ClaimedStart skip = [&](std::int32_t chunkX, std::int32_t chunkZ) {
            return request.skipExistingChunks && storage_->claimed(claimKey(world, structure, chunkX, chunkZ));
        };
        std::optional<std::pair<std::int32_t, std::int32_t>> start;
        if (const auto locator = std::ranges::find(world.structureLocators(), structure, &world::StructureLocator::id);
            locator != world.structureLocators().end()) {
            start = locator->locate(seed, originX >> 4, originZ >> 4, request.searchRadius, skip);
        } else if (server::world::terrain::VillageStructureGenerator* set = world.structureSetGenerator(structure);
                   set != nullptr) {
            const auto village = set->locate(seed, originX >> 4, originZ >> 4, &structure, request.searchRadius, skip);
            if (village.has_value()) {
                start = std::pair{village->chunkX, village->chunkZ};
            }
        }
        if (!start.has_value()) {
            continue;
        }
        // parity: `getLocatePos`, the start chunk's corner, measured from the loot with y at zero.
        const double dx = (start->first * 16.0) - originX;
        const double dy = -static_cast<double>(originY);
        const double dz = (start->second * 16.0) - originZ;
        const double distance = (dx * dx) + (dy * dy) + (dz * dz);
        if (distance < nearestDistance) {
            nearestDistance = distance;
            nearest = Found{.structure = structure, .chunkX = start->first, .chunkZ = start->second};
        }
    }
    if (!nearest.has_value()) {
        return false;
    }
    if (request.skipExistingChunks) {
        storage_->claim(claimKey(world, nearest->structure, nearest->chunkX, nearest->chunkZ));
    }
    if (!wateryBiomes_.has_value()) {
        world::BiomeTagLoader tags{options_.readJar};
        wateryBiomes_ = tags.biomesIn("minecraft:water_on_map_outlines");
    }
    const server::world::terrain::OverworldBiomeSource* biomes = world.biomeSource();
    const auto watery = [&](std::int32_t x, std::int32_t y, std::int32_t z) {
        return biomes != nullptr && wateryBiomes_->contains("minecraft:" + std::string{biomes->blockBiomeAt(x, y, z)});
    };
    makeExplorerMapLocked(components, world, nearest->chunkX * 16, nearest->chunkZ * 16, request.zoom,
                          request.decoration, watery, config::dimensionFacts(world.declaration().dimension).seaLevel);
    return true;
}

std::vector<Identifier> MapSystem::structuresIn(std::string_view tag, std::int32_t depth) const {
    constexpr std::int32_t kMaxDepth = 8;
    std::vector<Identifier> members;
    const std::size_t colon = tag.find(':');
    const std::string path{colon == std::string_view::npos ? tag : tag.substr(colon + 1)};
    const std::optional<std::string> json =
        options_.readJar && depth < kMaxDepth
            ? options_.readJar("data/minecraft/tags/worldgen/structure/" + path + ".json")
            : std::nullopt;
    if (!json.has_value()) {
        return members;
    }
    const auto file = server::registry::tags::tagFileJson::parse(*json);
    if (!file.has_value()) {
        return members;
    }
    for (const server::registry::tags::TagEntry& entry : file->entries) {
        if (entry.isTag) {
            const std::vector<Identifier> nested = structuresIn(entry.id.toString(), depth + 1);
            members.insert(members.end(), nested.begin(), nested.end());
        } else {
            members.push_back(entry.id);
        }
    }
    return members;
}

} // namespace basalt::server::runtime
