#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/World.hpp>
#include <basalt/server/runtime/BlockChangeBroadcaster.hpp>
#include <basalt/server/runtime/RaidSystem.hpp>
#include <basalt/server/runtime/entity/Raids.hpp>
#include <basalt/server/runtime/plugins/ServerWorldEnv.hpp>
#include <basalt/server/runtime/world/ChunkGenerationQueue.hpp>
#include <basalt/server/runtime/world/Lightning.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/SaplingGrowth.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>
#include <basalt/server/runtime/world/poi/PoiManager.hpp>
#include <basalt/server/world/Chunk.hpp>

#include <algorithm>

namespace basalt::server::runtime::plugins {
namespace {

namespace reg = server::registry;

using server::plugins::PluginFluid;
using server::plugins::PluginPoi;
using server::plugins::PluginRaid;
using server::plugins::PluginSpawnTunables;
using server::plugins::PluginStructureStart;
using wire::common::Identifier;

[[nodiscard]] std::array<std::int32_t, 6> boxOf(const server::world::structure::StructureBoundingBox& box) {
    return {box.minX, box.minY, box.minZ, box.maxX, box.maxY, box.maxZ};
}

[[nodiscard]] std::string_view statusName(const entity::RaidStatus status) {
    switch (status) {
    case entity::RaidStatus::Victory:
        return "victory";
    case entity::RaidStatus::Loss:
        return "loss";
    case entity::RaidStatus::Stopped:
        return "stopped";
    case entity::RaidStatus::Ongoing:
        break;
    }
    return "ongoing";
}

} // namespace

std::vector<PluginStructureStart> ServerWorldEnv::structureStarts(const std::int32_t chunkX,
                                                                  const std::int32_t chunkZ) {
    std::vector<PluginStructureStart> out;
    const server::world::Chunk* chunk = world_->state().chunkIfLoaded(chunkX, chunkZ);
    if (chunk == nullptr) {
        return out;
    }
    for (const server::world::structure::StructureStartRecord& start : chunk->structures().starts()) {
        PluginStructureStart made{.structure = start.structure,
                                  .chunkX = start.chunkX,
                                  .chunkZ = start.chunkZ,
                                  .bounds = boxOf(start.fullBounds)};
        for (const server::world::structure::StructureBoundingBox& piece : start.pieces) {
            made.pieces.push_back(boxOf(piece));
        }
        out.push_back(std::move(made));
    }
    return out;
}

std::vector<PluginRaid> ServerWorldEnv::raids() {
    std::vector<PluginRaid> out;
    RaidSystem::raidsIn(*world_).forEachInTickOrder([&out](const std::int32_t id, entity::Raid& raid) {
        PluginRaid made{.id = id,
                        .started = raid.started,
                        .ticksActive = raid.ticksActive,
                        .omenLevel = raid.raidOmenLevel,
                        .center = raid.center,
                        .status = std::string{statusName(raid.status)},
                        .groupsSpawned = raid.groupsSpawned,
                        .totalGroups = raid.numGroups,
                        .totalHealth = raid.totalHealth};
        for (const wire::common::Uuid& hero : raid.heroes) {
            made.heroes.push_back(hero.toString());
        }
        for (const auto& [wave, ids] : raid.groups) {
            made.raiders.insert(made.raiders.end(), ids.begin(), ids.end());
        }
        out.push_back(std::move(made));
        return true;
    });
    return out;
}

bool ServerWorldEnv::setRaid(const std::int32_t id, const std::int32_t omenLevel, const std::int32_t totalGroups) {
    entity::Raid* raid = RaidSystem::raidsIn(*world_).get(id);
    if (raid == nullptr) {
        return false;
    }
    raid->raidOmenLevel = std::clamp(omenLevel, 0, entity::Raid::kMaxOmenLevel);
    raid->numGroups = std::max(totalGroups, 0);
    RaidSystem::raidsIn(*world_).dirty = true;
    return true;
}

PluginFluid ServerWorldEnv::fluidAt(const std::int32_t x, const std::int32_t y, const std::int32_t z) {
    PluginFluid out;
    const std::optional<std::int32_t> state = world_->state().blockState(x, y, z);
    const Identifier* block = state.has_value() ? reg::blockStateDefinitions::blockOf(*state) : nullptr;
    if (block == nullptr) {
        return out;
    }
    const std::map<std::string, std::string> properties = reg::blockStateDefinitions::propertiesOf(*state);
    if (block->path() == "water" || block->path() == "lava") {
        const auto found = properties.find("level");
        const std::int32_t raw = found == properties.end() ? 0 : std::atoi(found->second.c_str());
        out.fluid = std::string{block->path()};
        out.source = raw == 0;
        out.falling = raw >= 8;
        out.level = raw == 0 || raw >= 8 ? 8 : 8 - raw;
        return out;
    }
    if (const auto logged = properties.find("waterlogged"); logged != properties.end() && logged->second == "true") {
        out = PluginFluid{.fluid = "water", .source = true, .level = 8, .falling = false};
    }
    return out;
}

std::optional<std::array<std::int32_t, 3>> ServerWorldEnv::lightningTarget(const std::int32_t x, const std::int32_t z) {
    return world::lightning::targetFor(world_->state(), x, z);
}

void ServerWorldEnv::gameEvent(const std::string_view event, const double x, const double y, const double z,
                               const std::string_view source) {
    world::vibrations::Source who;
    if (!source.empty()) {
        who.entity = wire::common::Uuid::parse(std::string{source});
    }
    world::vibrations::emit(world_->state(), event, x, y, z, world_->gameTime(), who);
}

bool ServerWorldEnv::growTree(const std::int32_t x, const std::int32_t y, const std::int32_t z,
                              const std::string_view sapling) {
    const std::optional<std::int32_t> sapState =
        reg::blockStateDefinitions::stateIdOf(Identifier::vanilla(std::string{sapling}), {{"stage", "1"}});
    const std::optional<std::int32_t> was = world_->state().blockState(x, y, z);
    if (!sapState.has_value() || !was.has_value() || !world::saplingGrowth::isSapling(*sapState)) {
        return false;
    }
    if (!world_->state().setBlockState(x, y, z, *sapState)) {
        return false;
    }
    random::RandomSource random = random::RandomSource::legacy(world_->gameTime() ^ (x * 31 + z));
    const std::vector<world::BlockChange> changes =
        world::saplingGrowth::advance(world_->state(), x, y, z, *sapState, random);
    if (changes.empty()) {
        static_cast<void>(world_->state().setBlockState(x, y, z, *was));
        return false;
    }
    if (BlockChangeBroadcaster* blocks = hooks_->blocks ? hooks_->blocks() : nullptr; blocks != nullptr) {
        for (const world::BlockChange& change : changes) {
            blocks->announceBlock(watchers(), change);
        }
    }
    return true;
}

std::vector<PluginPoi> ServerWorldEnv::pointsOfInterest(const std::vector<std::string>& types, const std::int32_t x,
                                                        const std::int32_t y, const std::int32_t z,
                                                        const std::int32_t radius, const std::int32_t occupancy) {
    world::poi::PoiMask mask = 0;
    if (types.empty()) {
        mask = world::poi::poiTypes::kAll;
    }
    for (const std::string& name : types) {
        const std::string_view path = name.starts_with("minecraft:") ? std::string_view{name}.substr(10) : name;
        if (const std::optional<world::poi::PoiType> type = world::poi::poiTypes::byName(path); type.has_value()) {
            mask |= world::poi::maskOf(*type);
        }
    }
    std::vector<world::poi::PoiRecord> found;
    world_->state().poi().inRange(mask, world::poi::BlockPos{x, y, z}, radius,
                                  occupancy == 0   ? world::poi::Occupancy::kHasSpace
                                  : occupancy == 1 ? world::poi::Occupancy::kIsOccupied
                                                   : world::poi::Occupancy::kAny,
                                  found);
    std::vector<PluginPoi> out;
    for (const world::poi::PoiRecord& record : found) {
        out.push_back(PluginPoi{.type = "minecraft:" + std::string{world::poi::poiTypes::name(record.type)},
                                .x = record.pos.x,
                                .y = record.pos.y,
                                .z = record.pos.z,
                                .freeTickets = record.freeTickets});
    }
    return out;
}

PluginSpawnTunables ServerWorldEnv::spawnTunables() {
    const world::LoadedWorld::SpawnTunables& held = world_->spawnTunables();
    return PluginSpawnTunables{.limits = held.limits,
                               .ticksPer = held.ticksPer,
                               .allowMonsters = held.allowMonsters,
                               .allowAnimals = held.allowAnimals};
}

void ServerWorldEnv::setSpawnTunables(const PluginSpawnTunables& tunables) {
    world::LoadedWorld::SpawnTunables& held = world_->spawnTunables();
    held.limits = tunables.limits;
    held.ticksPer = tunables.ticksPer;
    held.allowMonsters = tunables.allowMonsters;
    held.allowAnimals = tunables.allowAnimals;
}

std::int32_t ServerWorldEnv::viewDistance() { return world_->settings().viewDistance; }

void ServerWorldEnv::setViewDistance(const std::int32_t chunks) { world_->setViewDistance(std::clamp(chunks, 2, 32)); }

std::int32_t ServerWorldEnv::seaLevel() { return config::dimensionFacts(world_->declaration().dimension).seaLevel; }

std::vector<std::int32_t> ServerWorldEnv::forcedChunks() {
    std::vector<std::int32_t> out;
    for (const std::int64_t key : world_->forcedChunks()) {
        out.push_back(world::ChunkGenerationQueue::unpackX(key));
        out.push_back(world::ChunkGenerationQueue::unpackZ(key));
    }
    return out;
}

void ServerWorldEnv::setChunkForced(const std::int32_t chunkX, const std::int32_t chunkZ, const bool forced) {
    const std::int64_t key = world::ChunkGenerationQueue::pack(chunkX, chunkZ);
    if (forced) {
        world_->forcedChunks().insert(key);
    } else {
        world_->forcedChunks().erase(key);
    }
}

std::array<std::int32_t, 3> ServerWorldEnv::weatherTimes() {
    const world::WorldWeather& weather = world_->weather();
    return {weather.rainTime, weather.thunderTime, weather.clearWeatherTime};
}

void ServerWorldEnv::setWeatherTimes(const std::int32_t rain, const std::int32_t thunder, const std::int32_t clear) {
    world::WorldWeather& weather = world_->weather();
    weather.rainTime = std::max(rain, 0);
    weather.thunderTime = std::max(thunder, 0);
    weather.clearWeatherTime = std::max(clear, 0);
}

std::int64_t ServerWorldEnv::inhabitedTime(const std::int32_t chunkX, const std::int32_t chunkZ) {
    const server::world::Chunk* chunk = world_->state().chunkIfLoaded(chunkX, chunkZ);
    return chunk == nullptr ? 0 : chunk->inhabitedTime();
}

void ServerWorldEnv::setInhabitedTime(const std::int32_t chunkX, const std::int32_t chunkZ, const std::int64_t ticks) {
    if (server::world::Chunk* chunk = world_->state().chunkIfLoaded(chunkX, chunkZ); chunk != nullptr) {
        chunk->increaseInhabitedTime(ticks - chunk->inhabitedTime());
    }
}

bool ServerWorldEnv::useChunkFill(ChunkFill fill) {
    if (world_->declaration().type != config::WorldType::Void) {
        return false;
    }
    std::vector<std::int32_t> biomeIds;
    for (const auto& [id, name] : reg::biomeIdsTable()) {
        biomeIds.push_back(id);
    }
    world_->setChunkFiller([fill = std::move(fill), biomeIds = std::move(biomeIds)](
                               const std::int32_t chunkX, const std::int32_t chunkZ, server::world::Chunk& chunk) {
        std::vector<std::int32_t> blocks;
        std::vector<std::int32_t> biomes;
        fill(chunkX, chunkZ, blocks, biomes);
        for (std::int32_t& biome : biomes) {
            biome = biome >= 0 && static_cast<std::size_t>(biome) < biomeIds.size()
                        ? biomeIds[static_cast<std::size_t>(biome)]
                        : biomeIds.front();
        }
        constexpr std::size_t kBlocks = server::world::ChunkSection::kBlocks;
        constexpr std::size_t kBiomes = server::world::ChunkSection::kBiomes;
        for (std::size_t index = 0; index < chunk.sectionCount(); ++index) {
            if (blocks.size() >= (index + 1) * kBlocks) {
                chunk.section(index).setBlockStateIds(
                    std::span<const std::int32_t>{blocks}.subspan(index * kBlocks, kBlocks));
            }
            if (biomes.size() >= (index + 1) * kBiomes) {
                chunk.section(index).setBiomeIds(
                    std::span<const std::int32_t>{biomes}.subspan(index * kBiomes, kBiomes));
            }
        }
        chunk.invalidateHeightmaps();
    });
    return true;
}

} // namespace basalt::server::runtime::plugins
