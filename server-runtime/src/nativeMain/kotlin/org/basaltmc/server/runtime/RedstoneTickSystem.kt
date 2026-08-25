package org.basaltmc.server.runtime

import org.basaltmc.server.plugins.PluginEventBus
import org.basaltmc.server.plugins.PluginEventTypes
import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.runtime.entity.Wardens
import org.basaltmc.server.runtime.player.PlayerRegistry
import org.basaltmc.server.runtime.player.PlayerSession
import org.basaltmc.server.runtime.plugins.emit
import org.basaltmc.server.runtime.sched.Region
import org.basaltmc.server.runtime.world.BlockEffect
import org.basaltmc.server.runtime.world.BlockEntityTicks
import org.basaltmc.server.runtime.world.BlockInteractions
import org.basaltmc.server.runtime.world.Crafters
import org.basaltmc.server.runtime.world.DetectorRails
import org.basaltmc.server.runtime.world.Dispensers
import org.basaltmc.server.runtime.world.FallingBlocks
import org.basaltmc.server.runtime.world.Fires
import org.basaltmc.server.runtime.world.FluidFlow
import org.basaltmc.server.runtime.world.GameRuleRegistry
import org.basaltmc.server.runtime.world.Lecterns
import org.basaltmc.server.runtime.world.Lightning
import org.basaltmc.server.runtime.world.LoadedWorld
import org.basaltmc.server.runtime.world.PressurePlates
import org.basaltmc.server.runtime.world.ProjectileHits
import org.basaltmc.server.runtime.world.RandomTicks
import org.basaltmc.server.runtime.world.RealRecipes
import org.basaltmc.server.runtime.world.Redstone
import org.basaltmc.server.runtime.world.RedstoneComponents
import org.basaltmc.server.runtime.world.RedstoneReactors
import org.basaltmc.server.runtime.world.ScheduledBlocks
import org.basaltmc.server.runtime.world.Tripwires
import org.basaltmc.server.runtime.world.Vibrations
import org.basaltmc.server.runtime.world.WorldState
import org.basaltmc.wire.protocol.play.BlockPos
import org.basaltmc.wire.protocol.play.ClientboundBlockUpdatePacket
import org.basaltmc.wire.protocol.play.Gamemode
import org.basaltmc.wire.protocol.play.SoundPacket
import org.basaltmc.wire.protocol.play.SoundSource
import kotlin.math.floor
import kotlin.random.Random
import kotlin.uuid.Uuid

/**
 * Everything that runs on the world's own clock rather than a player's: scheduled block ticks
 * (fire, redstone, pistons, buttons), fluids, random block ticks, block entities, falling blocks,
 * pressure plates, tripwires, footsteps and a chunk's inhabited time.
 */
