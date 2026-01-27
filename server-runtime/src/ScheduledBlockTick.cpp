#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/BlockChangeBroadcaster.hpp>
#include <basalt/server/runtime/RedstoneTickSystem.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/sched/RegionScheduler.hpp>
#include <basalt/server/runtime/world/ArrowButtons.hpp>
#include <basalt/server/runtime/world/BlockInteractions.hpp>
#include <basalt/server/runtime/world/BlockSurvival.hpp>
#include <basalt/server/runtime/world/ComparatorReading.hpp>
#include <basalt/server/runtime/world/Crafters.hpp>
#include <basalt/server/runtime/world/DetectorRails.hpp>
#include <basalt/server/runtime/world/Dispensers.hpp>
#include <basalt/server/runtime/world/FallingBlocks.hpp>
#include <basalt/server/runtime/world/Fires.hpp>
#include <basalt/server/runtime/world/Geysers.hpp>
#include <basalt/server/runtime/world/Lecterns.hpp>
#include <basalt/server/runtime/world/Lightning.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/PressurePlates.hpp>
#include <basalt/server/runtime/world/ProjectileHits.hpp>
#include <basalt/server/runtime/world/RealRecipes.hpp>
#include <basalt/server/runtime/world/Redstone.hpp>
#include <basalt/server/runtime/world/RedstoneComponents.hpp>
#include <basalt/server/runtime/world/RedstoneReactors.hpp>
#include <basalt/server/runtime/world/ScheduledBlocks.hpp>
#include <basalt/server/runtime/world/Tripwires.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>
#include <basalt/wire/protocol/play/HudPackets.hpp>
#include <basalt/wire/protocol/play/WorldPackets.hpp>

#include <algorithm>
#include <cstdint>

