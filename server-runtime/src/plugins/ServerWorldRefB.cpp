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

std::int32_t LoadedWorldRef::validState(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId) {
    return world_->state().isLoaded(x >> 4, z >> 4) ? blockUpdates::validState(updateTarget(), x, y, z, stateId)
                                                    : stateId;
}

std::int32_t LoadedWorldRef::updateShapes(std::span<const std::int32_t> positions, std::span<const std::int32_t> states,
                                          bool indirect) {
    return blockUpdates::shapes(updateTarget(), positions, states, indirect);
}

std::int32_t LoadedWorldRef::useItemOn(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t face,
                                       std::string_view item, float yaw, float pitch) {
    ensureChunk(x >> 4, z >> 4);
    return itemUse::placeBlockItem(updateTarget(), item,
                                   itemUse::Click{.x = x, .y = y, .z = z, .face = face, .yaw = yaw, .pitch = pitch});
}

bool LoadedWorldRef::scheduleTick(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t delay, bool fluid) {
    const std::optional<std::int32_t> here = world_->state().blockState(x, y, z);
    if (!here.has_value()) {
        return false;
    }
    if (!fluid) {
        const Identifier* block = reg::blockStateDefinitions::blockOf(*here);
        if (block == nullptr || *here == reg::blocks::air().stateId) {
            return false;
        }
        world_->state().scheduleBlockTick(x, y, z, *block, delay);
        return true;
    }
    const std::optional<world::FluidKind> kind = world::fluidFlow::kindOf(*here);
    if (!kind.has_value()) {
        return false;
    }
    world_->state().scheduleFluidTick(x, y, z, world::fluidId(*kind, world::isSourceFluid(*here)), delay);
    return true;
}

std::int32_t LoadedWorldRef::applyChanges(std::span<const std::int32_t> positions, std::span<const std::int32_t> states,
                                          std::uint32_t effects) {
    using World = world::WorldState;
    const World::SideEffects spill =
        (effects & BlockEffects::kEntities) != 0 ? World::SideEffects::kRun : World::SideEffects::kSkip;
    const World::PoiTracking poi =
        (effects & BlockEffects::kPoi) != 0 ? World::PoiTracking::kTrack : World::PoiTracking::kSkip;
    const std::size_t count = std::min(positions.size() / 3, states.size());
    std::vector<std::int32_t> changed;
    std::vector<std::int32_t> before;
    for (std::size_t index = 0; index < count; ++index) {
        const std::int32_t x = positions[index * 3];
        const std::int32_t y = positions[index * 3 + 1];
        const std::int32_t z = positions[index * 3 + 2];
        ensureChunk(x >> 4, z >> 4);
        const std::int32_t was = world_->state().blockState(x, y, z).value_or(-1);
        if (was < 0 || was == states[index] || !world_->state().setBlockState(x, y, z, states[index], spill, poi)) {
            continue;
        }
        changed.insert(changed.end(), {x, y, z});
        before.push_back(was);
    }
    static_cast<void>(settleBlocks(changed, before, effects));
    return static_cast<std::int32_t>(changed.size() / 3);
}

bool LoadedWorldRef::writeSection(std::int32_t chunkX, std::int32_t sectionY, std::int32_t chunkZ,
                                  std::span<const std::int32_t> states, bool notify) {
    const std::int32_t index = sectionY - chunkHeight().minSection();
    if (index < 0 || index >= chunkHeight().sectionCount() || states.size() < server::world::ChunkSection::kBlocks) {
        return false;
    }
    ensureChunk(chunkX, chunkZ);
    std::array<std::int32_t, server::world::ChunkSection::kBlocks> before{};
    if (!readSection(chunkX, sectionY, chunkZ, false, before)) {
        return false;
    }
    for (std::int32_t at = 0; at < static_cast<std::int32_t>(before.size()); ++at) {
        if (before[static_cast<std::size_t>(at)] == states[static_cast<std::size_t>(at)]) {
            continue;
        }
        setStateAt((chunkX << 4) + (at & 15), (sectionY << 4) + (at >> 8), (chunkZ << 4) + ((at >> 4) & 15),
                   states[static_cast<std::size_t>(at)], notify);
    }
    return true;
}

