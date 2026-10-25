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

bool LoadedWorldRef::setBiomeAt(std::int32_t x, std::int32_t y, std::int32_t z, std::string_view biome) {
    server::world::Chunk* chunk = world_->state().chunkIfLoaded(x >> 4, z >> 4);
    const std::int32_t quartY = (y - chunkHeight().minY()) >> 2;
    if (chunk == nullptr || quartY < 0 || quartY >= chunkHeight().height() / 4) {
        return false;
    }
    const std::string_view path = biome.starts_with("minecraft:") ? biome.substr(10) : biome;
    for (const auto& [biomeId, name] : reg::biomeIdsTable()) {
        if (name == path) {
            chunk->setBiomeId((x & 15) >> 2, quartY, (z & 15) >> 2, biomeId);
            if (hooks_.environment) {
                hooks_.environment().syncBiomes(*world_, x, z, x, z);
            }
            return true;
        }
    }
    return false;
}

std::int32_t LoadedWorldRef::highest(std::int32_t x, std::int32_t z, std::string_view kind, bool load) {
    if (load) {
        ensureChunk(x >> 4, z >> 4);
    }
    const server::world::Chunk* chunk = world_->state().chunkIfLoaded(x >> 4, z >> 4);
    if (chunk == nullptr) {
        return chunkHeight().minY() - 1;
    }
    server::world::HeightmapType type = server::world::HeightmapType::MotionBlocking;
    if (kind == "surface") {
        type = server::world::HeightmapType::WorldSurface;
    } else if (kind == "ocean_floor") {
        type = server::world::HeightmapType::OceanFloor;
    }
    return chunk->heightAt(x & 15, z & 15, type) - 1;
}

bool LoadedWorldRef::chunkLoaded(std::int32_t chunkX, std::int32_t chunkZ) const {
    return world_->state().isLoaded(chunkX, chunkZ);
}

bool LoadedWorldRef::loadChunk(std::int32_t chunkX, std::int32_t chunkZ, bool generate) {
    if (world_->state().isLoaded(chunkX, chunkZ)) {
        return true;
    }
    if (!generate) {
        return false;
    }
    ensureChunk(chunkX, chunkZ);
    return world_->state().isLoaded(chunkX, chunkZ);
}

bool LoadedWorldRef::unloadChunk(std::int32_t chunkX, std::int32_t chunkZ) {
    server::world::Chunk* chunk = world_->state().chunkIfLoaded(chunkX, chunkZ);
    if (chunk == nullptr || world_->isHeld(chunkX, chunkZ)) {
        return false;
    }
    const std::int32_t reach = world_->settings().viewDistance + 1;
    if (players_) {
        for (const player::PlayerSession* player : players_()) {
            if (player->worldName == world_->name() &&
                std::abs(static_cast<std::int32_t>(std::floor(player->x)) / 16 - chunkX) <= reach &&
                std::abs(static_cast<std::int32_t>(std::floor(player->z)) / 16 - chunkZ) <= reach) {
                return false;
            }
        }
    }
    world_->save({chunk});
    return world_->state().evictClean({chunk}) > 0;
}

std::vector<std::int32_t> LoadedWorldRef::loadedChunks() const {
    std::vector<std::int32_t> flat;
    for (const server::world::Chunk* chunk : world_->state().chunks()) {
        flat.push_back(chunk->chunkX());
        flat.push_back(chunk->chunkZ());
    }
    return flat;
}

std::shared_ptr<PluginEntityRef> LoadedWorldRef::entityById(std::int32_t id) const {
    const entity::ServerEntity* found = world_->state().entity(id);
    if (found == nullptr || found->removed) {
        if (std::shared_ptr<PluginEntityRef> item = droppedItemRef(*world_, id); item != nullptr) {
            return item;
        }
        if (players_) {
            for (const player::PlayerSession* session : players_()) {
                if (session->entityId() == id && session->worldName == world_->name()) {
                    return playerEntityRef(*world_, id, players_);
                }
            }
        }
        return nullptr;
    }
    return std::make_shared<WorldEntityRef>(*world_, id, found->type().toString(), shared_, players_);
}

std::shared_ptr<PluginEntityRef> LoadedWorldRef::entityByUuid(std::string_view uuid) const {
    for (const entity::ServerEntity* alive : world_->state().entities()) {
        if (!alive->removed && alive->uuid().toString() == uuid) {
            return std::make_shared<WorldEntityRef>(*world_, alive->entityId(), alive->type().toString(), shared_,
                                                    players_);
        }
    }
    return nullptr;
}

