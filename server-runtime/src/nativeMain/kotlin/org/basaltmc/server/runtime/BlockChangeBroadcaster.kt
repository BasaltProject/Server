package org.basaltmc.server.runtime

import org.basaltmc.server.plugins.PluginEventBus
import org.basaltmc.server.plugins.PluginEventTypes
import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.registry.BlockSupport
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.registry.Items
import org.basaltmc.server.runtime.player.PlayerRegistry
import org.basaltmc.server.runtime.player.PlayerSession
import org.basaltmc.server.runtime.plugins.emit
import org.basaltmc.server.runtime.world.BlockChange
import org.basaltmc.server.runtime.world.BlockEffect
import org.basaltmc.server.runtime.world.FluidFlow
import org.basaltmc.server.runtime.world.FluidKind
import org.basaltmc.server.runtime.world.LoadedWorld
import org.basaltmc.server.runtime.world.NeighbourUpdates
import org.basaltmc.server.runtime.world.Pistons
import org.basaltmc.server.runtime.world.RealLootTables
import org.basaltmc.server.runtime.world.Redstone
import org.basaltmc.server.runtime.world.RedstoneComponents
import org.basaltmc.server.runtime.world.RedstoneReactors
import org.basaltmc.server.runtime.world.WorldState
import org.basaltmc.wire.protocol.play.BlockEventPacket
import org.basaltmc.wire.protocol.play.BlockPos
import org.basaltmc.wire.protocol.play.ClientboundBlockUpdatePacket
import org.basaltmc.wire.protocol.play.LevelEventPacket
import org.basaltmc.wire.protocol.play.SoundPacket
import kotlin.math.abs
import kotlin.random.Random
import kotlin.uuid.Uuid

/**
 * Everything a block change owes the clients watching it: the update itself, the sound or particle
 * that rides along with it, and what redstone and pistons do because of it.
 */
