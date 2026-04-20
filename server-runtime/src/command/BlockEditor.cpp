#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/registry/VanillaBlocks.hpp>
#include <basalt/server/runtime/command/BlockEditor.hpp>
#include <basalt/server/runtime/player/PlayerBlockUpdates.hpp>
#include <basalt/server/runtime/world/BlockEntityDefaults.hpp>
#include <basalt/server/runtime/world/CommandBlocks.hpp>
#include <basalt/server/runtime/world/ComparatorReading.hpp>
#include <basalt/server/runtime/world/FluidFlow.hpp>
#include <basalt/server/runtime/world/Geysers.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/NeighbourUpdates.hpp>
#include <basalt/server/runtime/world/Rails.hpp>
#include <basalt/server/runtime/world/RedstoneComponents.hpp>
#include <basalt/wire/protocol/play/WorldPackets.hpp>

#include <cstdint>
#include <set>
#include <string>
#include <vector>

namespace basalt::server::runtime::command {
namespace {

namespace play = wire::protocol::play;
namespace reg = server::registry;
/// `world::` in this namespace is `runtime::world`, which is where the cascade lives.
namespace neighbourUpdates = runtime::world::neighbourUpdates;

using wire::text::Component;

/// parity: `Level.destroyBlock` writes `fluidState.createLegacyBlock()`, not air.
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
    const std::optional<std::int32_t> before = world_->state().blockState(x, y, z);
    if (before == stateId) {
        return false;
    }
    if (!world_->state().setBlockState(x, y, z, stateId, sideEffects)) {
        return false;
    }
    replaced_.insert_or_assign(std::array<std::int32_t, 3>{x, y, z}, before);
    record(x, y, z, stateId);
    written_.push_back({x, y, z});
    createBlockEntity(x, y, z, stateId);
    return true;
}

void BlockEditor::bendRail(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId,
                           std::optional<std::int32_t> before) {
    // parity: `BaseRailBlock.onPlace`.
    if (!world::rails::isRail(stateId)) {
        return;
    }
    const wire::common::Identifier* now = reg::blockStateDefinitions::blockOf(stateId);
    const wire::common::Identifier* was = before.has_value() ? reg::blockStateDefinitions::blockOf(*before) : nullptr;
    if (now == nullptr || (was != nullptr && *was == *now)) {
        return;
    }
    const std::optional<std::int32_t> bent = world::rails::place(
        world_->state(), world_->name(), x, y, z,
        [this](std::int32_t bx, std::int32_t by, std::int32_t bz, std::int32_t id) { record(bx, by, bz, id); });
    if (bent.has_value()) {
        record(x, y, z, *bent);
    }
}

void BlockEditor::record(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId) {
    changesBySection_[play::SectionBlocksUpdatePacket::packSection(x >> 4, y >> 4, z >> 4)].insert_or_assign(
        play::SectionBlocksUpdatePacket::packPosition(x, y, z), stateId);
}

bool BlockEditor::set(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId) {
    // parity: commands write with `Block.UPDATE_SKIP_BLOCK_ENTITY_SIDEEFFECTS`.
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
    // parity: `SetBlockCommand` in `destroy` mode runs `level.destroyBlock(pos.
    destroyed_.insert({x, y, z});
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
    // parity: a block entity is not an empty bag.
    wire::nbt::NbtCompound data = world::blockEntityDefaults::of(*block, stateId);
    chunk->setBlockEntity(server::world::ChunkBlockEntity{x & 15, y, z & 15, *block, std::move(data)});
}

void BlockEditor::notifyNeighbours() {
    if (written_.empty()) {
        return;
    }
    // parity: in strict mode none of this runs.
    if (strict_) {
        written_.clear();
        written_properties_.clear();
        replaced_.clear();
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
        // parity: the `water_evaporates` attribute, which `WetSpongeBlock.onPlace` reads.
        .evaporates = world_->declaration().dimension == config::WorldDimension::Nether,
        .hasSkylight = facts.hasSkylight,
        .ultrawarm = world_->declaration().dimension == config::WorldDimension::Nether,
        // parity: a `/setblock` of water is a block change like any other.
        .wakeFluids =
            [this, &random](std::int32_t x, std::int32_t y, std::int32_t z) {
                const world::fluidFlow::Context flow{.world = world_->state(),
                                                     .gameRules = world_->gameRules(),
                                                     .random = random,
                                                     .ultrawarm = world_->declaration().dimension ==
                                                                  config::WorldDimension::Nether};
                static_cast<void>(world::fluidFlow::onBlockChanged(flow, x, y, z));
            },
        .arrivedFluid =
            [this, &random](std::int32_t x, std::int32_t y, std::int32_t z) {
                const world::fluidFlow::Context flow{.world = world_->state(),
                                                     .gameRules = world_->gameRules(),
                                                     .random = random,
                                                     .ultrawarm = world_->declaration().dimension ==
                                                                  config::WorldDimension::Nether};
                world::fluidFlow::onArrived(flow, x, y, z);
            },
        // parity: `PotentSulfurBlock.updateShape` is `validBlockState`.
        .geyser =
            [this](std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId) {
                return world::geysers::validState(world_->state(), world_->gameTime(), x, y, z, stateId);
            },
        // parity: `CarvedPumpkinBlock.trySpawnGolem`.
        .summonGolem =
            [this](world::golemPatterns::Golem kind, double gx, double gy, double gz) {
                if (environment_->options().summonGolem) {
                    environment_->options().summonGolem(*world_, world::golemPatterns::entityTypeOf(kind), gx, gy, gz);
                }
            },
        // parity: `#minecraft:copper`, which is the copper golem's body.
        .isCopper =
            [this, members = std::optional<std::set<wire::common::Identifier>>{},
             asked = false](std::int32_t stateId) mutable {
                if (!asked) {
                    asked = true;
                    if (environment_->options().blockTagMembers) {
                        members = environment_->options().blockTagMembers(wire::common::Identifier::vanilla("copper"));
                    }
                }
                const wire::common::Identifier* block = reg::blockStateDefinitions::blockOf(stateId);
                return block != nullptr && members.has_value() && members->contains(*block);
            },
        .gameTime = world_->gameTime(),
        .tntExplodes = world_->gameRules().get(world::gameRules::kTntExplodes),
        // parity: `/setblock` and `/fill` write with `2 | 256`, no `UPDATE_NEIGHBORS`.
        .railArrivalTells = false};
    for (const std::array<std::int32_t, 3>& at : positions) {
        std::optional<std::int32_t> written = world_->state().blockState(at[0], at[1], at[2]);
        // parity: `BlockInput.place` settles the state before it writes it.
        if (written.has_value() && *written != reg::blocks::air().stateId) {
            const auto beforeShaping = replaced_.find(at);
            const bool hadEntry = beforeShaping != replaced_.end();
            const std::optional<std::int32_t> wasThere = hadEntry ? beforeShaping->second : std::nullopt;
            if (const std::optional<std::int32_t> shaped = neighbourUpdates::reshape(context, at[0], at[1], at[2]);
                shaped.has_value()) {
                if (*shaped == reg::blocks::air().stateId) {
                    static_cast<void>(world_->state().setBlockState(at[0], at[1], at[2], *written));
                } else {
                    static_cast<void>(set(at[0], at[1], at[2], *shaped));
                }
            }
            keepNamedProperties(at);
            written_properties_.erase(at);
            if (hadEntry) {
                replaced_.insert_or_assign(at, wasThere);
            } else {
                replaced_.erase(at);
            }
            written = world_->state().blockState(at[0], at[1], at[2]);
        }
        const auto replacedHere = replaced_.find(at);
        neighbourUpdates::settleArrivedSignal(
            context, at[0], at[1], at[2], replacedHere == replaced_.end() ? std::nullopt : replacedHere->second,
            [this](std::int32_t stateId, std::int32_t x, std::int32_t y, std::int32_t z) { record(x, y, z, stateId); });
        if (world_->state().blockState(at[0], at[1], at[2]) != written) {
            written_properties_.erase(at);
            continue;
        }
        // parity: `BlockInput.place` keeps what the command wrote when the **shape** update leaves air.
        bool shapedToAir = false;
        bool unsupported = false;
        const auto wasHere = replaced_.find(at);
        neighbourUpdates::Context broke = context;
        broke.railArrivalTells = true;
        neighbourUpdates::afterChange(
            destroyed_.contains(at) ? broke : context, at[0], at[1], at[2],
            wasHere == replaced_.end() ? std::nullopt : wasHere->second, effects_,
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
                // parity: `overwriteWithDefinedProperties` only beats the block's **own** shaping.
                if (x == at[0] && y == at[1] && z == at[2]) {
                    keepNamedProperties(at);
                    written_properties_.erase(at);
                }
            },
            neighbourUpdates::ArrivedShape::AlreadyShaped,
            // parity: `BlockInput.place` settles the state it is going to write.
            [this, &at, &written, &shapedToAir, &unsupported, &wasHere] {
                if (shapedToAir && !unsupported && written.has_value() && *written != reg::blocks::air().stateId &&
                    world_->state().blockState(at[0], at[1], at[2]) == reg::blocks::air().stateId) {
                    static_cast<void>(set(at[0], at[1], at[2], *written));
                }
                // The overwrite happens **once**, where vanilla does it: when the block lands.
                keepNamedProperties(at);
                written_properties_.erase(at);
                // parity: `onPlace` runs **after** `overwriteWithDefinedProperties`.
                if (const std::optional<std::int32_t> now = world_->state().blockState(at[0], at[1], at[2]);
                    now.has_value()) {
                    bendRail(at[0], at[1], at[2], *now, wasHere == replaced_.end() ? std::nullopt : wasHere->second);
                }
            });
        // parity: `Level.updateNeighboursOnBlockSet` ends in `updateNeighbourForOutputSignal` for a block that has.
        if (world::comparatorReading::at(world_->state(), at[0], at[1], at[2], {}).has_value()) {
            world::redstoneComponents::scheduleComparatorsAround(world_->state(), world_->name(), at[0], at[1], at[2],
                                                                 world::comparatorReading::of(world_->state()));
        }
    }
    written_properties_.clear();
    replaced_.clear();
    // A reaction can move a block of its own, and those go out with everything else.
    written_.clear();
}