std::vector<std::shared_ptr<PluginEntityRef>> LoadedWorldRef::entitiesIn(std::int32_t chunkX,
                                                                         std::int32_t chunkZ) const {
    std::vector<std::shared_ptr<PluginEntityRef>> refs;
    for (const entity::ServerEntity* alive : world_->state().entitiesIn(chunkX, chunkZ)) {
        if (!alive->removed) {
            refs.push_back(std::make_shared<WorldEntityRef>(*world_, alive->entityId(), alive->type().toString(),
                                                            shared_, players_));
        }
    }
    return refs;
}

void LoadedWorldRef::save() { world_->saveDirty(); }

std::string LoadedWorldRef::folder() const { return world_->name(); }

std::vector<std::shared_ptr<PluginEntityRef>> LoadedWorldRef::entities() const {
    std::vector<std::shared_ptr<PluginEntityRef>> refs;
    for (const entity::ServerEntity* alive : world_->state().entities()) {
        if (!alive->removed) {
            refs.push_back(std::make_shared<WorldEntityRef>(*world_, alive->entityId(), alive->type().toString(),
                                                            shared_, players_));
        }
    }
    for (std::shared_ptr<PluginEntityRef>& item : droppedItemRefs(*world_)) {
        refs.push_back(std::move(item));
    }
    return refs;
}

std::shared_ptr<PluginEntityRef> LoadedWorldRef::spawn(std::string_view type, double x, double y, double z) {
    const std::expected<Identifier, wire::common::IdentifierError> id = Identifier::parse(std::string{type});
    if (!id.has_value() || reg::entityTypes::of(*id) == nullptr || !hooks_.spawnMob) {
        return nullptr;
    }
    entity::ServerEntity* spawned = hooks_.spawnMob(*world_, *id, x, y, z);
    if (spawned == nullptr) {
        return nullptr;
    }
    return std::make_shared<WorldEntityRef>(*world_, spawned->entityId(), spawned->type().toString(), shared_,
                                            players_);
}

std::int32_t LoadedWorldRef::dropItem(std::span<const std::uint8_t> stack, double x, double y, double z,
                                      double velocityX, double velocityY, double velocityZ) {
    return hooks_.nextEntityId
               ? plugins::dropItem(*world_, hooks_.nextEntityId(), stack, x, y, z, velocityX, velocityY, velocityZ)
               : -1;
}

bool LoadedWorldRef::explode(double x, double y, double z, float power, bool breakBlocks, std::int32_t sourceEntityId,
                             bool fire) {
    if (!hooks_.explode) {
        return false;
    }
    hooks_.explode(*world_, x, y, z, power, breakBlocks, sourceEntityId, fire);
    return true;
}

std::shared_ptr<PluginEntityRef> LoadedWorldRef::spawnAs(std::string_view type, double x, double y, double z, float yaw,
                                                         std::string_view reason) {
    const std::expected<Identifier, wire::common::IdentifierError> id = Identifier::parse(std::string{type});
    if (!id.has_value() || reg::entityTypes::of(*id) == nullptr || !hooks_.spawnAs) {
        return nullptr;
    }
    entity::ServerEntity* spawned = hooks_.spawnAs(*world_, *id, x, y, z, yaw, reason);
    if (spawned == nullptr) {
        return nullptr;
    }
    return std::make_shared<WorldEntityRef>(*world_, spawned->entityId(), spawned->type().toString(), shared_,
                                            players_);
}

std::shared_ptr<PluginHologramRef> LoadedWorldRef::hologram(std::string_view text, double x, double y, double z) {
    if (!hooks_.spawnMob) {
        return nullptr;
    }
    entity::ServerEntity* spawned = hooks_.spawnMob(*world_, textDisplay(), x, y, z);
    if (spawned == nullptr) {
        return nullptr;
    }
    spawned->displayText = std::string{text};
    return std::make_shared<HologramRef>(*world_, spawned->entityId(), spawned->type().toString());
}

