#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/StructureBlockSystem.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/StructureBlocks.hpp>
#include <basalt/server/runtime/world/StructureTemplates.hpp>
#include <basalt/server/world/structure/StructureTemplate.hpp>

#include <algorithm>
#include <chrono>
#include <string>
#include <vector>

namespace basalt::server::runtime {
namespace {

using Pos = std::array<std::int32_t, 3>;
using player::PlayerSession;
using wire::common::Identifier;
using wire::text::Component;
namespace blocks = world::structureBlocks;
namespace structure = server::world::structure;
namespace play = wire::protocol::play;

[[nodiscard]] server::world::ChunkBlockEntity* structureEntity(world::LoadedWorld& world, const Pos& at) {
    server::world::Chunk* chunk = world.state().chunkIfLoaded(at[0] >> 4, at[2] >> 4);
    server::world::ChunkBlockEntity* entity =
        chunk == nullptr ? nullptr : chunk->blockEntityAt(at[0] & 15, at[1], at[2] & 15);
    return entity != nullptr && entity->type().path() == "structure_block" ? entity : nullptr;
}

/// parity: `StructureBlockEntity.createRandom`, where a zero seed means the clock.
[[nodiscard]] random::RandomSource createRandom(std::int64_t seed) {
    if (seed != 0) {
        return random::RandomSource::legacy(seed);
    }
    return random::RandomSource::legacy(
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

[[nodiscard]] std::optional<Identifier> idOf(const blocks::Settings& settings) {
    const std::optional<std::string> name = blocks::resolvedName(settings.name);
    if (!name.has_value()) {
        return std::nullopt;
    }
    const auto parsed = Identifier::parse(*name);
    return parsed.has_value() ? std::optional{*parsed} : std::nullopt;
}

} // namespace

StructureBlockSystem::StructureBlockSystem(world::StructureTemplates& templates, player::PlayerRegistry& players)
    : templates_(&templates), players_(&players) {}

std::optional<std::int32_t> StructureBlockSystem::handle(world::LoadedWorld& world, PlayerSession& player,
                                                         const play::SetStructureBlockPacket& packet) {
    const Pos at{packet.pos.x, packet.pos.y, packet.pos.z};
    server::world::ChunkBlockEntity* entity = structureEntity(world, at);
    if (entity == nullptr) {
        return std::nullopt;
    }
    std::optional<std::int32_t> moved;
    blocks::Settings settings = blocks::read(entity->data());
    settings.mode = static_cast<blocks::Mode>(packet.mode);
    settings.name = blocks::resolvedName(packet.name).value_or(std::string{});
    settings.offset = packet.offset;
    settings.size = packet.size;
    settings.mirror = packet.mirror;
    settings.rotation = packet.rotation;
    settings.metadata = packet.data;
    settings.ignoreEntities = packet.ignoreEntities;
    settings.strict = packet.strict;
    settings.showAir = packet.showAir;
    settings.showBoundingBox = packet.showBoundingBox;
    settings.integrity = packet.integrity;
    settings.seed = packet.seed;
    blocks::write(settings, entity->data());
    // parity: `setMode`, the block's own `mode` written with flag two.
    if (const std::optional<std::int32_t> state = world.state().blockState(at[0], at[1], at[2]); state.has_value()) {
        const std::optional<std::int32_t> moded = server::registry::blockStateDefinitions::stateIdOf(
            Identifier::vanilla("structure_block"), {{"mode", std::string{blocks::modeProperty(settings.mode)}}},
            *state);
        if (moded.has_value() && *moded != *state && world.state().setBlockState(at[0], at[1], at[2], *moded)) {
            moved = *moded;
        }
    }
    if (!settings.name.empty()) {
        const Component name = Component::text(settings.name);
        using Update = play::SetStructureBlockPacket::Update;
        if (packet.update == Update::SaveArea) {
            const bool saved = settings.mode == blocks::Mode::Save && save(world, at, true);
            player.sendSystemMessage(Component::translatable(
                saved ? "structure_block.save_success" : "structure_block.save_failure", {name}));
        } else if (packet.update == Update::LoadArea) {
            const std::optional<Identifier> id = idOf(settings);
            if (settings.mode != blocks::Mode::Load || !id.has_value() || templates_->find(*id) == nullptr) {
                player.sendSystemMessage(Component::translatable("structure_block.load_not_found", {name}));
            } else if (place(world, at, true)) {
                player.sendSystemMessage(Component::translatable("structure_block.load_success", {name}));
            } else {
                player.sendSystemMessage(Component::translatable("structure_block.load_prepare", {name}));
            }
        } else if (packet.update == Update::ScanArea) {
            if (detectSize(world, at)) {
                player.sendSystemMessage(Component::translatable("structure_block.size_success", {name}));
            } else {
                player.sendSystemMessage(Component::translatable("structure_block.size_failure"));
            }
        }
    } else {
        player.sendSystemMessage(
            Component::translatable("structure_block.invalid_structure_name", {Component::text(packet.name)}));
    }
    if (server::world::Chunk* chunk = world.state().chunkIfLoaded(at[0] >> 4, at[2] >> 4); chunk != nullptr) {
        chunk->markDirty();
    }
    sendBlockEntity(world, at);
    return moved;
}

void StructureBlockSystem::trigger(world::LoadedWorld& world, const Pos& at) {
    const server::world::ChunkBlockEntity* entity = structureEntity(world, at);
    if (entity == nullptr) {
        return;
    }
    const blocks::Settings settings = blocks::read(entity->data());
    switch (settings.mode) {
    case blocks::Mode::Save:
        static_cast<void>(save(world, at, false));
        break;
    case blocks::Mode::Load:
        static_cast<void>(place(world, at, false));
        break;
    case blocks::Mode::Corner:
        if (const std::optional<Identifier> id = idOf(settings); id.has_value()) {
            templates_->forget(*id);
        }
        break;
    case blocks::Mode::Data:
        break;
    }
}

bool StructureBlockSystem::save(world::LoadedWorld& world, const Pos& at, bool toDisk) {
    const server::world::ChunkBlockEntity* entity = structureEntity(world, at);
    if (entity == nullptr) {
        return false;
    }
    const blocks::Settings settings = blocks::read(entity->data());
    const std::optional<Identifier> id = idOf(settings);
    if (!id.has_value()) {
        return false;
    }
    const Pos origin{at[0] + settings.offset[0], at[1] + settings.offset[1], at[2] + settings.offset[2]};
    return templates_->store(*id, blocks::capture(world.state(), origin, settings.size, !settings.ignoreEntities),
                             toDisk);
}

bool StructureBlockSystem::detectSize(world::LoadedWorld& world, const Pos& at) {
    server::world::ChunkBlockEntity* entity = structureEntity(world, at);
    if (entity == nullptr) {
        return false;
    }
    blocks::Settings settings = blocks::read(entity->data());
    if (settings.mode != blocks::Mode::Save) {
        return false;
    }
    const std::optional<blocks::Box> box = blocks::enclosingBox(world.state(), at, settings.name);
    if (!box.has_value()) {
        return false;
    }
    const Pos delta{box->max[0] - box->min[0], box->max[1] - box->min[1], box->max[2] - box->min[2]};
    if (delta[0] <= 1 || delta[1] <= 1 || delta[2] <= 1) {
        return false;
    }
    settings.offset = {box->min[0] - at[0] + 1, box->min[1] - at[1] + 1, box->min[2] - at[2] + 1};
    settings.size = {delta[0] - 1, delta[1] - 1, delta[2] - 1};
    blocks::write(settings, entity->data());
    sendBlockEntity(world, at);
    return true;
}

bool StructureBlockSystem::place(world::LoadedWorld& world, const Pos& at, bool sameSizeOnly) {
    server::world::ChunkBlockEntity* entity = structureEntity(world, at);
    if (entity == nullptr) {
        return false;
    }
    blocks::Settings settings = blocks::read(entity->data());
    const std::optional<Identifier> id = idOf(settings);
    if ((sameSizeOnly && settings.mode != blocks::Mode::Load) || !id.has_value()) {
        return false;
    }
    const structure::StructureTemplate* found = templates_->find(*id);
    if (found == nullptr) {
        return false;
    }
    const structure::StructurePos templateSize = found->size();
    const Pos size{templateSize.x, templateSize.y, templateSize.z};
    const bool sameSize = size == settings.size;
    // parity: `loadStructureInfo`, which takes the template's size in either way.
    settings.size = size;
    blocks::write(settings, entity->data());
    if (sameSizeOnly && !sameSize) {
        return false;
    }
    const float integrity = std::clamp(settings.integrity, 0.0F, 1.0F);
    const std::optional<structure::StructureProcessorList> rot =
        settings.integrity < 1.0F
            ? std::optional{structure::StructureProcessorList{{}, structure::BlockRot{.integrity = integrity}}}
            : std::nullopt;
    const structure::StructurePos origin{at[0] + settings.offset[0], at[1] + settings.offset[1],
                                         at[2] + settings.offset[2]};
    const auto rotation = static_cast<structure::StructureRotation>(settings.rotation);
    const auto mirror = static_cast<structure::StructureMirror>(settings.mirror);
    const structure::StructurePos pivot{0, 0, 0};
    const structure::StructureBoundingBox box = found->getBoundingBox(origin, rotation, pivot, mirror);
    for (std::int32_t chunkX = box.minX >> 4; chunkX <= (box.maxX >> 4); ++chunkX) {
        for (std::int32_t chunkZ = box.minZ >> 4; chunkZ <= (box.maxZ >> 4); ++chunkZ) {
            server::world::Chunk* chunk = world.state().chunkIfLoaded(chunkX, chunkZ);
            if (chunk == nullptr) {
                continue;
            }
            // Every pass replays the same streams, so the chunks agree on the palette and the rot.
            random::RandomSource random = createRandom(settings.seed);
            random::RandomSource settingsRandom = createRandom(settings.seed);
            found->placeInto(
                *chunk, structure::TemplatePlacement{
                            .position = origin,
                            .rotation = rotation,
                            .mirror = mirror,
                            .pivot = pivot,
                            .clipTo = structure::StructureBoundingBox{chunkX << 4, box.minY, chunkZ << 4,
                                                                      (chunkX << 4) + 15, box.maxY, (chunkZ << 4) + 15},
                            .processors = rot.has_value() ? &*rot : nullptr,
                            .random = &random,
                            .settingsRandom = rot.has_value() ? &settingsRandom : nullptr,
                            .ignoreEntities = settings.ignoreEntities,
                            .wholePiece = rot.has_value()});
            chunk->markDirty();
            for (PlayerSession* watcher : players_->snapshot()) {
                if (watcher->worldName == world.name() && watcher->connection().resendChunk) {
                    watcher->connection().resendChunk(chunkX, chunkZ);
                }
            }
        }
    }
    return true;
}

void StructureBlockSystem::sendBlockEntity(world::LoadedWorld& world, const Pos& at) {
    const server::world::ChunkBlockEntity* entity = structureEntity(world, at);
    const std::optional<std::int32_t> typeId =
        entity == nullptr ? std::nullopt : server::registry::blockEntityTypes::idOf(entity->type());
    if (!typeId.has_value()) {
        return;
    }
    const play::BlockEntityDataPacket packet{
        .pos = play::BlockPos{at[0], at[1], at[2]}, .typeId = *typeId, .tag = entity->data()};
    for (PlayerSession* watcher : players_->snapshot()) {
        if (watcher->worldName == world.name() && watcher->connection().sendPlayPacket) {
            watcher->connection().sendPlayPacket(play::BlockEntityDataPacket::kId,
                                                 [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
        }
    }
}

} // namespace basalt::server::runtime
