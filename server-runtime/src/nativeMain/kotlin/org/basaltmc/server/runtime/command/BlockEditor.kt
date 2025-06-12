package org.basaltmc.server.runtime.command

import org.basaltmc.server.runtime.world.LoadedWorld
import org.basaltmc.server.world.ChunkHeight
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
        return true
    }

    /** Sends everything written since the last flush to whoever is in this world. */
    public fun flush() {
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