namespace basalt::server::runtime {
namespace {

using player::PlayerSession;
using wire::protocol::play::BlockPos;
using wire::protocol::play::ClientboundBlockUpdatePacket;
using wire::protocol::play::SoundPacket;
using wire::protocol::play::SoundSource;

void sendBlockUpdates(const std::vector<PlayerSession*>& watchers, const std::vector<world::BlockChange>& changes) {
    for (const world::BlockChange& change : changes) {
        for (PlayerSession* player : watchers) {
            if (!player->connection().sendPlayPacket) {
                continue;
            }
            player->connection().sendPlayPacket(ClientboundBlockUpdatePacket::kId, [&](wire::io::ByteBuffer& buffer) {
                ClientboundBlockUpdatePacket{.pos = BlockPos{change.x, change.y, change.z},
                                             .blockStateId = change.stateId}
                    .write(buffer);
            });
        }
    }
}

} // namespace

void RedstoneTickSystem::tickScheduledBlocks(world::LoadedWorld& world, const sched::Region& region) {
    const std::vector<std::array<std::int32_t, 3>> due = world.state().dueBlockTicks();
    if (due.empty()) {
        return;
    }
    const std::vector<PlayerSession*> watchers = watchersOf(world);
    const bool hasSkylight = config::dimensionFacts(world.declaration().dimension).hasSkylight;
    const world::redstoneComponents::ContainerSignalAt containerSignal = world::comparatorReading::of(world.state());
    for (const std::array<std::int32_t, 3>& position : due) {
        const std::int32_t x = position[0];
        const std::int32_t y = position[1];
        const std::int32_t z = position[2];
        if (!region.owns(x >> 4, z >> 4)) {
            continue;
        }
        const std::int32_t here = world.state().blockState(x, y, z).value_or(0);
        // Fire acts on its own clock, and keeps asking for the next turn as it goes.
        if (world::fires::isFire(here)) {
            if (hooks_.igniteAllowed && !hooks_.igniteAllowed(world, x, y, z, "spread")) {
                continue;
            }
            random::RandomSource random = freshRandom();
            sendBlockUpdates(watchers, world::fires::tick(world.state(), world.weather(), world.difficulty(), x, y, z,
                                                          random, {}, hasSkylight));
            continue;
        }
        // A block that lost its footing turns into an entity; everything else here is a button.
        if (world::fallingBlocks::falls(here)) {
            const entity::ServerEntity* fell = world::fallingBlocks::beginFalling(
                world.state(), x, y, z, [this] { return players_->allocateEntityId(); });
            if (fell != nullptr) {
                sendBlockUpdates(
                    watchers,
                    {world::BlockChange{.x = x, .y = y, .z = z, .stateId = server::registry::blocks::air().stateId}});
            }
            continue;
        }
        std::vector<world::BlockChange> scheduled;
        random::RandomSource random = freshRandom();
        world::scheduledBlocks::frostedIce(world.state(), x, y, z, random, scheduled);
        world::scheduledBlocks::driedGhast(world.state(), x, y, z, scheduled);
        // parity: `RedstoneLampBlock.tick`, the four ticks a lamp waits before going dark.
        if (const std::optional<world::BlockChange> lamp =
                world::redstoneReactors::lampTick(world.state(), world.name(), x, y, z);
            lamp.has_value()) {
            scheduled.push_back(*lamp);
        }
        // parity: `BasePressurePlateBlock.tick`, the turn a pressed plate books to come back up.
        if (world::pressurePlates::isPlate(here)) {
            if (const std::optional<world::BlockChange> plate =
                    world::pressurePlates::update(world.state(), x, y, z, world::pressurePlates::Trigger::Scheduled,
                                                  standingOn(world, x, y, z, watchers));
                plate.has_value()) {
                scheduled.push_back(*plate);
            }
        }
        // parity: `TripWireBlock.tick`, the recheck a pressed wire books for itself.
        const wire::common::Identifier* block = server::registry::blockStateDefinitions::blockOf(here);
        if (block != nullptr && *block == world::tripwires::wireBlock()) {
            bool stillOn = false;
            for (const PlayerSession* player : watchers) {
                if (world::tripwires::isStandingOn(x, y, z, player->x, player->y, player->z)) {
                    stillOn = true;
                    break;
                }
            }
            if (!stillOn) {
                for (const world::DroppedItem& item : world.state().items()) {
                    if (world::tripwires::isStandingOn(x, y, z, item.x, item.y, item.z)) {
                        stillOn = true;
                        break;
                    }
                }
            }
            world::tripwires::press(world.state(), x, y, z, stillOn, scheduled);
        }
        // parity: `LecternBlock.tick`, the two ticks a page turn pulses for.
        if (const std::optional<world::BlockChange> lectern = world::lecterns::endPulse(world.state(), x, y, z);
            lectern.has_value()) {
            scheduled.push_back(*lectern);
        }
        // parity: `DetectorRailBlock.tick`, the recheck a pressed rail books for itself.
        if (const std::optional<world::BlockChange> rail =
                world::detectorRails::check(world.state(), x, y, z, world.state().entities());
            rail.has_value()) {
            scheduled.push_back(*rail);
        }
        // parity: `SculkSensorBlock.tick`: thirty ticks awake, then ten of cooldown, plus the turn
        // the vibration on its way spends. `activate` and `deactivate` both end in `updateNeighbours`,
        // so this goes out with the redstone settled around it and not as a bare block update:
        // measured in `sensormuerte`, where our sensor woke on the right tick and the comparator in
        // front of it never noticed.
        if (const std::optional<world::BlockChange> sensor =
                world::vibrations::tick(world.state(), x, y, z, world.gameTime());
            sensor.has_value()) {
            broadcaster_->announceRedstone(world, watchers, *sensor);
            // parity: `SculkSensorBlock.updateNeighbours` tells the square **below** as well. A
            // sensor only feeds strongly upwards, so what is under it would never hear it otherwise.
            random::RandomSource sensorRandom = freshRandom();
            const std::vector<world::BlockChange> under =
                world::redstoneReactors::update(world.state(), world.name(), x, y - 1, z, sensorRandom);
            scheduled.insert(scheduled.end(), under.begin(), under.end());
        }
        // parity: `LightningRodBlock`'s own scheduled tick, eight after the strike.
        if (const std::optional<world::BlockChange> rod = world::lightning::releaseRod(world.state(), x, y, z);
            rod.has_value()) {
            scheduled.push_back(*rod);
        }
        // parity: `TargetBlock`'s own scheduled tick, which lets the signal go again.
        if (const std::optional<world::BlockChange> target =
                world::projectileHits::releaseTarget(world.state(), x, y, z);
            target.has_value()) {
            scheduled.push_back(*target);
        }
        const std::vector<world::BlockChange> piston = broadcaster_->finishPiston(world, position, watchers);
        scheduled.insert(scheduled.end(), piston.begin(), piston.end());
        // parity: `DispenserBlock.tick`, the four ticks after the signal arrived.
        if (!hooks_.dispenseAllowed || hooks_.dispenseAllowed(world, x, y, z)) {
            const world::fluidFlow::Context context{.world = world.state(),
                                                    .gameRules = world.gameRules(),
                                                    .random = random,
                                                    .ultrawarm = world.declaration().dimension ==
                                                                 config::WorldDimension::Nether};
            const world::dispensers::Result result = world::dispensers::fire(
                context, x, y, z, {.allocateEntityId = [this] { return players_->allocateEntityId(); }});
            if (!result.effects.empty()) {
                broadcaster_->sendEffects(watchers, x, y, z, world.state().blockState(x, y, z).value_or(0),
                                          result.effects);
            }
        }
        // parity: `CrafterBlock.tick`, four ticks after the signal arrived.
        if (const std::optional<world::BlockChange> crafted = world::crafters::craft(
                world.state(), x, y, z,
                {.resultFor =
                     [this](const server::world::crafting::CraftingInput& input) { return recipes_->resultFor(input); },
                 .allocateEntityId = [this] { return players_->allocateEntityId(); }});
            crafted.has_value()) {
            scheduled.push_back(*crafted);
        }
        // parity: `RedstoneTorchBlock.tick` and `DiodeBlock.tick`, which is where a component's delay
        // is actually spent. What it changes then settles the dust around it.
        if (const std::optional<world::BlockChange> moved = world::redstoneComponents::tick(
                world.state(), world.name(), x, y, z, world.gameTime(), containerSignal);
            moved.has_value()) {
            // A comparator whose level moved without its state moving answers with the state it
            // already had: there is nothing to send, only neighbours to tell.
            if (moved->stateId != here) {
                scheduled.push_back(*moved);
            }
            const std::vector<world::BlockChange> settled =
                world::redstone::settle(world.state(), world.name(), moved->x, moved->y, moved->z);
            scheduled.insert(scheduled.end(), settled.begin(), settled.end());
            world::redstoneComponents::scheduleAround(world.state(), world.name(), moved->x, moved->y, moved->z, false,
                                                      containerSignal);
            // parity: writing its own state still runs the shape update on the six squares around it,
            // and that is what locks the repeater beside a repeater that just turned on.
            const std::vector<world::BlockChange> held =
                world::redstoneComponents::relockAround(world.state(), moved->x, moved->y, moved->z);
            scheduled.insert(scheduled.end(), held.begin(), held.end());
            // parity: every piece of dust that moved reaches two squares out, which is what tells the
            // next repeater in a chain that the dust in between lit up.
            for (const world::BlockChange& change : settled) {
                world::redstoneComponents::scheduleAroundWire(world.state(), world.name(), change.x, change.y, change.z,
                                                              containerSignal);
            }
            // parity: a component writes its own state and the shapes around it are updated, which
            // is what an observer watching a repeater or another observer is waiting for.
            world::redstoneComponents::watchFor(world.state(), moved->x, moved->y, moved->z);
            for (const world::BlockChange& change : settled) {
                world::redstoneComponents::watchFor(world.state(), change.x, change.y, change.z);
            }
            // parity: `Level.updateNeighborsAt` after the component wrote its new state, which is
            // what lights the lamp on a repeater's output side.
            const std::vector<world::BlockChange> reacted =
                world::redstoneReactors::update(world.state(), world.name(), moved->x, moved->y, moved->z, random);
            scheduled.insert(scheduled.end(), reacted.begin(), reacted.end());
            for (const world::BlockChange& change : settled) {
                const std::vector<world::BlockChange> around = world::redstoneReactors::updateAroundWire(
                    world.state(), world.name(), change.x, change.y, change.z, random);
                scheduled.insert(scheduled.end(), around.begin(), around.end());
            }
        }
        world::scheduledBlocks::frogspawn(world.state(), x, y, z, scheduled);
        // parity: `SugarCaneBlock.tick` ends in `level.destroyBlock(pos, true)`, and the `true` is
        // the drop. What was standing there is read before, because after it is air.
        const std::int32_t standing = world.state().blockState(x, y, z).value_or(0);
        const std::size_t before = scheduled.size();
        world::scheduledBlocks::sugarCane(
            world.state(), x, y, z,
            world::blockSurvival::isSupported(
                world.state(), x, y, z, std::nullopt,
                [&world](const world::WorldState&, std::int32_t sx, std::int32_t sy, std::int32_t sz) {
                    return world::scheduledBlocks::scaffoldingHasFallen(world.state(), sx, sy, sz);
                },
                hasSkylight),
            scheduled);
        if (scheduled.size() != before) {
            broadcaster_->dropBrokenBlock(world, x, y, z, standing);
        }
        world::geysers::tick(world.state(), world.metadata().seed, world.gameTime(), x, y, z, scheduled);
        world::scheduledBlocks::snifferEgg(world.state(), x, y, z, random, scheduled);
        if (!scheduled.empty()) {
            sendBlockUpdates(watchers, scheduled);
            continue;
        }
        // parity: `ButtonBlock.checkPressed` looks again on its own turn, and an arrow still stuck in
        // the button keeps it down and books another turn instead of letting it up.
        if (world::arrowButtons::heldByArrow(world.state(), x, y, z, here)) {
            if (const std::optional<std::int32_t> pressed = world::blockInteractions::pressedTicks(here);
                pressed.has_value()) {
                world.state().scheduleBlockTick(x, y, z, wire::common::Identifier::vanilla("button"), *pressed);
            }
            continue;
        }
        const std::optional<world::blockInteractions::BlockRelease> released =
            world::blockInteractions::release(world.state(), x, y, z);
        if (!released.has_value()) {
            continue;
        }
        // parity: `ButtonBlock.tick` into `updateNeighbours`: a button coming back up has to take
        // the signal with it, or whatever it powered stays on for good.
        broadcaster_->announceRedstone(world, watchers,
                                       world::BlockChange{.x = x, .y = y, .z = z, .stateId = released->stateId});
        // parity: the same `updateNeighbours` the press did, so the wall the button is on hears it
        // come back up too.
        if (const std::optional<std::array<std::int32_t, 3>> wall =
                world::blockInteractions::attachmentOf(released->stateId, x, y, z);
            wall.has_value()) {
            broadcaster_->announceRedstone(
                world, watchers,
                world::BlockChange{.x = (*wall)[0],
                                   .y = (*wall)[1],
                                   .z = (*wall)[2],
                                   .stateId =
                                       world.state().blockState((*wall)[0], (*wall)[1], (*wall)[2]).value_or(0)});
        }
        sendBlockUpdates(watchers, {world::BlockChange{.x = x, .y = y, .z = z, .stateId = released->stateId}});
        for (PlayerSession* player : watchers) {
            if (!player->connection().sendPlayPacket) {
                continue;
            }
            player->connection().sendPlayPacket(SoundPacket::kId, [&](wire::io::ByteBuffer& buffer) {
                SoundPacket{.sound = released->sound,
                            .source = SoundSource::Blocks,
                            .x = x + 0.5,
                            .y = y + 0.5,
                            .z = z + 0.5,
                            .volume = 1.0F,
                            .pitch = 1.0F,
                            .seed = options_.nextSeed ? options_.nextSeed() : 0}
                    .write(buffer);
            });
        }
    }
}

} // namespace basalt::server::runtime
