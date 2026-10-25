#include "DataFix.hpp"
#include "ServerWorldRefs.hpp"
#include "WorldBlockUpdates.hpp"
#include "WorldItemUse.hpp"

#include <basalt/server/registry/BlockFluids.hpp>
#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/BlockSupport.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/registry/ItemTools.hpp>
#include <basalt/server/registry/MapColors.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/registry/VanillaTables.hpp>
#include <basalt/server/registry/World.hpp>
#include <basalt/server/runtime/BlockChangeBroadcaster.hpp>
#include <basalt/server/runtime/command/CommandEnvironment.hpp>
#include <basalt/server/runtime/entity/EntityNbt.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/Villagers.hpp>
#include <basalt/server/runtime/placeholder/PlaceholderSlots.hpp>
#include <basalt/server/runtime/player/PlayerBlockUpdates.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/plugins/DroppedItemOps.hpp>
#include <basalt/server/runtime/plugins/EntityOps.hpp>
#include <basalt/server/runtime/plugins/PlayerEntityOps.hpp>
#include <basalt/server/runtime/plugins/ServerObjectModel.hpp>
#include <basalt/server/runtime/plugins/ServerWorldEnv.hpp>
#include <basalt/server/runtime/registry/VanillaConfigurationRegistries.hpp>
#include <basalt/server/runtime/world/BlockEntityDefaults.hpp>
#include <basalt/server/runtime/world/EndCrystals.hpp>
#include <basalt/server/runtime/world/FluidFlow.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/MobEffects.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>
#include <basalt/server/runtime/world/WorldLight.hpp>
#include <basalt/server/runtime/world/maps/MapDecorationTypes.hpp>
#include <basalt/server/runtime/world/poi/PoiTypes.hpp>
#include <basalt/server/storage/PlayerStore.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/light/Light.hpp>
#include <basalt/server/world/structure/StructureTemplate.hpp>
#include <basalt/server/world/terrain/decoration/end/EndSpikes.hpp>
#include <basalt/wire/common/placeholder/PlaceholderText.hpp>
#include <basalt/wire/nbt/NbtIO.hpp>
#include <basalt/wire/protocol/play/BlockEntityPackets.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <utility>

