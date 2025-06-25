package org.basaltmc.server.runtime.command

import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.runtime.world.BlockChange
import org.basaltmc.server.runtime.world.Containers
import org.basaltmc.server.runtime.world.LoadedWorld
import org.basaltmc.server.runtime.world.NeighbourUpdates
import org.basaltmc.server.runtime.world.Shelves
import org.basaltmc.server.world.ChunkBlockEntity
import org.basaltmc.server.world.ChunkHeight
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.protocol.play.BlockPos
import org.basaltmc.wire.protocol.play.ClientboundBlockUpdatePacket
import org.basaltmc.wire.protocol.play.SectionBlocksUpdatePacket
import org.basaltmc.wire.text.Component

/**
 * Block writes on behalf of a command, batched so one `/fill` is not one packet per block.
 *
 * Changes are collected per 16x16x16 section and sent when [flush] runs: a section with a single
 * change goes out as `block_update`, one with several as `section_blocks_update`, which is the same
 * split vanilla's chunk tracker makes.
 *
 * Only loaded chunks are written and [require] refuses anything outside one, which is vanilla's
 * rule (`BlockPosArgument.getLoadedBlockPos`) and keeps a `/fill` off the generation path.
 */
public class BlockEditor(
    private val environment: CommandEnvironment,
    public val world: LoadedWorld,
) {
    private val bounds: ChunkHeight = world.declaration.dimension.chunkHeight
    private val changesBySection = mutableMapOf<Long, MutableMap<Short, Int>>()

    /**
     * Where blocks were written, so their neighbours can be told once the whole edit is done.
     *
     * parity: `/setblock` and `/fill` both write with `Block.UPDATE_ALL`, so a redstone block placed
     * by command powers what is next to it and a door beside it swings. Doing it after the writes
     * rather than during them is what stops a `/fill` reacting to a structure it is still building.
     */
    private val written = mutableListOf<IntArray>()

    public val minY: Int get() = bounds.minY
    public val maxY: Int get() = bounds.maxY

    /** The state at this position, or null when its chunk is not loaded. */
    public fun stateAt(
        x: Int,
        y: Int,
        z: Int,
    ): Int? {
        if (y < bounds.minY || y > bounds.maxY) return null
        return world.state.getBlockState(x, y, z)
    }

    /** parity: `BlockPosArgument.getLoadedBlockPos`, which refuses an unloaded position by name. */
    public fun require(
        x: Int,
        y: Int,
        z: Int,
    ) {
        if (y < bounds.minY || y > bounds.maxY) {
            throw CommandSyntaxException(Component.translatable("argument.pos.outofbounds"))
        }
        if (!world.state.isLoaded(x shr 4, z shr 4)) {
            throw CommandSyntaxException(Component.translatable("argument.pos.unloaded"))
        }
    }

    /** Writes one block, remembering it for [flush]. False when nothing changed or the chunk is gone. */
    public fun set(
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
    ): Boolean {
        if (y < bounds.minY || y > bounds.maxY) return false
        if (world.state.getBlockState(x, y, z) == stateId) return false
        if (!world.state.setBlockState(x, y, z, stateId)) return false
        val section = SectionBlocksUpdatePacket.packSection(x shr 4, y shr 4, z shr 4)
        changesBySection.getOrPut(section) { mutableMapOf() }[SectionBlocksUpdatePacket.packPosition(x, y, z)] = stateId
        written += intArrayOf(x, y, z)
        createBlockEntity(x, y, z, stateId)
        return true
    }

    /**
     * parity: `Level.setBlock` gives a block its block entity as it lands, so `/setblock` of a chest
     * leaves something `/data` can read. This build creates them where they are used instead, which
     * left a command-placed container with nowhere to put anything.
     */
    private fun createBlockEntity(
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
    ) {
        val block = BlockStateDefinitions.blockOf(stateId) ?: return
        if (Containers.SIZES[block.path] == null && !Shelves.isShelf(block.path)) return
        val chunk = world.state.chunkIfLoaded(x shr 4, z shr 4) ?: return
        if (chunk.blockEntityAt(x and 15, y, z and 15) != null) return
        chunk.setBlockEntity(ChunkBlockEntity(x and 15, y, z and 15, block, NbtCompound()))
    }

    /** Sends everything written since the last flush to whoever is in this world. */
    public fun flush() {
        notifyNeighbours()
        if (changesBySection.isEmpty()) return
        val watchers = environment.players.snapshot().filter { it.worldName == world.name }
        if (watchers.isNotEmpty()) {
            for ((section, changes) in changesBySection) {
                val sectionX = ((section shl 0) shr 42).toInt()
                val sectionY = ((section shl 44) shr 44).toInt()
                val sectionZ = ((section shl 22) shr 42).toInt()
                if (changes.size == 1) {
                    val (packed, stateId) = changes.entries.single()
                    val pos =
                        BlockPos(
                            (sectionX shl 4) + ((packed.toInt() ushr 8) and 15),
                            (sectionY shl 4) + (packed.toInt() and 15),
                            (sectionZ shl 4) + ((packed.toInt() ushr 4) and 15),
                        )
                    for (player in watchers) {
                        player.connection.sendPlayPacket(ClientboundBlockUpdatePacket.ID) {
                            ClientboundBlockUpdatePacket(pos, stateId).write(it)
                        }
                    }
                } else {
                    val packet =
                        SectionBlocksUpdatePacket(
                            sectionX,
                            sectionY,
                            sectionZ,
                            changes.keys.toShortArray(),
                            changes.values.toIntArray(),
                        )
                    for (player in watchers) {
                        player.connection.sendPlayPacket(SectionBlocksUpdatePacket.ID) { packet.write(it) }
                    }
                }
            }
        }
        changesBySection.clear()
        for (change in effects) {
            environment.blockEffects(world, change.x, change.y, change.z, change.stateId, change.effects)
        }
        effects.clear()
    }

    private val effects = mutableListOf<BlockChange>()

    /** Lets what was written act on what was already there, which is what makes a command build work. */
    private fun notifyNeighbours() {
        if (written.isEmpty()) return
        val positions = written.toList()
        written.clear()
        for (at in positions) {
            NeighbourUpdates.afterChange(
                world,
                at[0],
                at[1],
                at[2],
                onReshaped = { stateId, x, y, z -> set(x, y, z, stateId) },
                effects = effects,
                reshapeSelf = false,
            ) { _, x, y, z -> set(x, y, z, Blocks.AIR.stateId) }
        }
        // A reaction can move a block of its own, and those go out with everything else.
        written.clear()
    }
}

/** The world a command acts on: the one its runner is standing in, or the default for the console. */
public fun CommandContext.world(): LoadedWorld {
    val name = source.player?.worldName
    val worlds = environment.worlds
    return worlds[name]
        ?: worlds[environment.defaultWorldName]
        ?: worlds.values.firstOrNull()
        ?: throw CommandSyntaxException(Component.text("No world is loaded."))
}

/** A [BlockEditor] over the world this command acts on. */
public fun CommandContext.blocks(): BlockEditor = BlockEditor(environment, world())