void BlockEditor::keepNamedProperties(const std::array<std::int32_t, 3>& at) {
    // parity: `BlockInput.place` ends in `overwriteWithDefinedProperties`.
    const auto named = written_properties_.find(at);
    if (named == written_properties_.end() || named->second.empty()) {
        return;
    }
    const std::optional<std::int32_t> shaped = world_->state().blockState(at[0], at[1], at[2]);
    if (!shaped.has_value()) {
        return;
    }
    const wire::common::Identifier* block = reg::blockStateDefinitions::blockOf(*shaped);
    if (block == nullptr) {
        return;
    }
    if (const std::optional<std::int32_t> kept = reg::blockStateDefinitions::stateIdOf(*block, named->second, *shaped);
        kept.has_value() && *kept != *shaped) {
        static_cast<void>(set(at[0], at[1], at[2], *kept));
    }
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
                    player::sendBlockChangeTo(*player, packet.pos.x, packet.pos.y, packet.pos.z, packet.blockStateId);
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
                } else {
                    for (const auto& [position, stateId] : changes) {
                        player::sendBlockChangeTo(*player, (sectionX << 4) + ((position >> 8) & 15),
                                                  (sectionY << 4) + (position & 15),
                                                  (sectionZ << 4) + ((position >> 4) & 15), stateId);
                    }
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