public class RedstoneTickSystem(
    private val playerRegistry: PlayerRegistry,
    private val pluginEvents: PluginEventBus,
    private val recipes: RealRecipes,
    private val broadcaster: BlockChangeBroadcaster,
    private val chunkSimulation: ChunkSimulationView,
) {
    /**
     * Runs every fluid tick that came due and tells the players what moved.
     *
     * parity: `ServerLevel.tickFluid`, driven by the same per-chunk queue vanilla persists. A tick
     * is scheduled by whatever changed a block next to a fluid, so a settled ocean costs nothing.
     */
    public fun tickFluids(
        world: LoadedWorld,
        region: Region,
    ) {
        val due = world.state.dueFluidTicks()
        if (due.isEmpty()) return
        val watchers = playerRegistry.snapshot().filter { it.worldName == world.name }
        for (position in due) {
            if (!region.owns(position[0] shr 4, position[2] shr 4)) continue
            for (change in FluidFlow.tick(world, position[0], position[1], position[2])) {
                val pos = BlockPos(change[0], change[1], change[2])
                broadcaster.dropWashedAwayBlock(world, pos, change[4])
                for (player in watchers) {
                    player.connection.sendPlayPacket(ClientboundBlockUpdatePacket.ID) {
                        ClientboundBlockUpdatePacket(pos, change[3]).write(it)
                    }
                }
                // parity: `Level.setBlock` from the fluid tick updates the neighbours like any other
                // block change. Without it, water that washed a snow layer away left the grass under
                // it wearing its white top for ever, since nothing ever asked it to look again.
                broadcaster.notifyAround(world, watchers, pos)
            }
        }
    }

    /**
     * Runs the block ticks that have come due: buttons letting themselves back up, redstone
     * components spending their delay, pistons landing, plates coming back up.
     */
    public fun tickScheduledBlocks(
        world: LoadedWorld,
        region: Region,
    ) {
        val due = world.state.dueBlockTicks()
        if (due.isEmpty()) return
        val watchers = playerRegistry.snapshot().filter { it.worldName == world.name }
        for (position in due) {
            if (!region.owns(position[0] shr 4, position[2] shr 4)) continue
            // Fire acts on its own clock, and keeps asking for the next turn as it goes.
            if (Fires.isFire(world.state.getBlockState(position[0], position[1], position[2]) ?: 0)) {
                val spreading =
                    pluginEvents.emit(PluginEventTypes.BLOCK_IGNITE) { event ->
                        event.set(0, world.name)
                        event.set(1, position[0])
                        event.set(2, position[1])
                        event.set(3, position[2])
                        event.set(4, "spread")
                        event.set(5, null)
                    }
                if (spreading?.cancelled == true) continue
                for (change in Fires.tick(
                    world,
                    position[0],
                    position[1],
                    position[2],
                    LegacyRandom(Random.nextLong()),
                )) {
                    val pos = BlockPos(change.x, change.y, change.z)
                    for (player in watchers) {
                        player.connection.sendPlayPacket(ClientboundBlockUpdatePacket.ID) {
                            ClientboundBlockUpdatePacket(pos, change.stateId).write(it)
                        }
                    }
                }
                continue
            }
            // A block that lost its footing turns into an entity; everything else here is a button.
            if (FallingBlocks.falls(world.state.getBlockState(position[0], position[1], position[2]) ?: 0)) {
                val fell =
                    FallingBlocks.beginFalling(world, position[0], position[1], position[2]) {
                        playerRegistry.allocateEntityId()
                    }
                if (fell != null) {
                    val pos = BlockPos(position[0], position[1], position[2])
                    for (player in watchers) {
                        player.connection.sendPlayPacket(ClientboundBlockUpdatePacket.ID) {
                            ClientboundBlockUpdatePacket(pos, Blocks.AIR.stateId).write(it)
                        }
                    }
                }
                continue
            }
            val scheduled = mutableListOf<org.basaltmc.server.runtime.world.BlockChange>()
            ScheduledBlocks.frostedIce(
                world,
                position[0],
                position[1],
                position[2],
                LegacyRandom(Random.nextLong()),
                scheduled,
            )
            ScheduledBlocks.driedGhast(world, position[0], position[1], position[2], scheduled)
            // parity: `RedstoneTorchBlock.tick` and `DiodeBlock.tick`, which is where a component's
            // delay is actually spent. What it changes then settles the dust around it.
            // parity: `RedstoneLampBlock.tick`, the four ticks a lamp waits before going dark, and
            // `TntBlock`'s fuse.
            RedstoneReactors.lampTick(world, position[0], position[1], position[2])?.let { scheduled += it }
            // parity: `BasePressurePlateBlock.tick`, the turn a pressed plate books to come back up.
            if (PressurePlates.isPlate(world.state.getBlockState(position[0], position[1], position[2]) ?: 0)) {
                PressurePlates
                    .update(
                        world,
                        position[0],
                        position[1],
                        position[2],
                        standingOn(world, position[0], position[1], position[2], watchers),
                    )?.let { scheduled += it }
            }
            // parity: `TripWireBlock.tick`, the recheck a pressed wire books for itself.
            if (BlockStateDefinitions.blockOf(world.state.getBlockState(position[0], position[1], position[2]) ?: 0)
                == Tripwires.WIRE
            ) {
                val stillOn =
                    watchers.any {
                        Tripwires.isStandingOn(position[0], position[1], position[2], it.x, it.y, it.z)
                    } ||
                        world.state.items().any {
                            Tripwires.isStandingOn(position[0], position[1], position[2], it.x, it.y, it.z)
                        }
                Tripwires.press(world, position[0], position[1], position[2], stillOn, scheduled)
            }
            // parity: `LecternBlock.tick`, the two ticks a page turn pulses for.
            Lecterns.endPulse(world, position[0], position[1], position[2])?.let { scheduled += it }
            // parity: `DetectorRailBlock.tick`, the recheck a pressed rail books for itself.
            DetectorRails
                .check(world, position[0], position[1], position[2], world.state.entities())
                ?.let { scheduled += it }
            // parity: `SculkSensorBlock.tick`: thirty ticks awake, then ten of cooldown.
            Vibrations.tick(world, position[0], position[1], position[2])?.let { scheduled += it }
            // parity: `LightningRodBlock`'s own scheduled tick, eight after the strike.
            Lightning.releaseRod(world, position[0], position[1], position[2])?.let { scheduled += it }
            // parity: `TargetBlock`'s own scheduled tick, which lets the signal go again.
            ProjectileHits.releaseTarget(world, position[0], position[1], position[2])?.let { scheduled += it }
            // parity: `DispenserBlock.tick`, the four ticks after the signal arrived.
            scheduled += broadcaster.finishPiston(world, position, watchers)
            val dispenserEffects = mutableListOf<BlockEffect>()
            val dispensing =
                pluginEvents.emit(PluginEventTypes.BLOCK_DISPENSE) { event ->
                    event.set(0, world.name)
                    event.set(1, position[0])
                    event.set(2, position[1])
                    event.set(3, position[2])
                    event.set(4, null)
                }
            if (dispensing?.cancelled != true) {
                Dispensers.fire(
                    world,
                    position[0],
                    position[1],
                    position[2],
                    LegacyRandom(Random.nextLong()),
                    dispenserEffects,
                ) { playerRegistry.allocateEntityId() }
            }
            if (dispenserEffects.isNotEmpty()) {
                val stateId = world.state.getBlockState(position[0], position[1], position[2]) ?: 0
                broadcaster.sendEffects(watchers, position[0], position[1], position[2], stateId, dispenserEffects)
            }
            // parity: `CrafterBlock.tick`, four ticks after the signal arrived.
            Crafters
                .craft(
                    world,
                    position[0],
                    position[1],
                    position[2],
                    { input -> recipes.resultFor(input) },
                ) { playerRegistry.allocateEntityId() }
                ?.let { scheduled += it }
            RedstoneComponents.tick(world, position[0], position[1], position[2])?.let { moved ->
                scheduled += moved
                val settled = Redstone.settle(world, moved.x, moved.y, moved.z)
                scheduled += settled
                RedstoneComponents.scheduleAround(world, moved.x, moved.y, moved.z)
                // parity: `Level.updateNeighborsAt` after the component wrote its new state, which is
                // what lights the lamp on a repeater's output side: without it only the dust moved,
                // and everything a component feeds directly stayed as it was.
                scheduled += RedstoneReactors.update(world, moved.x, moved.y, moved.z)
                for (change in settled) scheduled += RedstoneReactors.update(world, change.x, change.y, change.z)
            }
            ScheduledBlocks.frogspawn(world, position[0], position[1], position[2], scheduled)
            org.basaltmc.server.runtime.world.Geysers
                .tick(world, position[0], position[1], position[2], scheduled)
            ScheduledBlocks.snifferEgg(
                world,
                position[0],
                position[1],
                position[2],
                LegacyRandom(Random.nextLong()),
                scheduled,
            )
            if (scheduled.isNotEmpty()) {
                for (change in scheduled) {
                    val pos = BlockPos(change.x, change.y, change.z)
                    for (player in watchers) {
                        player.connection.sendPlayPacket(ClientboundBlockUpdatePacket.ID) {
                            ClientboundBlockUpdatePacket(pos, change.stateId).write(it)
                        }
                    }
                }
                continue
            }
            val released = BlockInteractions.release(world, position[0], position[1], position[2]) ?: continue
            val pos = BlockPos(position[0], position[1], position[2])
            // parity: `ButtonBlock.tick` into `updateNeighbours`: a button coming back up has to
            // take the signal with it, or whatever it powered stays on for good.
            broadcaster.announceRedstone(
                world,
                watchers,
                org.basaltmc.server.runtime.world
                    .BlockChange(pos.x, pos.y, pos.z, released.stateId),
            )
            for (player in watchers) {
                player.connection.sendPlayPacket(ClientboundBlockUpdatePacket.ID) {
                    ClientboundBlockUpdatePacket(pos, released.stateId).write(it)
                }
                player.connection.sendPlayPacket(SoundPacket.ID) {
                    SoundPacket(
                        sound = released.sound,
                        source = SoundSource.BLOCKS,
                        x = pos.x + 0.5,
                        y = pos.y + 0.5,
                        z = pos.z + 0.5,
                        volume = 1f,
                        pitch = 1f,
                        seed = Random.nextLong(),
                    ).write(it)
                }
            }
        }
    }

    /**
     * Presses the plates people are standing on.
     *
     * parity: `BasePressurePlateBlock.entityInside`, which is a collision rather than a tick. Here
     * the players are walked once a tick instead, which reaches the same state on the same tick: a
     * plate is pressed while somebody is on it and books its own turn to come back up.
     */
    public fun tickPressurePlates(
        world: LoadedWorld,
        region: Region,
    ) {
        val watchers = playerRegistry.snapshot().filter { it.worldName == world.name }
        if (watchers.isEmpty()) return
        for (player in watchers) {
            if (player.gamemode == Gamemode.SPECTATOR) continue
            if (!region.ownsPosition(player.x, player.z)) continue
            val x = floor(player.x).toInt()
            val z = floor(player.z).toInt()
            val y = PressurePlates.blockUnder(player.y)
            val stateId = world.state.getBlockState(x, y, z) ?: continue
            if (!PressurePlates.isPlate(stateId)) continue
            if (!PressurePlates.isStandingOn(x, y, z, player.x, player.y, player.z)) continue
            val change = PressurePlates.update(world, x, y, z, standingOn(world, x, y, z, watchers)) ?: continue
            broadcaster.announceRedstone(world, watchers, change)
        }
    }

    /**
     * The footsteps a sculk sensor hears.
     *
     * parity: `GameEvent.STEP`, which vanilla raises as an entity moves. A player only emits one
     * when they have crossed into another block *and* a sensor is known to be near: the index is
     * what makes that affordable to ask every tick (see [WorldState.hasSculkSensorNear]).
     */
    public fun tickFootsteps(world: LoadedWorld) {
        val watchers = playerRegistry.snapshot().filter { it.worldName == world.name }
        if (watchers.isEmpty()) return
        val changes = mutableListOf<org.basaltmc.server.runtime.world.BlockChange>()
        for (player in watchers) {
            if (player.gamemode == Gamemode.SPECTATOR) continue
            if (!player.onGround) continue
            val block = pack(floor(player.x).toInt(), floor(player.y).toInt(), floor(player.z).toInt())
            if (lastFootstep[player.entityId] == block) continue
            lastFootstep[player.entityId] = block
            // parity: the warden's own `VibrationUser`, which hears a step sixteen blocks away and
            // gets angrier at whoever took it. Sneaking makes no vibration, so it hears nothing.
            if (!player.sneaking) hearFootstep(world, player)
            if (!world.state.hasSculkSensorNear(player.x, player.y, player.z, Vibrations.RANGE)) continue
            Vibrations.emit(world, "step", player.x, player.y, player.z, changes)
        }
        for (change in changes) broadcaster.announceRedstone(world, watchers, change)
    }

    /** parity: `Warden.increaseAngerAt` from a vibration: a step it hears is worth thirty-five. */
    private fun hearFootstep(
        world: LoadedWorld,
        player: PlayerSession,
    ) {
        for (entity in world.state.entities()) {
            if (entity.removed || !Wardens.isWarden(entity.type)) continue
            val dx = entity.x - player.x
            val dy = entity.y - player.y
            val dz = entity.z - player.z
            if (dx * dx + dy * dy + dz * dz > Wardens.HEARING_RANGE * Wardens.HEARING_RANGE) continue
            Wardens.anger(entity, player.entityId, Wardens.HEARD)
        }
    }

    /**
     * Presses the tripwires something is standing in.
     *
     * parity: `TripWireBlock.entityInside`, which is a collision in vanilla and a walk over the
     * players and dropped items here. What it powers is the hook at the end of the string.
     */
    public fun tickTripwires(
        world: LoadedWorld,
        region: Region,
    ) {
        val watchers = playerRegistry.snapshot().filter { it.worldName == world.name }
        val changes = mutableListOf<org.basaltmc.server.runtime.world.BlockChange>()
        val crossed = HashSet<Long>()
        for (player in watchers) {
            if (player.gamemode == Gamemode.SPECTATOR) continue
            if (!region.ownsPosition(player.x, player.z)) continue
            crossingsOf(world, player.x, player.y, player.z, crossed)
        }
        for (item in world.state.items()) {
            if (!region.ownsPosition(item.x, item.z)) continue
            crossingsOf(world, item.x, item.y, item.z, crossed)
        }
        for (packed in crossed) {
            Tripwires.press(
                world,
                unpackX(packed),
                unpackY(packed),
                unpackZ(packed),
                occupied = true,
                changes = changes,
            )
        }
        for (change in changes) broadcaster.announceRedstone(world, watchers, change)
    }

    /** The tripwire block a thing at these coordinates is standing in, if it is standing in one. */
    private fun crossingsOf(
        world: LoadedWorld,
        x: Double,
        y: Double,
        z: Double,
        into: MutableSet<Long>,
    ) {
        val blockX = floor(x).toInt()
        val blockZ = floor(z).toInt()
        for (blockY in floor(y - 0.1).toInt()..floor(y).toInt()) {
            val stateId = world.state.getBlockState(blockX, blockY, blockZ) ?: continue
            if (BlockStateDefinitions.blockOf(stateId) != Tripwires.WIRE) continue
            if (!Tripwires.isStandingOn(blockX, blockY, blockZ, x, y, z)) continue
            into += pack(blockX, blockY, blockZ)
        }
    }

    private fun pack(
        x: Int,
        y: Int,
        z: Int,
    ): Long = (x.toLong() and 0x3ffffff shl 38) or (z.toLong() and 0x3ffffff shl 12) or (y.toLong() and 0xfff)

    private fun unpackX(packed: Long): Int = (packed shl 0 shr 38).toInt()

    private fun unpackY(packed: Long): Int = (packed shl 52 shr 52).toInt()

    private fun unpackZ(packed: Long): Int = (packed shl 26 shr 38).toInt()

    /** How many things are on the plate at these coordinates, the way its own sensitivity counts. */
    private fun standingOn(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        watchers: List<PlayerSession>,
    ): Int {
        var count =
            watchers.count {
                it.gamemode != Gamemode.SPECTATOR && PressurePlates.isStandingOn(x, y, z, it.x, it.y, it.z)
            }
        // parity: the stone and blackstone plates are `MOBS`, so a dropped item does not press them.
        val path = BlockStateDefinitions.blockOf(world.state.getBlockState(x, y, z) ?: 0)?.path.orEmpty()
        if (path != "stone_pressure_plate" && path != "polished_blackstone_pressure_plate") {
            count += world.state.items().count { PressurePlates.isStandingOn(x, y, z, it.x, it.y, it.z) }
        }
        return count
    }

    /** Which block each player was last heard stepping in, so one step is one vibration. */
    private val lastFootstep = HashMap<Int, Long>()

    /** parity: `AbstractMinecart.getDefaultGravity`, the same 0.04 as anything else that is not alive. */
    public val minecartGravity: Double = 0.04

    /** The block ticks vanilla schedules at random per chunk: crops growing, ice melting and so on. */
    private val randomTicks = RandomTicks()

    public fun tickFallingBlocks(
        world: LoadedWorld,
        region: Region,
    ) {
        val falling =
            world.state.entities().filter {
                !it.removed && it.type == FallingBlocks.ENTITY_TYPE && region.ownsPosition(it.x, it.z)
            }
        if (falling.isEmpty()) return
        val watchers = playerRegistry.snapshot().filter { it.worldName == world.name }
        for (entity in falling) {
            val landed = FallingBlocks.tick(world, entity) ?: continue
            val pos = BlockPos(landed.x, landed.y, landed.z)
            for (player in watchers) {
                player.connection.sendPlayPacket(ClientboundBlockUpdatePacket.ID) {
                    ClientboundBlockUpdatePacket(pos, landed.stateId).write(it)
                }
            }
        }
    }

    /**
     * Lets the blocks vanilla picks at random act: crops grow, ice melts, cane climbs.
     *
     * parity: `ServerLevel.tickChunk`, which only runs over the chunks a player is near. Whoever is
     * in the world decides that here too, so an empty world sits still rather than growing on.
     */
    public fun tickRandomBlocks(
        world: LoadedWorld,
        region: Region,
    ) {
        val speed = world.gameRules[GameRuleRegistry.RANDOM_TICK_SPEED]
        if (speed <= 0) return
        val chunks = chunkSimulation.simulatedChunks(world, region)
        if (chunks.isEmpty()) return
        val changes =
            randomTicks.tick(
                world,
                chunks,
                speed,
                LegacyRandom(Random.nextLong()),
                allocateEntityId = { playerRegistry.allocateEntityId() },
                events = pluginEvents,
            )
        if (changes.isEmpty()) return
        val watchers = playerRegistry.snapshot().filter { it.worldName == world.name }
        for (change in changes) {
            // parity: a block entity that changed its own state is a block change like any other,
            // so its neighbours hear about it: a daylight detector that dropped to zero at dusk has
            // to take the dust beside it down with it.
            broadcaster.announceRedstone(
                world,
                watchers,
                org.basaltmc.server.runtime.world
                    .BlockChange(change.x, change.y, change.z, change.stateId),
            )
            // parity: `setBlock` again: what a random tick changed reshapes what is around it, which
            // is how the grass under a snow layer that melted stops wearing snow.
            broadcaster.notifyAround(world, watchers, BlockPos(change.x, change.y, change.z))
        }
    }

    /**
     * Lets the block entities that have something to do act: today the three that smelt.
     *
     * parity: `ServerLevel.tickBlockEntities`, over the same chunks a random tick visits.
     */
    public fun tickBlockEntities(
        world: LoadedWorld,
        region: Region,
    ) {
        val chunks = chunkSimulation.simulatedChunks(world, region)
        if (chunks.isEmpty()) return
        val changes =
            BlockEntityTicks.tick(
                world,
                chunks,
                { cooker, stack -> recipes.cookingFor(cooker, stack) },
                // parity: `CampfireBlockEntity.cookTick`, which drops what is done beside the fire.
                { done ->
                    world.state.spawnItem(
                        WorldState.DroppedItem(
                            entityId = playerRegistry.allocateEntityId(),
                            uuid = Uuid.random(),
                            item = done.stack.item,
                            count = done.stack.count,
                            x = done.x + 0.5,
                            y = done.y + 1.0,
                            z = done.z + 0.5,
                        ),
                    )
                },
                events = pluginEvents,
            )
        if (changes.isEmpty()) return
        val watchers = playerRegistry.snapshot().filter { it.worldName == world.name }
        for (change in changes) {
            val pos = BlockPos(change.x, change.y, change.z)
            for (player in watchers) {
                player.connection.sendPlayPacket(ClientboundBlockUpdatePacket.ID) {
                    ClientboundBlockUpdatePacket(pos, change.stateId).write(it)
                }
            }
        }
    }

    /** parity: every loaded chunk inside simulation distance advances its own local-difficulty age. */
    public fun tickInhabitedTime(
        world: LoadedWorld,
        region: Region,
    ) {
        for (position in chunkSimulation.simulatedChunks(world, region)) {
            world.state.chunkIfLoaded(position[0], position[1])?.increaseInhabitedTime()
        }
    }
}