std::int32_t LoadedWorldRef::applySections(std::int32_t chunkX, std::int32_t chunkZ,
                                           std::span<const std::int32_t> sectionYs,
                                           std::span<const std::int32_t> states, bool resend,
                                           std::span<std::int32_t> previous) {
    const std::array<std::int32_t, 3> chunk{chunkX, chunkZ, static_cast<std::int32_t>(sectionYs.size())};
    return applyChunks(chunk, sectionYs, states, resend, previous);
}

std::int32_t LoadedWorldRef::applyChunks(std::span<const std::int32_t> chunks, std::span<const std::int32_t> sectionYs,
                                         std::span<const std::int32_t> states, bool resend,
                                         std::span<std::int32_t> previous) {
    constexpr std::size_t kBlocks = server::world::ChunkSection::kBlocks;
    constexpr std::size_t kBlockChangeLimit = 24;
    if (states.size() < sectionYs.size() * kBlocks) {
        return -1;
    }
    std::int32_t changed = 0;
    bool failed = false;
    std::size_t section = 0;
    std::vector<std::int32_t> resent;
    std::vector<std::uint16_t> offsets;
    std::vector<std::uint16_t> chunkOffsets;
    std::vector<std::int32_t> chunkStates;
    for (std::size_t at = 0; at + 2 < chunks.size(); at += 3) {
        const std::int32_t chunkX = chunks[at];
        const std::int32_t chunkZ = chunks[at + 1];
        const std::size_t count = static_cast<std::size_t>(std::max(chunks[at + 2], 0));
        if (section + count > sectionYs.size()) {
            return -1;
        }
        ensureChunk(chunkX, chunkZ);
        std::int32_t inChunk = 0;
        chunkOffsets.clear();
        chunkStates.clear();
        for (std::size_t index = 0; index < count; ++index, ++section) {
            std::span<std::int32_t> held;
            if (previous.size() >= (section + 1) * kBlocks) {
                held = previous.subspan(section * kBlocks, kBlocks);
            }
            offsets.clear();
            const std::span<const std::int32_t> next = states.subspan(section * kBlocks, kBlocks);
            const std::int32_t done =
                world_->state().replaceSection(chunkX, sectionYs[section], chunkZ, next, held, &offsets);
            if (done < 0) {
                failed = true;
                continue;
            }
            inChunk += done;
            if (inChunk <= static_cast<std::int32_t>(kBlockChangeLimit)) {
                for (const std::uint16_t offset : offsets) {
                    chunkOffsets.push_back(static_cast<std::uint16_t>(offset));
                    chunkStates.push_back(sectionYs[section]);
                    chunkStates.push_back(next[offset]);
                }
            }
        }
        changed += inChunk;
        if (inChunk == 0 || !resend) {
            continue;
        }
        if (inChunk <= static_cast<std::int32_t>(kBlockChangeLimit)) {
            for (std::size_t one = 0; one < chunkOffsets.size(); ++one) {
                const std::uint16_t offset = chunkOffsets[one];
                tell((chunkX << 4) + (offset & 15), (chunkStates[one * 2] << 4) + (offset >> 8),
                     (chunkZ << 4) + ((offset >> 4) & 15), chunkStates[one * 2 + 1]);
            }
        } else {
            resent.push_back(chunkX);
            resent.push_back(chunkZ);
        }
    }
    if (!resent.empty()) {
        resendChunks(resent);
    }
    return failed ? -1 : changed;
}

std::uint64_t LoadedWorldRef::blockVersion() const { return world_->state().blockVersion(); }