public class BlockChangeBroadcaster(
    private val playerRegistry: PlayerRegistry,
    private val lootTables: RealLootTables,
    private val pluginEvents: PluginEventBus,
    /** How far from a piston a player can be and still be in the way of what it pushes. */
    private val pistonReach: Double = 14.0,
) {
    /** The neighbour update any block written by the world itself owes the blocks around it. */
    public fun notifyAround(
        world: LoadedWorld,
        watchers: List<PlayerSession>,
        pos: BlockPos,
    ) {
        NeighbourUpdates.afterChange(
            world,
            pos.x,
            pos.y,
            pos.z,
            onReshaped = { stateId, x, y, z ->
                for (player in watchers) {
                    player.connection.sendPlayPacket(ClientboundBlockUpdatePacket.ID) {
                        ClientboundBlockUpdatePacket(BlockPos(x, y, z), stateId).write(it)
                    }
                }
            },
            reshapeSelf = false,
        ) { stateId, x, y, z ->
            val at = BlockPos(x, y, z)
            for (player in watchers) {
                player.connection.sendPlayPacket(ClientboundBlockUpdatePacket.ID) {
                    ClientboundBlockUpdatePacket(at, Blocks.AIR.stateId).write(it)
                }
            }
            dropWashedAwayBlock(world, at, stateId)
        }
    }

    /**
     * One block, its new state, and whatever it asked the clients to hear or see while they are at
     * it: a note, a bell, a door swinging, a dispenser clicking at nothing.
     */
    public fun announceBlock(
        watchers: List<PlayerSession>,
        change: BlockChange,
    ) {
        val pos = BlockPos(change.x, change.y, change.z)
        for (player in watchers) {
            player.connection.sendPlayPacket(ClientboundBlockUpdatePacket.ID) {
                ClientboundBlockUpdatePacket(pos, change.stateId).write(it)
            }
        }
        if (change.effects.isEmpty()) return
        sendEffects(watchers, change.x, change.y, change.z, change.stateId, change.effects)
    }

    /** The effect half on its own, for the blocks that make a noise without changing state. */
    public fun sendEffects(
        watchers: List<PlayerSession>,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
        effects: List<BlockEffect>,
    ) {
        val pos = BlockPos(x, y, z)
        for (effect in effects) {
            when (effect) {
                is BlockEffect.Sound -> {
                    val sound =
                        SoundPacket(
                            sound = effect.name,
                            source = effect.source,
                            x = x + 0.5,
                            y = y + 0.5,
                            z = z + 0.5,
                            volume = effect.volume,
                            pitch = effect.pitch,
                            seed = Random.nextLong(),
                        )
                    for (player in watchers) {
                        player.connection.sendPlayPacket(SoundPacket.ID) { sound.write(it) }
                    }
                }

                is BlockEffect.Event -> {
                    val blockId = BlockSupport.blockRegistryIdOf(stateId)
                    for (player in watchers) {
                        player.connection.sendPlayPacket(BlockEventPacket.ID) {
                            BlockEventPacket(pos, effect.b0, effect.b1, blockId).write(it)
                        }
                    }
                }

                is BlockEffect.Effect -> {
                    for (player in watchers) {
                        player.connection.sendPlayPacket(LevelEventPacket.ID) {
                            LevelEventPacket(effect.type, pos, effect.data).write(it)
                        }
                    }
                }
            }
        }
    }

    /** Sends a block that redstone moved to everyone watching, and lets its neighbours react. */
    public fun announceRedstone(
        world: LoadedWorld,
        watchers: List<PlayerSession>,
        change: BlockChange,
    ) {
        announceBlock(watchers, change)
        val settled = Redstone.settle(world, change.x, change.y, change.z)
        for (moved in settled) announceBlock(watchers, moved)
        for (reacted in RedstoneReactors.update(world, change.x, change.y, change.z)) {
            announceBlock(watchers, reacted)
        }
        // parity: every piece of dust that moved tells its own neighbours, so a lamp at the end of
        // the line lights rather than waiting for something to touch it.
        for (moved in settled) {
            for (reacted in RedstoneReactors.update(world, moved.x, moved.y, moved.z)) {
                announceBlock(watchers, reacted)
            }
        }
        RedstoneComponents.scheduleAround(world, change.x, change.y, change.z)
    }

    /**
     * The piston half of a scheduled tick, with the players close enough to be shoved by it.
     *
     * parity: `PistonMovingBlockEntity.moveCollidedEntities`. A player who gets pushed is teleported,
     * because a client only learns it moved when the server says so.
     */
    public fun finishPiston(
        world: LoadedWorld,
        position: IntArray,
        watchers: List<PlayerSession>,
    ): List<BlockChange> {
        val nearby =
            watchers
                .filter { abs(it.x - position[0]) <= pistonReach && abs(it.z - position[2]) <= pistonReach }
                .map { Pistons.PlayerPosition(it.uuid.toString(), it.x, it.y, it.z) }
        val moving =
            pluginEvents.emit(PluginEventTypes.BLOCK_PISTON) { event ->
                event.set(0, world.name)
                event.set(1, position[0])
                event.set(2, position[1])
                event.set(3, position[2])
                event.set(4, true)
                event.set(5, null)
                event.set(6, null)
            }
        if (moving?.cancelled == true) return emptyList()
        return Pistons.finish(world, position[0], position[1], position[2], nearby) { pushed ->
            val player = watchers.firstOrNull { it.uuid.toString() == pushed.uuid } ?: return@finish
            player.connection.teleportTo(pushed.x, pushed.y, pushed.z, player.yaw, player.pitch)
        }
    }

    /**
     * Drops what a flow washed away. parity: `WaterFluid.beforeDestroyingBlock`, which rolls the
     * block's own loot table exactly as breaking it by hand would.
     *
     * Lava is the exception and drops nothing: its `beforeDestroyingBlock` only plays the fizz.
     */
    public fun dropWashedAwayBlock(
        world: LoadedWorld,
        pos: BlockPos,
        destroyedState: Int,
    ) {
        if (destroyedState <= 0 || FluidFlow.kindOf(destroyedState) != null) return
        if (FluidFlow.kindOf(world.state.getBlockState(pos.x, pos.y, pos.z) ?: return) != FluidKind.WATER) return
        val block = Blocks.byStateId(destroyedState)?.block ?: return
        val table = lootTables.forBlock(block) ?: return
        for (stack in table.generate(LegacyRandom(Random.nextLong()))) {
            if (Items.REGISTRY.byIdentifier(stack.item) == null) continue
            world.state.spawnItem(
                WorldState.DroppedItem(
                    entityId = playerRegistry.allocateEntityId(),
                    uuid = Uuid.random(),
                    item = stack.item,
                    count = stack.count,
                    x = pos.x + 0.5,
                    y = pos.y + 0.5,
                    z = pos.z + 0.5,
                ),
            )
        }
    }
}
