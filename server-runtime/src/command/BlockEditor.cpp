#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/registry/VanillaBlocks.hpp>
#include <basalt/server/runtime/command/BlockEditor.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/NeighbourUpdates.hpp>
#include <basalt/wire/protocol/play/WorldPackets.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace basalt::server::runtime::command {
namespace {

namespace play = wire::protocol::play;
namespace reg = server::registry;
/// `world::` in this namespace is `runtime::world`, which is where the cascade lives.
namespace neighbourUpdates = runtime::world::neighbourUpdates;

using wire::text::Component;

} // namespace

BlockEditor::BlockEditor(const CommandEnvironment& environment, world::LoadedWorld& world)
    : environment_(&environment), world_(&world),
      bounds_(config::dimensionFacts(world.declaration().dimension).chunkHeight) {}

std::optional<std::int32_t> BlockEditor::stateAt(std::int32_t x, std::int32_t y, std::int32_t z) const {
    if (y < bounds_.minY() || y > bounds_.maxY()) {
        return std::nullopt;
    }
    return world_->state().blockState(x, y, z);
}

std::optional<CommandSyntaxError> BlockEditor::require(std::int32_t x, std::int32_t y, std::int32_t z) const {
    if (y < bounds_.minY() || y > bounds_.maxY()) {
        return CommandSyntaxError{.component = Component::translatable("argument.pos.outofbounds")};
    }
    if (!world_->state().isLoaded(x >> 4, z >> 4)) {
        return CommandSyntaxError{.component = Component::translatable("argument.pos.unloaded")};
    }
    return std::nullopt;
}

bool BlockEditor::set(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId) {
    if (y < bounds_.minY() || y > bounds_.maxY()) {
        return false;
    }
    if (world_->state().blockState(x, y, z) == stateId) {
        return false;
    }
    if (!world_->state().setBlockState(x, y, z, stateId)) {
        return false;
    }
    changesBySection_[play::SectionBlocksUpdatePacket::packSection(x >> 4, y >> 4, z >> 4)].insert_or_assign(
        play::SectionBlocksUpdatePacket::packPosition(x, y, z), stateId);
    written_.push_back({x, y, z});
    createBlockEntity(x, y, z, stateId);
    return true;
}

void BlockEditor::createBlockEntity(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId) {
    const wire::common::Identifier* block = reg::blockStateDefinitions::blockOf(stateId);
    if (block == nullptr || reg::blockEntityTypes::ofBlock(*block) == nullptr) {
        return;
    }
    server::world::Chunk* chunk = world_->state().chunkIfLoaded(x >> 4, z >> 4);
    if (chunk == nullptr || chunk->blockEntityAt(x & 15, y, z & 15) != nullptr) {
        return;
    }
    chunk->setBlockEntity(server::world::ChunkBlockEntity{x & 15, y, z & 15, *block, wire::nbt::NbtCompound{}});
}

void BlockEditor::notifyNeighbours() {
    if (written_.empty()) {
        return;
    }
    const std::vector<std::array<std::int32_t, 3>> positions = std::move(written_);
    written_.clear();
    random::RandomSource random = random::RandomSource::legacy(0);
    const config::DimensionFacts& facts = config::dimensionFacts(world_->declaration().dimension);
    const neighbourUpdates::Context context{.world = world_->state(),
                                            .worldName = world_->name(),
                                            .minY = bounds_.minY(),
                                            .maxY = bounds_.maxY(),
                                            .random = random,
                                            .hasSkylight = facts.hasSkylight};
    for (const std::array<std::int32_t, 3>& at : positions) {
        const std::optional<std::int32_t> written = world_->state().blockState(at[0], at[1], at[2]);
        // parity: `BlockInput.place` keeps what the command wrote when the **shape** update leaves
        // air, but a block that nothing holds up is a real removal and stays removed. That is the
        // difference between half a door, which stays, and a dust with nothing under it, which goes.
        bool shapedToAir = false;
        bool unsupported = false;
        neighbourUpdates::afterChange(
            context, at[0], at[1], at[2], effects_,
            [this, &at, &unsupported](std::int32_t, std::int32_t x, std::int32_t y, std::int32_t z) {
                if (x == at[0] && y == at[1] && z == at[2]) {
                    unsupported = true;
                }
                static_cast<void>(set(x, y, z, reg::blocks::air().stateId));
            },
            [this, &at, &shapedToAir](std::int32_t stateId, std::int32_t x, std::int32_t y, std::int32_t z) {
                if (stateId == reg::blocks::air().stateId && x == at[0] && y == at[1] && z == at[2]) {
                    shapedToAir = true;
                }
                static_cast<void>(set(x, y, z, stateId));
            },
            adjustToNeighbours_);
        if (shapedToAir && !unsupported && written.has_value() && *written != reg::blocks::air().stateId &&
            world_->state().blockState(at[0], at[1], at[2]) == reg::blocks::air().stateId) {
            static_cast<void>(set(at[0], at[1], at[2], *written));
        }
    }
    // A reaction can move a block of its own, and those go out with everything else.
    written_.clear();
}

void BlockEditor::flush() {
    notifyNeighbours();
    if (!changesBySection_.empty()) {
        const std::vector<player::PlayerSession*> watchers = environment_->playersIn(*world_);
        for (const auto& [section, changes] : changesBySection_) {
            const auto sectionX = static_cast<std::int32_t>(section >> 42);
            const auto sectionY = static_cast<std::int32_t>((section << 44) >> 44);
            const auto sectionZ = static_cast<std::int32_t>((section << 22) >> 42);
            if (changes.size() == 1) {
                const auto [packed, stateId] = *changes.begin();
                const play::ClientboundBlockUpdatePacket packet{
                    .pos = play::BlockPos{(sectionX << 4) + ((packed >> 8) & 15), (sectionY << 4) + (packed & 15),
                                          (sectionZ << 4) + ((packed >> 4) & 15)},
                    .blockStateId = stateId};
                for (const player::PlayerSession* player : watchers) {
                    if (player->connection().sendPlayPacket) {
                        player->connection().sendPlayPacket(
                            play::ClientboundBlockUpdatePacket::kId,
                            [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
                    }
                }
                continue;
            }
            play::SectionBlocksUpdatePacket packet{.sectionX = sectionX, .sectionY = sectionY, .sectionZ = sectionZ};
            for (const auto& [position, stateId] : changes) {
                packet.changes.push_back({.position = position, .blockStateId = stateId});
            }
            for (const player::PlayerSession* player : watchers) {
                if (player->connection().sendPlayPacket) {
                    player->connection().sendPlayPacket(
                        play::SectionBlocksUpdatePacket::kId,
                        [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
                }
            }
        }
        changesBySection_.clear();
    }
    for (const world::BlockChange& change : effects_) {
        environment_->blockEffects(*world_, change.x, change.y, change.z, change.stateId, change.effects);
    }
    effects_.clear();
}

std::expected<BlockEditor, CommandSyntaxError> blocksOf(const CommandContext& context) {
    const Parsed<world::LoadedWorld*> world = context.world();
    if (!world.has_value()) {
        return std::unexpected{world.error()};
    }
    return BlockEditor{context.environment(), **world};
}

} // namespace basalt::server::runtime::command
