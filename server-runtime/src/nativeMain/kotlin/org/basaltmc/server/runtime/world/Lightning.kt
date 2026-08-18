package org.basaltmc.server.runtime.world

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.world.Chunk
import org.basaltmc.wire.common.Identifier

/**
 * What a storm drops on the world.
 *
 * parity: `ServerLevel.tickChunk`'s thunder half over `LightningBolt`. A rod within 128 blocks takes
 * the strike instead of whatever was under it, and being struck powers it for eight ticks. Fires are
 * lit only on normal and hard, which is why a peaceful thunderstorm burns nothing.
 */
public object Lightning {
    /** parity: the one in a hundred thousand a chunk rolls each tick while it thunders. */
    public const val STRIKE_ODDS: Int = 100000

    /** parity: `findLightningRod`, which looks 128 blocks around the target. */
    public const val ROD_RANGE: Int = 128

    /** parity: `LightningRodBlock.onLightningStrike`, eight ticks of signal. */
    public const val ROD_TICKS: Int = 8

    private val ROD = Identifier.vanilla("lightning_rod")

    /** Where a strike aimed at this column actually lands: a rod nearby, or the top of the column. */
    public fun targetFor(
        world: LoadedWorld,
        x: Int,
        z: Int,
    ): IntArray? {
        val chunk = world.state.chunkIfLoaded(x shr 4, z shr 4) ?: return null
        val top = chunk.heightAt(x and 15, z and 15, Chunk.HeightmapType.MOTION_BLOCKING)
        rodNear(world, x, top, z)?.let { return it }
        return intArrayOf(x, top, z)
    }

    /**
     * parity: `LightningBolt.powerLightningRod` and `spawnFire`: the rod it hit answers, and on the
     * harder difficulties the ground it hit catches.
     */
    public fun strike(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        lightFires: Boolean,
        changes: MutableList<BlockChange>,
    ) {
        powerRod(world, x, y - 1, z, changes)
        powerRod(world, x, y, z, changes)
        if (!lightFires) return
        if (Fires.ignite(world, x, y, z)) changes += BlockChange(x, y, z, world.state.getBlockState(x, y, z) ?: 0)
    }

    /** The scheduled half: the rod lets its signal go eight ticks after it was struck. */
    public fun releaseRod(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): BlockChange? {
        val stateId = world.state.getBlockState(x, y, z) ?: return null
        val block = BlockStateDefinitions.blockOf(stateId) ?: return null
        if (block != ROD) return null
        if (BlockStateDefinitions.propertiesOf(stateId)["powered"] != "true") return null
        val turned = BlockStateDefinitions.stateIdOf(block, mapOf("powered" to "false"), stateId) ?: return null
        if (!world.state.setBlockState(x, y, z, turned)) return null
        return BlockChange(x, y, z, turned)
    }

    /** parity: the roll a chunk makes while the sky is thundering. */
    public fun shouldStrike(random: BasaltRandom): Boolean = random.nextInt(STRIKE_ODDS) == 0

    private fun powerRod(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        changes: MutableList<BlockChange>,
    ) {
        val stateId = world.state.getBlockState(x, y, z) ?: return
        val block = BlockStateDefinitions.blockOf(stateId) ?: return
        if (block != ROD) return
        val turned = BlockStateDefinitions.stateIdOf(block, mapOf("powered" to "true"), stateId) ?: return
        if (world.state.setBlockState(x, y, z, turned)) changes += BlockChange(x, y, z, turned)
        world.state.scheduleBlockTick(x, y, z, ROD, ROD_TICKS)
    }

    /**
     * parity: `findLightningRod`, which takes the closest rod whose tip is the top of its own column.
     * Vanilla searches its point-of-interest index; here the loaded chunks around the target are
     * walked instead, which reaches the same rods without an index to keep.
     */
    private fun rodNear(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): IntArray? {
        var best: IntArray? = null
        var bestDistance = Int.MAX_VALUE
        val radius = ROD_RANGE shr 4
        for (chunkX in (x shr 4) - radius..(x shr 4) + radius) {
            for (chunkZ in (z shr 4) - radius..(z shr 4) + radius) {
                val chunk = world.state.chunkIfLoaded(chunkX, chunkZ) ?: continue
                for (localX in 0 until 16) {
                    for (localZ in 0 until 16) {
                        val top = chunk.heightAt(localX, localZ, Chunk.HeightmapType.MOTION_BLOCKING)
                        val rodX = (chunkX shl 4) + localX
                        val rodZ = (chunkZ shl 4) + localZ
                        val stateId = world.state.getBlockState(rodX, top - 1, rodZ) ?: continue
                        if (BlockStateDefinitions.blockOf(stateId) != ROD) continue
                        val distance =
                            (rodX - x) * (rodX - x) + (top - y) * (top - y) + (rodZ - z) * (rodZ - z)
                        if (distance > ROD_RANGE * ROD_RANGE || distance >= bestDistance) continue
                        bestDistance = distance
                        best = intArrayOf(rodX, top, rodZ)
                    }
                }
            }
        }
        return best
    }
}