std::shared_ptr<server::plugins::PluginNpcRef> LoadedWorldRef::npc(std::string_view name, double x, double y, double z,
                                                                   std::function<void(const std::string&)> onClick) {
    if (hooks_.npcs == nullptr || !hooks_.nextEntityId) {
        return nullptr;
    }
    // Nameless on purpose: what is read over its head is the display made below.
    auto npc = std::make_shared<Npc>(hooks_.nextEntityId(), wire::common::Uuid::random(), std::string{}, world_->name(),
                                     x, y, z);
    hooks_.npcs->add(npc, std::move(onClick));
    std::shared_ptr<PluginHologramRef> label;
    if (!name.empty()) {
        label = hologram(name, x, y + NpcHandle::kLabelHeight, z);
    }
    const std::function<std::vector<player::PlayerSession*>()> players = players_;
    return std::make_shared<NpcHandle>(
        std::move(npc), *hooks_.npcs,
        [players](const std::string& playerName) -> std::optional<std::pair<std::string, std::optional<std::string>>> {
            for (const player::PlayerSession* session : players()) {
                if (session->name().size() != playerName.size() ||
                    !std::ranges::equal(session->name(), playerName, [](char left, char right) {
                        return std::tolower(left) == std::tolower(right);
                    })) {
                    continue;
                }
                for (const wire::protocol::login::GameProfileProperty& property : session->profileProperties) {
                    if (property.name == "textures") {
                        return std::pair{property.value, property.signature};
                    }
                }
            }
            return std::nullopt;
        },
        std::move(label));
}

server::world::ChunkHeight LoadedWorldRef::chunkHeight() const {
    return config::dimensionFacts(world_->declaration().dimension).chunkHeight;
}

std::int32_t LoadedWorldRef::biomeIndexOf(std::int32_t biomeId) {
    std::int32_t index = 0;
    for (const auto& [id, name] : reg::biomeIdsTable()) {
        if (id == biomeId) {
            return index;
        }
        ++index;
    }
    return -1;
}

std::optional<std::int32_t> LoadedWorldRef::biomeIdAt(std::int32_t index) {
    std::int32_t at = 0;
    for (const auto& [id, name] : reg::biomeIdsTable()) {
        if (at++ == index) {
            return id;
        }
    }
    return std::nullopt;
}

void LoadedWorldRef::awaitChunk(world::LoadedWorld* world, std::int32_t chunkX, std::int32_t chunkZ,
                                std::int32_t waited, std::shared_ptr<std::function<void(bool)>> done) {
    constexpr std::int32_t kGiveUpTicks = 1200;
    if (world->state().isLoaded(chunkX, chunkZ)) {
        (*done)(true);
        return;
    }
    if (waited >= kGiveUpTicks) {
        (*done)(false);
        return;
    }
    world->owner().schedule(
        1, [world, chunkX, chunkZ, waited, done] { awaitChunk(world, chunkX, chunkZ, waited + 1, done); });
}

void LoadedWorldRef::ensureChunk(std::int32_t chunkX, std::int32_t chunkZ) {
    if (!world_->state().isLoaded(chunkX, chunkZ)) {
        static_cast<void>(world_->state().getOrGenerateChunk(chunkX, chunkZ));
    }
}

bool LoadedWorldRef::hasSkylight() const { return config::dimensionFacts(world_->declaration().dimension).hasSkylight; }

void LoadedWorldRef::showChunks(world::LoadedWorld& world,
                                const std::function<std::vector<player::PlayerSession*>()>& players,
                                std::span<const std::int32_t> chunks) {
    if (!players || chunks.empty()) {
        return;
    }
    constexpr std::int32_t kWatchRadius = 16;
    for (const player::PlayerSession* player : players()) {
        if (player->worldName != world.name() || !player->connection().resendChunk) {
            continue;
        }
        const std::int32_t playerChunkX = static_cast<std::int32_t>(std::floor(player->x)) >> 4;
        const std::int32_t playerChunkZ = static_cast<std::int32_t>(std::floor(player->z)) >> 4;
        for (std::size_t at = 0; at + 1 < chunks.size(); at += 2) {
            if (std::abs(chunks[at] - playerChunkX) <= kWatchRadius &&
                std::abs(chunks[at + 1] - playerChunkZ) <= kWatchRadius) {
                player->connection().resendChunk(chunks[at], chunks[at + 1]);
            }
        }
    }
}

blockUpdates::Target LoadedWorldRef::updateTarget() const {
    std::vector<player::PlayerSession*> watchers;
    if (players_) {
        for (player::PlayerSession* player : players_()) {
            if (player->worldName == world_->name()) {
                watchers.push_back(player);
            }
        }
    }
    return blockUpdates::Target{*world_, hooks_.blocks ? hooks_.blocks() : nullptr, std::move(watchers)};
}

void LoadedWorldRef::tell(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId) const {
    if (!players_) {
        return;
    }
    for (const player::PlayerSession* player : players_()) {
        if (player->worldName == world_->name()) {
            player::sendBlockChangeTo(*player, x, y, z, stateId);
        }
    }
}
} // namespace basalt::server::runtime::plugins::worldModel
