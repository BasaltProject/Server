#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/registry/VanillaBlocks.hpp>
#include <basalt/server/runtime/command/BlockEditor.hpp>
#include <basalt/server/runtime/world/FluidFlow.hpp>
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

/// parity: `Level.destroyBlock` writes `fluidState.createLegacyBlock()`, not air, so breaking a
/// waterlogged block leaves the water it was logged with.
[[nodiscard]] std::int32_t leftBehindBy(std::int32_t stateId) {
    const std::map<std::string, std::string> properties = reg::blockStateDefinitions::propertiesOf(stateId);
    const auto logged = properties.find("waterlogged");
    if (logged == properties.end() || logged->second != "true") {
        return reg::blocks::air().stateId;
    }
    const std::optional<std::int32_t> water =
        reg::blockStateDefinitions::stateIdOf(wire::common::Identifier::vanilla("water"), {{"level", "0"}});
    return water.value_or(reg::blocks::air().stateId);
}

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

bool BlockEditor::write(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId,
                        world::WorldState::SideEffects sideEffects) {
    if (y < bounds_.minY() || y > bounds_.maxY()) {
        return false;
    }
    if (world_->state().blockState(x, y, z) == stateId) {
        return false;
    }
    if (!world_->state().setBlockState(x, y, z, stateId, sideEffects)) {
        return false;
    }
    record(x, y, z, stateId);
    written_.push_back({x, y, z});
    createBlockEntity(x, y, z, stateId);
    return true;
}

void BlockEditor::record(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId) {
    changesBySection_[play::SectionBlocksUpdatePacket::packSection(x >> 4, y >> 4, z >> 4)].insert_or_assign(
        play::SectionBlocksUpdatePacket::packPosition(x, y, z), stateId);
}

bool BlockEditor::set(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId) {
    // parity: commands write with `Block.UPDATE_SKIP_BLOCK_ENTITY_SIDEEFFECTS`, so replacing a chest
    // by command destroys what it held instead of dropping it.
    return write(x, y, z, stateId, world::WorldState::SideEffects::kSkip);
}

bool BlockEditor::destroy(std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> old = stateAt(x, y, z);
    if (!old.has_value() || *old == reg::blocks::air().stateId) {
        return false;
    }
    if (environment_->options().dropBrokenBlock) {
        environment_->options().dropBrokenBlock(*world_, x, y, z, *old);
    }
    return write(x, y, z, leftBehindBy(*old), world::WorldState::SideEffects::kRun);
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
    const neighbourUpdates::Context context{
        .world = world_->state(),
        .worldName = world_->name(),
        .minY = bounds_.minY(),
        .maxY = bounds_.maxY(),
        .random = random,
        .hasSkylight = facts.hasSkylight,
        // parity: a `/setblock` of water is a block change like any other, and what makes water
        // flow is a block change. Without this it stood where the command put it for ever.
        .wakeFluids =
            [this, &random](std::int32_t x, std::int32_t y, std::int32_t z) {
                const world::fluidFlow::Context flow{.world = world_->state(),
                                                     .gameRules = world_->gameRules(),
                                                     .random = random,
                                                     .ultrawarm = world_->declaration().dimension ==
                                                                  config::WorldDimension::Nether};
                static_cast<void>(world::fluidFlow::onBlockChanged(flow, x, y, z));
            },
        .gameTime = world_->gameTime()};
    for (const std::array<std::int32_t, 3>& at : positions) {
        const std::optional<std::int32_t> written = world_->state().blockState(at[0], at[1], at[2]);
        // parity: `BlockInput.place` keeps what the command wrote when the **shape** update leaves
        // air, but a block that nothing holds up is a real removal and stays removed. That is the
        // difference between half a door, which stays, and a dust with nothing under it, which goes.
        bool shapedToAir = false;
        bool unsupported = false;
        neighbourUpdates::afterChange(
            context, at[0], at[1], at[2], effects_,
            [this, &at, &unsupported](std::int32_t stateId, std::int32_t x, std::int32_t y, std::int32_t z) {
                if (x == at[0] && y == at[1] && z == at[2]) {
                    unsupported = true;
                }
                static_cast<void>(set(x, y, z, reg::blocks::air().stateId));
                // parity: knocking a block down drops it, whether a hand or a command did it.
                if (environment_->options().dropBrokenBlock) {
                    environment_->options().dropBrokenBlock(*world_, x, y, z, stateId);
                }
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
        // parity: `BlockInput.place` ends in `overwriteWithDefinedProperties`, so a property the
        // command named beats whatever the shape update decided. Without it a note block placed as a
        // harp over glass came out a hat, and the jar leaves it a harp until the block below changes.
        const auto named = written_properties_.find(at);
        if (named == written_properties_.end() || named->second.empty()) {
            continue;
        }
        const std::optional<std::int32_t> shaped = world_->state().blockState(at[0], at[1], at[2]);
        if (!shaped.has_value()) {
            continue;
        }
        const wire::common::Identifier* block = reg::blockStateDefinitions::blockOf(*shaped);
        if (block == nullptr) {
            continue;
        }
        if (const std::optional<std::int32_t> kept =
                reg::blockStateDefinitions::stateIdOf(*block, named->second, *shaped);
            kept.has_value() && *kept != *shaped) {
            static_cast<void>(set(at[0], at[1], at[2], *kept));
            // parity: `onPlace` runs after `overwriteWithDefinedProperties`, so a target or a sensor
            // whose `power` the command named still lands at zero.
            neighbourUpdates::settleArrivedSignal(world_->state(), at[0], at[1], at[2],
                                                  [this](std::int32_t stateId, std::int32_t x, std::int32_t y,
                                                         std::int32_t z) { record(x, y, z, stateId); });
        }
    }
    written_properties_.clear();
    // A reaction can move a block of its own, and those go out with everything else.
    written_.clear();
}

void BlockEditor::keepWritten(const std::int32_t x, const std::int32_t y, const std::int32_t z,
                              std::map<std::string, std::string> properties) {
    written_properties_[std::array<std::int32_t, 3>{x, y, z}] = std::move(properties);
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