void LoadedWorldRef::tellBlockEntity(std::int32_t x, std::int32_t y, std::int32_t z,
                                     const server::world::ChunkBlockEntity& made) {
    const Identifier* entityType = reg::blockEntityTypes::ofBlock(made.type());
    const std::optional<std::int32_t> typeId =
        reg::blockEntityTypes::idOf(entityType != nullptr ? *entityType : made.type());
    if (!typeId.has_value() || !players_) {
        return;
    }
    const wire::protocol::play::BlockEntityDataPacket packet{.pos = {x, y, z}, .typeId = *typeId, .tag = made.data()};
    for (const player::PlayerSession* player : players_()) {
        if (player->worldName == world_->name() && player->connection().sendPlayPacket) {
            player->connection().sendPlayPacket(wire::protocol::play::BlockEntityDataPacket::kId,
                                                [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
        }
    }
}

std::optional<std::vector<std::uint8_t>> LoadedWorldRef::blockEntityNbt(std::int32_t x, std::int32_t y,
                                                                        std::int32_t z) {
    const server::world::Chunk* chunk = world_->state().chunkIfLoaded(x >> 4, z >> 4);
    const server::world::ChunkBlockEntity* held = chunk == nullptr ? nullptr : chunk->blockEntityAt(x & 15, y, z & 15);
    if (held == nullptr) {
        return std::nullopt;
    }
    wire::nbt::NbtCompound tag = held->data();
    const Identifier* entityType = reg::blockEntityTypes::ofBlock(held->type());
    tag.putString("id", (entityType != nullptr ? *entityType : held->type()).toString());
    tag.putInt("x", x);
    tag.putInt("y", y);
    tag.putInt("z", z);
    return wire::nbt::nbtIO::write(wire::nbt::NbtTag::ofCompound(std::move(tag)));
}

std::vector<std::int32_t> LoadedWorldRef::blockEntityPositions(std::int32_t chunkX, std::int32_t chunkZ) {
    std::vector<std::int32_t> found;
    const server::world::Chunk* chunk = world_->state().chunkIfLoaded(chunkX, chunkZ);
    if (chunk == nullptr) {
        return found;
    }
    for (const server::world::ChunkBlockEntity& held : chunk->blockEntities()) {
        found.push_back((chunkX << 4) + held.localX());
        found.push_back(held.y());
        found.push_back((chunkZ << 4) + held.localZ());
    }
    return found;
}

bool LoadedWorldRef::setBlockEntityNbt(std::int32_t x, std::int32_t y, std::int32_t z,
                                       std::span<const std::uint8_t> nbt) {
    server::world::Chunk* chunk = world_->state().chunkIfLoaded(x >> 4, z >> 4);
    const server::world::ChunkBlockEntity* held = chunk == nullptr ? nullptr : chunk->blockEntityAt(x & 15, y, z & 15);
    const auto parsed = wire::nbt::nbtIO::read(nbt);
    const wire::nbt::NbtCompound* given = parsed.has_value() ? parsed->tag.get<wire::nbt::NbtCompound>() : nullptr;
    if (held == nullptr || given == nullptr) {
        return false;
    }
    wire::nbt::NbtCompound data = *given;
    for (const char* key : {"id", "x", "y", "z"}) {
        data.remove(key);
    }
    world_->state().noteSculkRange(data.getInt("Paper.ListenerRange"));
    server::world::ChunkBlockEntity next = held->withData(std::move(data));
    chunk->setBlockEntity(next);
    tellBlockEntity(x, y, z, next);
    return true;
}

bool LoadedWorldRef::createBlockEntity(std::int32_t x, std::int32_t y, std::int32_t z,
                                       std::span<const std::uint8_t> nbt) {
    server::world::Chunk* chunk = world_->state().chunkIfLoaded(x >> 4, z >> 4);
    const std::int32_t stateId = chunk == nullptr ? -1 : world_->state().blockState(x, y, z).value_or(-1);
    const Identifier* block = stateId < 0 ? nullptr : reg::blockStateDefinitions::blockOf(stateId);
    if (block == nullptr || reg::blockEntityTypes::ofBlock(*block) == nullptr) {
        return false;
    }
    wire::nbt::NbtCompound data = world::blockEntityDefaults::of(*block, stateId);
    if (!nbt.empty()) {
        const auto parsed = wire::nbt::nbtIO::read(nbt);
        const wire::nbt::NbtCompound* given = parsed.has_value() ? parsed->tag.get<wire::nbt::NbtCompound>() : nullptr;
        if (given == nullptr) {
            return false;
        }
        data = *given;
        for (const char* key : {"id", "x", "y", "z"}) {
            data.remove(key);
        }
    }
    world_->state().noteSculkRange(data.getInt("Paper.ListenerRange"));
    const server::world::ChunkBlockEntity made{x & 15, y, z & 15, *block, std::move(data)};
    chunk->setBlockEntity(made);
    tellBlockEntity(x, y, z, made);
    return true;
}

bool LoadedWorldRef::removeBlockEntity(std::int32_t x, std::int32_t y, std::int32_t z) {
    server::world::Chunk* chunk = world_->state().chunkIfLoaded(x >> 4, z >> 4);
    if (chunk == nullptr || chunk->blockEntityAt(x & 15, y, z & 15) == nullptr) {
        return false;
    }
    chunk->removeBlockEntityAt(x & 15, y, z & 15);
    return true;
}

std::optional<std::vector<std::uint8_t>> LoadedWorldRef::entityNbt(std::int32_t entityId) {
    const entity::ServerEntity* found = world_->state().entity(entityId);
    if (found == nullptr) {
        return std::nullopt;
    }
    return wire::nbt::nbtIO::write(wire::nbt::NbtTag::ofCompound(entity::entityNbt::encode(*found)));
}

std::int32_t LoadedWorldRef::spawnEntityNbt(std::span<const std::uint8_t> nbt, std::string_view cause) {
    const auto parsed = wire::nbt::nbtIO::read(nbt);
    const wire::nbt::NbtCompound* given = parsed.has_value() ? parsed->tag.get<wire::nbt::NbtCompound>() : nullptr;
    if (given == nullptr || !hooks_.nextEntityId) {
        return -1;
    }
    if (hooks_.spawnAllowed) {
        const auto type = Identifier::parse(std::string{given->getString("id")});
        std::array<double, 3> at{};
        if (const wire::nbt::NbtList* pos = given->getList("Pos"); pos != nullptr && pos->size() >= 3) {
            for (std::size_t axis = 0; axis < 3; ++axis) {
                if (const double* value = (*pos)[axis].get<double>(); value != nullptr) {
                    at[axis] = *value;
                }
            }
        }
        if (type.has_value() && !hooks_.spawnAllowed(*world_, *type, at[0], at[1], at[2], cause)) {
            return -1;
        }
    }
    const entity::entityNbt::AllocateEntityIdFor allocate = [this](const Identifier&) { return hooks_.nextEntityId(); };
    const entity::entityNbt::ProtocolTypeOf protocolTypeOf = [](const Identifier& type) -> std::optional<std::int32_t> {
        const reg::EntityTypeEntry* entry = reg::entityTypes::of(type);
        return entry == nullptr ? std::nullopt : std::optional{entry->protocolId};
    };
    std::int32_t first = -1;
    for (std::unique_ptr<entity::ServerEntity>& restored :
         entity::entityNbt::decodeTree(*given, allocate, protocolTypeOf)) {
        const std::int32_t id = world_->state().spawnEntity(std::move(restored)).entityId();
        first = first < 0 ? id : first;
    }
    return first;
}

std::optional<LoadedWorldRef::DragonFightFacts> LoadedWorldRef::dragonFight() {
    const world::DragonFight* fight = world_->dragonFight();
    if (fight == nullptr) {
        return std::nullopt;
    }
    DragonFightFacts facts;
    facts.previouslyKilled = fight->previouslyKilled;
    facts.portal = fight->exitPortalLocation;
    if (fight->respawnStage.has_value()) {
        facts.respawnStage = std::string{world::respawnStageName(*fight->respawnStage)};
    }
    facts.gatewaysLeft = static_cast<std::int32_t>(fight->gateways.size());
    if (fight->dragonUuid.has_value()) {
        facts.dragonUuid = fight->dragonUuid->toString();
    }
    facts.barProgress = fight->barProgress;
    facts.barVisible = fight->barVisible;
    facts.barTitle = fight->barTitle;
    facts.barColor = fight->barColor;
    facts.barStyle = fight->barStyle;
    facts.barFlags = fight->barFlags;
    facts.barPlayers.assign(fight->barPlayers.begin(), fight->barPlayers.end());
    for (const wire::common::Uuid& crystal : fight->respawnCrystals) {
        facts.respawnCrystals.push_back(crystal.toString());
    }
    // parity: `EnderDragonFight.getSpikeCrystals`, the crystals over each pillar.
    for (const auto& spike : server::world::terrain::decoration::end::endSpikes::forSeed(world_->metadata().seed)) {
        for (const entity::ServerEntity* candidate : world_->state().entities()) {
            if (!candidate->removed && world::endCrystals::isCrystal(candidate->type()) &&
                candidate->x + 1.0 > spike.centerX - spike.radius &&
                candidate->x - 1.0 < spike.centerX + spike.radius &&
                candidate->z + 1.0 > spike.centerZ - spike.radius &&
                candidate->z - 1.0 < spike.centerZ + spike.radius) {
                facts.healingCrystals.push_back(candidate->uuid().toString());
            }
        }
    }
    return facts;
}
} // namespace basalt::server::runtime::plugins::worldModel
