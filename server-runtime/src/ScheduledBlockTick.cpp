#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/BlockChangeBroadcaster.hpp>
#include <basalt/server/runtime/RedstoneTickSystem.hpp>
#include <basalt/server/runtime/entity/BlockTriggers.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/sched/RegionScheduler.hpp>
#include <basalt/server/runtime/world/ArrowButtons.hpp>
#include <basalt/server/runtime/world/Beds.hpp>
#include <basalt/server/runtime/world/BlockInteractions.hpp>
#include <basalt/server/runtime/world/BlockSurvival.hpp>
#include <basalt/server/runtime/world/CommandBlocks.hpp>
#include <basalt/server/runtime/world/ComparatorReading.hpp>
#include <basalt/server/runtime/world/Composters.hpp>
#include <basalt/server/runtime/world/Crafters.hpp>
#include <basalt/server/runtime/world/DetectorRails.hpp>
#include <basalt/server/runtime/world/Dispensers.hpp>
#include <basalt/server/runtime/world/Dripleaves.hpp>
#include <basalt/server/runtime/world/FallingBlocks.hpp>
#include <basalt/server/runtime/world/Fires.hpp>
#include <basalt/server/runtime/world/Geysers.hpp>
#include <basalt/server/runtime/world/Lecterns.hpp>
#include <basalt/server/runtime/world/Lightning.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/NeighbourUpdates.hpp>
#include <basalt/server/runtime/world/Pistons.hpp>
#include <basalt/server/runtime/world/PressurePlates.hpp>
#include <basalt/server/runtime/world/ProjectileHits.hpp>
#include <basalt/server/runtime/world/RealBlockTags.hpp>
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
#include <limits>

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
    const std::vector<world::WorldState::DueTick> due = world.state().dueBlockTicksTyped();
    if (due.empty()) {
        return;
    }
    const std::vector<PlayerSession*> watchers = watchersOf(world);
    const bool hasSkylight = config::dimensionFacts(world.declaration().dimension).hasSkylight;
    const world::redstoneComponents::ContainerSignalAt containerSignal = world::comparatorReading::of(world.state());
    for (const world::WorldState::DueTick& tick : due) {
        const std::array<std::int32_t, 3> position{tick.x, tick.y, tick.z};
        const std::int32_t x = position[0];
        const std::int32_t y = position[1];
        const std::int32_t z = position[2];
        // parity: `LevelTicks.runCollectedTicks` takes the turn out of the batch before running it,
        // so what the turn sets off books for real while everything still waiting does not.
        world.state().beginBlockTick(x, y, z);
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
        // parity: `LeavesBlock.tick`, which is where the distance really moves: the shape update only
        // books this turn.
        if (const std::optional<world::BlockChange> leaves = world::neighbourUpdates::leafTick(world.state(), x, y, z);
            leaves.has_value()) {
            // parity: `LeavesBlock.tick` writes with `setBlock(..., 3)`, so what is standing on the
            // leaves hears it and falls if it cannot hold on there.
            sendBlockUpdates(watchers, {*leaves});
            broadcaster_->notifyAround(world, watchers, x, y, z, std::nullopt);
        }
        world::scheduledBlocks::frostedIce(world.state(), x, y, z, random, scheduled);
        world::scheduledBlocks::driedGhast(world.state(), x, y, z, world.gameTime(), scheduled);
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
                                                  standingOn(world, x, y, z, watchers, world.state().items(),
                                                             std::numeric_limits<std::size_t>::max()),
                                                  world.gameTime());
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
            // parity: `TripWireBlock.checkPressed` asks the level for **every** entity in the wire's
            // own shape, so what keeps a string down is not only a player or an item.
            if (!stillOn) {
                for (const entity::ServerEntity* standing : world.state().entities()) {
                    if (!standing->removed && !entity::blockTriggers::ignored(standing->type()) &&
                        world::tripwires::isStandingOn(x, y, z, standing->x, standing->y, standing->z)) {
                        stillOn = true;
                        break;
                    }
                }
            }
            // `checkPressed` ends in `setBlock(..., 3)` for the wire and for every hook `updateSource`
            // reaches, so the line letting go has to settle the dust around it. Sending the squares
            // as packets left a lamp lit for good once whoever stepped on the string walked away.
            std::vector<world::BlockChange> pressed;
            world::tripwires::press(world.state(), x, y, z, stillOn, world.gameTime(), pressed);
            for (const world::BlockChange& moved : pressed) {
                broadcaster_->announceRedstone(world, watchers, moved);
            }
        }
        // parity: `TripWireHookBlock.tick`, the look the hook booked when a wire told it something.
        // Ten ticks later the string it was handed is gone for real, and the hook lets go.
        //
        // Every square `calculateState` writes goes out with flag 3 and the two hooks run
        // `notifyNeighbors` on top, so this settles the dust around what moved instead of only
        // redrawing it for the clients, which is what it used to do.
        if (here != 0 && world::tripwires::isHook(here)) {
            std::vector<world::BlockChange> hook;
            world::tripwires::calculateHook(world.state(), x, y, z, world.gameTime(), hook);
            for (const world::BlockChange& moved : hook) {
                broadcaster_->announceRedstone(world, watchers, moved);
            }
        }
        // parity: `ComposterBlock.tick`, the twenty ticks the seventh layer waits before it becomes
        // the bone meal, with the sound that says it is ready.
        if (std::optional<world::BlockChange> ready = world::composters::settle(world.state(), x, y, z);
            ready.has_value()) {
            ready->effects.push_back(world::blockEffect::Sound{
                .name = "block.composter.ready", .source = SoundSource::Blocks, .volume = 1.0F, .pitch = 1.0F});
            broadcaster_->announceBlock(watchers, *ready);
            broadcaster_->sendEffects(watchers, x, y, z, ready->stateId, ready->effects);
            // parity: `setBlock(..., 3)` tells the neighbours, which is what a comparator reading the
            // composter is waiting for.
            world::redstoneComponents::scheduleComparatorsAround(world.state(), world.name(), x, y, z, containerSignal);
        }
        // parity: `LecternBlock.tick`, the two ticks a page turn pulses for.
        if (const std::optional<world::BlockChange> lectern = world::lecterns::endPulse(world.state(), x, y, z);
            lectern.has_value()) {
            scheduled.push_back(*lectern);
        }
        // parity: `DetectorRailBlock.tick`, the recheck a pressed rail books for itself.
        if (const world::detectorRails::Checked rail = world::detectorRails::check(
                world.state(), x, y, z, world.state().entities(), world::detectorRails::Caller::ScheduledTick);
            rail.ran) {
            if (rail.change.has_value()) {
                scheduled.push_back(*rail.change);
            }
            // parity: `checkPressed` ends in `updateNeighbourForOutputSignal` whatever it decided, so
            // a comparator reading a container minecart parked here follows what is inside it.
            world::redstoneComponents::scheduleComparatorsAround(world.state(), world.name(), x, y, z, containerSignal);
        }
        // parity: `SculkSensorBlock.tick`: thirty ticks awake, then ten of cooldown, plus the turn
        // the vibration on its way spends. `activate` and `deactivate` both end in `updateNeighbours`,
        // so this goes out with the redstone settled around it and not as a bare block update:
        // measured in `sensormuerte`, where our sensor woke on the right tick and the comparator in
        // front of it never noticed.
        if (const std::optional<world::BlockChange> sensor =
                world::vibrations::tick(world.state(), x, y, z, world.gameTime());
            sensor.has_value()) {
            // parity: `SculkSensorBlock.updateNeighbours` tells the square below as well, which is
            // what `alsoTells` carries for every path that announces a sensor.
            broadcaster_->announceRedstone(world, watchers, *sensor);
        }
        // parity: `BigDripleafBlock.tick`, the stage it takes next: ten ticks each on the way down
        // and a hundred lying flat before it comes back up.
        if (const std::optional<world::BlockChange> leaf =
                world::dripleaves::tick(world.state(), world.name(), x, y, z, world.gameTime());
            leaf.has_value()) {
            scheduled.push_back(*leaf);
        }
        // parity: `LightningRodBlock.tick` writes with `UPDATE_ALL` and then runs `updateNeighbours`,
        // so letting go of the signal has to settle the dust around it and not only redraw the rod.
        // Measured in `pararrayos`, where the rod went quiet on the right tick and the dust beside it
        // stayed at fifteen for ever.
        if (const std::optional<world::BlockChange> rod = world::lightning::releaseRod(world.state(), x, y, z);
            rod.has_value()) {
            broadcaster_->announceRedstone(world, watchers, *rod);
        }
        // parity: `TargetBlock`'s own scheduled tick, which ends the same way: the signal going is a
        // block change like any other.
        if (const std::optional<world::BlockChange> target =
                world::projectileHits::releaseTarget(world.state(), x, y, z);
            target.has_value()) {
            broadcaster_->announceRedstone(world, watchers, *target);
        }
        // parity: `CommandBlock.tick`, the booked turn that runs the command and walks the chain on.
        // It ends in `updateNeighbourForOutputSignal`, which here is the comparators around it.
        if (world::commandBlocks::tick(
                world.state(), x, y, z, world.gameTime(), world.gameRules().get(world::gameRules::kCommandBlocksWork),
                world.gameRules().number(world::gameRules::kMaxCommandSequenceLength), runCommand_)) {
            world::redstoneComponents::scheduleComparatorsAround(world.state(), world.name(), x, y, z,
                                                                 world::comparatorReading::of(world.state()));
        }
        // parity: `DispenserBlock.tick`, the four ticks after the signal arrived.
        if (!hooks_.dispenseAllowed || hooks_.dispenseAllowed(world, x, y, z)) {
            const world::fluidFlow::Context context{.world = world.state(),
                                                    .gameRules = world.gameRules(),
                                                    .random = random,
                                                    .ultrawarm = world.declaration().dimension ==
                                                                 config::WorldDimension::Nether};
            const world::dispensers::Result result = world::dispensers::fire(
                context, x, y, z,
                {.allocateEntityId = [this] { return players_->allocateEntityId(); },
                 .isCopper =
                     [this](std::int32_t stateId) {
                         const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
                         return block != nullptr && options_.blockTags != nullptr &&
                                world::realBlockTags::contains(*options_.blockTags, "minecraft:copper", *block);
                     },
                 .recipeFor =
                     [this](server::world::crafting::Cooker cooker, const server::world::loot::ItemStack& stack) {
                         return recipes_ == nullptr ? nullptr : recipes_->cookingFor(cooker, stack);
                     },
                 .gameTime = world.gameTime(),
                 .convertsToMud =
                     [this](const wire::common::Identifier& block) {
                         return options_.blockTags != nullptr &&
                                world::realBlockTags::contains(*options_.blockTags, "minecraft:convertable_to_mud",
                                                               block);
                     }});
            if (!result.effects.empty()) {
                broadcaster_->sendEffects(watchers, x, y, z, world.state().blockState(x, y, z).value_or(0),
                                          result.effects);
            }
            // parity: every block a dispenser behaviour writes goes down with `Level.setBlock`'s
            // flag three, so the square it lands on tells its neighbours: the grass under a bucket
            // of powder snow is what wears the white top.
            for (const world::BlockChange& change : result.changes) {
                broadcaster_->announceRedstone(world, watchers, change);
                broadcaster_->notifyAround(world, watchers, change.x, change.y, change.z, std::nullopt);
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
            const std::vector<world::BlockChange> reacted = world::redstoneReactors::update(
                world.state(), world.name(), moved->x, moved->y, moved->z, world.gameTime(), random,
                world.gameRules().get(world::gameRules::kTntExplodes));
            scheduled.insert(scheduled.end(), reacted.begin(), reacted.end());
            // parity: a component writes with `UPDATE_ALL`, so a piston beside it runs its own
            // `neighborChanged`. Nothing else asks a piston, and without this a repeater feeding one
            // never made it move.
            const config::DimensionFacts& facts = config::dimensionFacts(world.declaration().dimension);
            const world::pistons::Context pistonContext{.world = world.state(),
                                                        .worldName = world.name(),
                                                        .minY = facts.chunkHeight.minY(),
                                                        .maxY = facts.chunkHeight.maxY(),
                                                        .random = random,
                                                        .gameTime = world.gameTime()};
            world::pistons::trigger(pistonContext, moved->x, moved->y, moved->z);
            // parity: `DispenserBlock.neighborChanged` is the other one nobody else asks: a dropper
            // fed by a repeater or by a piece of dust that just moved has to remember it was
            // triggered and book its four ticks.
            if (const std::optional<world::BlockChange> armed =
                    world::dispensers::onNeighbourChange(world.state(), world.name(), moved->x, moved->y, moved->z);
                armed.has_value()) {
                scheduled.push_back(*armed);
            }
            for (const server::registry::BlockFace face :
                 {server::registry::BlockFace::Down, server::registry::BlockFace::Up,
                  server::registry::BlockFace::North, server::registry::BlockFace::South,
                  server::registry::BlockFace::West, server::registry::BlockFace::East}) {
                world::pistons::trigger(pistonContext, moved->x + server::registry::stepX(face),
                                        moved->y + server::registry::stepY(face),
                                        moved->z + server::registry::stepZ(face));
                if (const std::optional<world::BlockChange> armed = world::dispensers::onNeighbourChange(
                        world.state(), world.name(), moved->x + server::registry::stepX(face),
                        moved->y + server::registry::stepY(face), moved->z + server::registry::stepZ(face));
                    armed.has_value()) {
                    scheduled.push_back(*armed);
                }
            }
            // parity: a torch flicking on its own turn writes with `UPDATE_ALL` and stops at its six
            // squares; only a diode and an observer reach past the block they feed, and only as far
            // as that block's own neighbours.
            for (const world::redstoneComponents::Reach& at :
                 world::redstoneComponents::outputReach(moved->stateId, moved->x, moved->y, moved->z)) {
                if (!at.skip.has_value()) {
                    continue;
                }
                world::redstoneComponents::scheduleInFront(world.state(), world.name(), at.x, at.y, at.z, *at.skip,
                                                           containerSignal);
                const std::vector<world::BlockChange> ahead = world::redstoneReactors::updateInFront(
                    world.state(), world.name(), at.x, at.y, at.z, *at.skip, world.gameTime(), random,
                    world.gameRules().get(world::gameRules::kTntExplodes));
                scheduled.insert(scheduled.end(), ahead.begin(), ahead.end());
            }
            for (const world::BlockChange& change : settled) {
                const std::vector<world::BlockChange> around = world::redstoneReactors::updateAroundWire(
                    world.state(), world.name(), change.x, change.y, change.z, world.gameTime(), random,
                    world.gameRules().get(world::gameRules::kTntExplodes));
                scheduled.insert(scheduled.end(), around.begin(), around.end());
                // parity: the dust reaches its own square and its six, and each of those runs an
                // `updateNeighborsAt` of its own, so what hears the news is **their** neighbours: a
                // piston stuck to the block the dust is standing on is two steps away and moves.
                for (const std::array<std::int32_t, 3>& at :
                     world::redstoneComponents::wireReach(change.x, change.y, change.z)) {
                    for (const server::registry::BlockFace face :
                         {server::registry::BlockFace::West, server::registry::BlockFace::East,
                          server::registry::BlockFace::Down, server::registry::BlockFace::Up,
                          server::registry::BlockFace::North, server::registry::BlockFace::South}) {
                        const std::int32_t nx = at[0] + server::registry::stepX(face);
                        const std::int32_t ny = at[1] + server::registry::stepY(face);
                        const std::int32_t nz = at[2] + server::registry::stepZ(face);
                        world::pistons::trigger(pistonContext, nx, ny, nz);
                        if (const std::optional<world::BlockChange> armed =
                                world::dispensers::onNeighbourChange(world.state(), world.name(), nx, ny, nz);
                            armed.has_value()) {
                            scheduled.push_back(*armed);
                        }
                        if (const std::optional<world::BlockChange> armed =
                                world::crafters::onNeighbourChange(world.state(), world.name(), nx, ny, nz);
                            armed.has_value()) {
                            scheduled.push_back(*armed);
                        }
                    }
                }
            }
        }
        world::scheduledBlocks::frogspawn(world.state(), x, y, z, scheduled);
        // parity: `SugarCaneBlock.tick` and `CactusBlock.tick` both end in
        // `level.destroyBlock(pos, true)`, and the `true` is the drop. What was standing there is
        // read before, because after it is air.
        const std::int32_t standing = world.state().blockState(x, y, z).value_or(0);
        const std::size_t before = scheduled.size();
        world::scheduledBlocks::breakIfStillLoose(
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
            // parity: `destroyBlock` writes air with `Level.setBlock`'s flag three, so the square
            // over it hears it go and books its own turn. Without this only the bottom of a tower
            // came down. Measured in `cactuscana`.
            broadcaster_->notifyAround(world, watchers, x, y, z, std::nullopt);
        }
        // parity: `FarmlandBlock.turnToDirt`, which writes with `setBlockAndUpdate` and raises a
        // `BLOCK_CHANGE`, so a sculk sensor beside a field hears it give way.
        if (const std::optional<std::int32_t> soil = world::scheduledBlocks::coveredSoil(world.state(), x, y, z);
            soil.has_value()) {
            scheduled.push_back(world::BlockChange{.x = x, .y = y, .z = z, .stateId = *soil});
            broadcaster_->notifyAround(world, watchers, x, y, z, std::nullopt);
            if (world.state().hasSculkSensorNear(x, y, z, world::vibrations::kCalibratedRange)) {
                world::vibrations::emit(world.state(), "block_change", static_cast<double>(x) + 0.5,
                                        static_cast<double>(y) + 0.5, static_cast<double>(z) + 0.5, world.gameTime());
            }
        }
        // parity: `LiquidBlock.tick` and `BubbleColumnBlock.tick`, both of which end in
        // `BubbleColumnBlock.updateColumn`: the column is built, carried on or taken apart here and
        // not inside the shape cascade. Measured in `columnaburbujas`.
        world::scheduledBlocks::bubbleColumn(world.state(), x, y, z, scheduled);
        // parity: the coral turn, which writes with flag two: it goes white without telling anybody.
        if (const std::optional<std::int32_t> dead = world::scheduledBlocks::coral(world.state(), x, y, z);
            dead.has_value()) {
            scheduled.push_back(world::BlockChange{.x = x, .y = y, .z = z, .stateId = *dead});
        }
        // parity: `CreakingHeartBlock.tick` over `updateState`, written with `setBlock(..., 3)`. The
        // jar asks an environment attribute whether the creaking is active; here that is the night,
        // which is what the vanilla timeline says at noon and at midnight.
        if (const std::optional<std::int32_t> heart = world::scheduledBlocks::creakingHeartTick(
                world.state(), x, y, z,
                [this](std::int32_t stateId) {
                    const wire::common::Identifier* block = server::registry::blockStateDefinitions::blockOf(stateId);
                    return block != nullptr && options_.blockTags != nullptr &&
                           world::realBlockTags::contains(*options_.blockTags, "minecraft:pale_oak_logs", *block);
                },
                world::beds::isNight(world.clock().totalTicks()));
            heart.has_value()) {
            scheduled.push_back(world::BlockChange{.x = x, .y = y, .z = z, .stateId = *heart});
            broadcaster_->notifyAround(world, watchers, x, y, z, std::nullopt);
        }
        // parity: `SpeleothemBlock.tick`: a stalagmite that lost its footing breaks where it stands,
        // and a stalactite that lost its anchor comes down whole, every piece of the column an
        // entity of its own, down to the tip. Measured in `estalactita`.
        const bool speleothemSupported = world::blockSurvival::isSupported(
            world.state(), x, y, z, std::nullopt,
            [&world](const world::WorldState&, std::int32_t sx, std::int32_t sy, std::int32_t sz) {
                return world::scheduledBlocks::scaffoldingHasFallen(world.state(), sx, sy, sz);
            },
            hasSkylight);
        const world::scheduledBlocks::SpeleothemTurn spike =
            world::scheduledBlocks::speleothemTick(world.state(), x, y, z, speleothemSupported);
        if (spike.destroys) {
            const std::int32_t was = world.state().blockState(x, y, z).value_or(0);
            world.state().setBlockState(x, y, z, server::registry::blocks::air().stateId);
            broadcaster_->dropBrokenBlock(world, x, y, z, was);
            scheduled.push_back(
                world::BlockChange{.x = x, .y = y, .z = z, .stateId = server::registry::blocks::air().stateId});
            broadcaster_->notifyAround(world, watchers, x, y, z, std::nullopt);
        } else if (spike.falls) {
            for (std::int32_t at = y; world::scheduledBlocks::isStalactiteAt(world.state(), x, at, z); --at) {
                const bool tip = world::scheduledBlocks::isStalactiteTipAt(world.state(), x, at, z);
                static_cast<void>(world::fallingBlocks::fallFrom(world.state(), x, at, z,
                                                                 [this] { return players_->allocateEntityId(); }));
                scheduled.push_back(
                    world::BlockChange{.x = x, .y = at, .z = z, .stateId = server::registry::blocks::air().stateId});
                if (tip) {
                    break;
                }
            }
        }
        // parity: `ScaffoldingBlock.tick`, the turn the shape update booked: it counts the distance
        // out again and only then decides whether the piece stays, breaks or falls. Both endings
        // write with `Level.setBlock`'s flag three, so the piece beside it books its own turn and
        // the count walks out one square per tick. Measured in `andamio`.
        const world::scheduledBlocks::ScaffoldingTurn scaffold =
            world::scheduledBlocks::scaffoldingTick(world.state(), x, y, z);
        if (scaffold.reshaped.has_value()) {
            scheduled.push_back(world::BlockChange{.x = x, .y = y, .z = z, .stateId = *scaffold.reshaped});
            broadcaster_->notifyAround(world, watchers, x, y, z, std::nullopt);
        } else if (scaffold.falls || scaffold.breaks) {
            const std::int32_t was = world.state().blockState(x, y, z).value_or(0);
            if (scaffold.falls) {
                static_cast<void>(world::fallingBlocks::fallFrom(world.state(), x, y, z,
                                                                 [this] { return players_->allocateEntityId(); }));
            } else {
                world.state().setBlockState(x, y, z, server::registry::blocks::air().stateId);
                broadcaster_->dropBrokenBlock(world, x, y, z, was);
            }
            scheduled.push_back(
                world::BlockChange{.x = x, .y = y, .z = z, .stateId = server::registry::blocks::air().stateId});
            broadcaster_->notifyAround(world, watchers, x, y, z, std::nullopt);
        }
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
                if (const wire::common::Identifier* button = server::registry::blockStateDefinitions::blockOf(here);
                    button != nullptr) {
                    world.state().scheduleBlockTick(x, y, z, *button, *pressed);
                }
            }
            continue;
        }
        const std::optional<world::blockInteractions::BlockRelease> released =
            world::blockInteractions::release(world.state(), x, y, z);
        if (!released.has_value()) {
            continue;
        }
        // parity: the `BLOCK_DEACTIVATE` of `checkPressed`, which is the arrow's button letting go.
        if (world.state().hasSculkSensorNear(x, y, z, world::vibrations::kCalibratedRange)) {
            world::vibrations::emit(world.state(), "block_deactivate", x + 0.5, y + 0.5, z + 0.5, world.gameTime());
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
    // parity: `LevelTicks.cleanupAfterTick`. Nothing waits in the batch once the pass is over, so a
    // world that stops between ticks books like any other.
    world.state().endBlockTicks();
}

} // namespace basalt::server::runtime