namespace basalt::server::runtime::plugins::worldModel {
namespace reg = server::registry;
using wire::common::Identifier;

bool LoadedWorldRef::setDragonPreviouslyKilled(bool value) {
    world::DragonFight* fight = world_->dragonFight();
    if (fight == nullptr) {
        return false;
    }
    fight->previouslyKilled = value;
    return true;
}

bool LoadedWorldRef::placeEndGateway(std::int32_t x, std::int32_t y, std::int32_t z) {
    return hooks_.endGatewayAt && hooks_.endGatewayAt(*world_, x, y, z);
}

bool LoadedWorldRef::dragonFightAction(std::string_view action, std::int32_t value) {
    return hooks_.endFight && hooks_.endFight(*world_, action, value);
}

bool LoadedWorldRef::dragonFightBar(std::string_view action, std::string_view text, std::int32_t value) {
    return hooks_.endFightBar && hooks_.endFightBar(*world_, action, text, value);
}

std::optional<std::array<std::int32_t, 2>> LoadedWorldRef::locateStructure(std::string_view structure, std::int32_t x,
                                                                           std::int32_t z, std::int32_t cells,
                                                                           bool skipExisting) {
    static_cast<void>(skipExisting);
    const auto wanted = Identifier::parse(std::string{structure});
    if (!wanted.has_value()) {
        return std::nullopt;
    }
    const std::int64_t seed = world_->metadata().seed;
    if (*wanted == Identifier::vanilla("stronghold")) {
        server::world::terrain::StrongholdStructureGenerator* strongholds = world_->strongholdGenerator();
        const auto found = strongholds == nullptr ? std::nullopt : strongholds->locate(x, z);
        return found.has_value() ? std::optional{std::array{found->first * 16, found->second * 16}} : std::nullopt;
    }
    for (const world::StructureLocator& locator : world_->structureLocators()) {
        if (locator.id == *wanted) {
            const auto found = locator.locate(seed, x >> 4, z >> 4, cells, {});
            return found.has_value() ? std::optional{std::array{found->first * 16, found->second * 16}} : std::nullopt;
        }
    }
    if (server::world::terrain::VillageStructureGenerator* generator = world_->structureSetGenerator(*wanted);
        generator != nullptr) {
        const auto found = generator->locate(seed, x >> 4, z >> 4, &*wanted);
        return found.has_value() ? std::optional{std::array{found->chunkX * 16, found->chunkZ * 16}} : std::nullopt;
    }
    return std::nullopt;
}

std::optional<std::array<std::int32_t, 3>> LoadedWorldRef::locateBiome(const std::vector<std::string>& biomes,
                                                                       std::int32_t x, std::int32_t y, std::int32_t z,
                                                                       std::int32_t radius, std::int32_t step) {
    const server::world::terrain::OverworldBiomeSource* source = world_->biomeSource();
    if (source == nullptr || step <= 0) {
        return std::nullopt;
    }
    for (std::int32_t ring = 0; ring <= radius; ring += step) {
        for (std::int32_t dx = -ring; dx <= ring; dx += step) {
            for (std::int32_t dz = -ring; dz <= ring; dz += step) {
                if (ring > 0 && dx * dx != ring * ring && dz * dz != ring * ring) {
                    continue;
                }
                const std::string_view name = source->biomeAt(x + dx, y, z + dz);
                if (std::ranges::any_of(biomes, [name](const std::string& wanted) {
                        return wanted == name || (wanted.starts_with("minecraft:") && wanted.substr(10) == name);
                    })) {
                    return std::array{x + dx, y, z + dz};
                }
            }
        }
    }
    return std::nullopt;
}

std::vector<std::uint8_t> LoadedWorldRef::packedData(const wire::nbt::NbtCompound& data) {
    return wire::nbt::nbtIO::write(wire::nbt::NbtTag::ofCompound(data));
}

std::optional<wire::nbt::NbtCompound> LoadedWorldRef::unpackedData(std::span<const std::uint8_t> nbt) {
    const auto parsed = wire::nbt::nbtIO::read(nbt);
    const wire::nbt::NbtCompound* given = parsed.has_value() ? parsed->tag.get<wire::nbt::NbtCompound>() : nullptr;
    return given == nullptr ? std::nullopt : std::optional{*given};
}

std::vector<std::uint8_t> LoadedWorldRef::pluginData() { return packedData(world_->pluginData()); }

bool LoadedWorldRef::setPluginData(std::span<const std::uint8_t> nbt) {
    std::optional<wire::nbt::NbtCompound> given = unpackedData(nbt);
    if (!given.has_value()) {
        return false;
    }
    world_->setPluginData(std::move(*given));
    return true;
}

std::optional<std::vector<std::uint8_t>> LoadedWorldRef::chunkPluginData(std::int32_t chunkX, std::int32_t chunkZ) {
    const server::world::Chunk* chunk = world_->state().chunkIfLoaded(chunkX, chunkZ);
    return chunk == nullptr ? std::nullopt : std::optional{packedData(chunk->pluginData())};
}

bool LoadedWorldRef::setChunkPluginData(std::int32_t chunkX, std::int32_t chunkZ, std::span<const std::uint8_t> nbt) {
    server::world::Chunk* chunk = world_->state().chunkIfLoaded(chunkX, chunkZ);
    std::optional<wire::nbt::NbtCompound> given = unpackedData(nbt);
    if (chunk == nullptr || !given.has_value()) {
        return false;
    }
    chunk->setPluginData(std::move(*given));
    return true;
}

std::optional<std::vector<std::uint8_t>> LoadedWorldRef::blockEntityPluginData(std::int32_t x, std::int32_t y,
                                                                               std::int32_t z) {
    const server::world::Chunk* chunk = world_->state().chunkIfLoaded(x >> 4, z >> 4);
    const server::world::ChunkBlockEntity* held = chunk == nullptr ? nullptr : chunk->blockEntityAt(x & 15, y, z & 15);
    if (held == nullptr) {
        return std::nullopt;
    }
    const wire::nbt::NbtCompound* kept = held->data().getCompound(kBukkitValues);
    return packedData(kept == nullptr ? wire::nbt::NbtCompound{} : *kept);
}

bool LoadedWorldRef::setBlockEntityPluginData(std::int32_t x, std::int32_t y, std::int32_t z,
                                              std::span<const std::uint8_t> nbt) {
    server::world::Chunk* chunk = world_->state().chunkIfLoaded(x >> 4, z >> 4);
    const server::world::ChunkBlockEntity* held = chunk == nullptr ? nullptr : chunk->blockEntityAt(x & 15, y, z & 15);
    std::optional<wire::nbt::NbtCompound> given = unpackedData(nbt);
    if (held == nullptr && chunk != nullptr && given.has_value() && !given->empty() && createBlockEntity(x, y, z, {})) {
        held = chunk->blockEntityAt(x & 15, y, z & 15);
    }
    if (held == nullptr || !given.has_value()) {
        return false;
    }
    wire::nbt::NbtCompound data = held->data();
    if (given->empty()) {
        data.remove(kBukkitValues);
    } else {
        data.set(kBukkitValues, wire::nbt::NbtTag::ofCompound(std::move(*given)));
    }
    chunk->setBlockEntity(held->withData(std::move(data)));
    return true;
}

std::optional<std::vector<std::uint8_t>> LoadedWorldRef::entityPluginData(std::int32_t entityId) {
    const entity::ServerEntity* found = world_->state().entity(entityId);
    return found == nullptr ? std::nullopt : std::optional{packedData(found->pluginData)};
}

bool LoadedWorldRef::setEntityPluginData(std::int32_t entityId, std::span<const std::uint8_t> nbt) {
    entity::ServerEntity* found = world_->state().entity(entityId);
    std::optional<wire::nbt::NbtCompound> given = unpackedData(nbt);
    if (found == nullptr || !given.has_value()) {
        return false;
    }
    found->pluginData = std::move(*given);
    return true;
}

bool LoadedWorldRef::readBiomes(std::int32_t chunkX, std::int32_t chunkZ, std::span<std::int32_t> out) {
    const server::world::Chunk* chunk = world_->state().chunkIfLoaded(chunkX, chunkZ);
    const std::int32_t cells = chunkHeight().height() / 4;
    if (chunk == nullptr || out.size() < static_cast<std::size_t>(cells) * 16) {
        return false;
    }
    for (std::int32_t qy = 0; qy < cells; ++qy) {
        for (std::int32_t qz = 0; qz < 4; ++qz) {
            for (std::int32_t qx = 0; qx < 4; ++qx) {
                out[static_cast<std::size_t>((qy * 4 + qz) * 4 + qx)] = biomeIndexOf(chunk->biomeId(qx, qy, qz));
            }
        }
    }
    return true;
}

bool LoadedWorldRef::writeBiomes(std::int32_t chunkX, std::int32_t chunkZ, std::span<const std::int32_t> ids) {
    server::world::Chunk* chunk = world_->state().chunkIfLoaded(chunkX, chunkZ);
    const std::int32_t cells = chunkHeight().height() / 4;
    if (chunk == nullptr || ids.size() < static_cast<std::size_t>(cells) * 16) {
        return false;
    }
    for (std::int32_t qy = 0; qy < cells; ++qy) {
        for (std::int32_t qz = 0; qz < 4; ++qz) {
            for (std::int32_t qx = 0; qx < 4; ++qx) {
                const std::int32_t id = ids[static_cast<std::size_t>((qy * 4 + qz) * 4 + qx)];
                if (const std::optional<std::int32_t> biome = biomeIdAt(id); biome.has_value()) {
                    chunk->setBiomeId(qx, qy, qz, *biome);
                }
            }
        }
    }
    const std::array<std::int32_t, 2> pair{chunkX, chunkZ};
    resendChunks(pair);
    return true;
}

void LoadedWorldRef::lightJob(std::span<const std::int32_t> chunks, std::function<void(std::int32_t)> done) {
    std::vector<std::int64_t> keys;
    for (std::size_t at = 0; at + 1 < chunks.size(); at += 2) {
        keys.push_back(world::ChunkGenerationQueue::pack(chunks[at], chunks[at + 1]));
    }
    world_->relightAsync(std::move(keys), [world = world_, players = players_,
                                           pairs = std::vector<std::int32_t>(chunks.begin(), chunks.end()),
                                           done = std::move(done)](std::int32_t relit) {
        showChunks(*world, players, pairs);
        done(relit);
    });
}

std::int32_t LoadedWorldRef::lightAt(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t layer) {
    if (!world_->state().isLoaded(x >> 4, z >> 4)) {
        return -1;
    }
    const bool sky = layer == 1;
    world::worldLight::refresh(world_->state(), x >> 4, z >> 4, hasSkylight());
    return sky ? world::worldLight::skyLight(world_->state(), x, y, z, hasSkylight())
               : world::worldLight::blockLight(world_->state(), x, y, z);
}

bool LoadedWorldRef::readLight(std::int32_t chunkX, std::int32_t sectionY, std::int32_t chunkZ, std::int32_t layer,
                               bool load, std::span<std::uint8_t> out) {
    const bool sky = layer == 1;
    if (load) {
        ensureChunk(chunkX, chunkZ);
    }
    // parity: the engine answers with what it has worked out so far; the light thread catches up with changes.
    if (out.size() < kLightBytes || (sky && !hasSkylight())) {
        return false;
    }
    const server::world::Chunk* chunk = world_->state().chunkIfLoaded(chunkX, chunkZ);
    const server::world::Chunk::SharedLight cached =
        chunk == nullptr ? nullptr : (sky ? chunk->cachedSkyLight() : chunk->cachedBlockLight());
    const std::int32_t index = sectionY - chunkHeight().minSection();
    if (cached == nullptr || index < 0 || index >= chunkHeight().sectionCount()) {
        return false;
    }
    // A layer with nothing lit in it is an empty list, and a section with nothing in it an empty array.
    std::ranges::fill(out.first(kLightBytes), std::uint8_t{0});
    if (static_cast<std::size_t>(index) < cached->size()) {
        const std::vector<std::uint8_t>& bytes = (*cached)[static_cast<std::size_t>(index)];
        std::copy_n(bytes.begin(), std::min(bytes.size(), kLightBytes), out.begin());
    }
    return true;
}

bool LoadedWorldRef::writeLight(std::int32_t chunkX, std::int32_t sectionY, std::int32_t chunkZ, std::int32_t layer,
                                std::span<const std::uint8_t> nibbles) {
    const bool sky = layer == 1;
    if (nibbles.size() < kLightBytes || (sky && !hasSkylight())) {
        return false;
    }
    server::world::Chunk* chunk = world_->state().chunkIfLoaded(chunkX, chunkZ);
    const std::int32_t index = sectionY - chunkHeight().minSection();
    if (chunk == nullptr || index < 0 ||
        !chunk->storeLightSection(sky, static_cast<std::size_t>(index), nibbles.first(kLightBytes))) {
        return false;
    }
    // The engine owns the light: what a plugin wrote is read back until things settle, then worked out again.
    constexpr std::chrono::milliseconds kQuiet{250};
    world_->relightSoon(
        chunkX, chunkZ, kQuiet,
        [world = world_, players = players_, pair = std::array{chunkX, chunkZ}] { showChunks(*world, players, pair); });
    return true;
}

void LoadedWorldRef::checkLight(std::span<const std::int32_t> positions) {
    for (std::size_t at = 0; at + 2 < positions.size(); at += 3) {
        world_->state().markLightAround(positions[at] >> 4, positions[at + 2] >> 4);
    }
}

void LoadedWorldRef::resendChunks(std::span<const std::int32_t> chunks) {
    std::vector<std::int32_t> ready;
    std::vector<std::int32_t> unlit;
    std::vector<std::int64_t> keys;
    for (std::size_t at = 0; at + 1 < chunks.size(); at += 2) {
        const server::world::Chunk* chunk = world_->state().chunkIfLoaded(chunks[at], chunks[at + 1]);
        if (chunk == nullptr) {
            continue;
        }
        const bool lit = chunk->cachedBlockLight() != nullptr &&
                         (!hasSkylight() || chunk->cachedSkyLight() != nullptr) && !chunk->lightIsStale();
        std::vector<std::int32_t>& into = lit ? ready : unlit;
        into.push_back(chunks[at]);
        into.push_back(chunks[at + 1]);
        if (!lit) {
            keys.push_back(world::ChunkGenerationQueue::pack(chunks[at], chunks[at + 1]));
        }
    }
    showChunks(*world_, players_, ready);
    if (!unlit.empty()) {
        world_->relightAsync(std::move(keys), [world = world_, players = players_, pairs = std::move(unlit)](
                                                  std::int32_t) { showChunks(*world, players, pairs); });
    }
}

std::optional<std::string> LoadedWorldRef::biomeAt(std::int32_t x, std::int32_t y, std::int32_t z) const {
    const server::world::Chunk* chunk = world_->state().chunkIfLoaded(x >> 4, z >> 4);
    const std::int32_t quartY = (y - chunkHeight().minY()) >> 2;
    if (chunk == nullptr || quartY < 0 || quartY >= chunkHeight().height() / 4) {
        return std::nullopt;
    }
    const std::int32_t id = chunk->biomeId((x & 15) >> 2, quartY, (z & 15) >> 2);
    for (const auto& [biomeId, name] : reg::biomeIdsTable()) {
        if (biomeId == id) {
            return "minecraft:" + std::string{name};
        }
    }
    return std::nullopt;
}
} // namespace basalt::server::runtime::plugins::worldModel
