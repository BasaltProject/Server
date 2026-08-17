package org.basaltmc.server.runtime.world

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.registry.Blocks
import kotlin.math.sqrt

/**
 * An explosion: which blocks it takes out, and how hard it hits what is standing near.
 *
 * parity: `ServerExplosion.calculateExplodedPositions`, ray for ray. Sixteen cubed directions, only
 * the ones on the shell of that cube, each stepped 0.3 blocks at a time while its power lasts, and
 * every block it passes spending `(resistance + 0.3) * 0.3` of it.
 *
 * A bed in the Nether is what asks for one today; TNT and creepers will ask for the same routine.
 */
public object Explosions {
    private const val RAYS = 16
    private const val STEP = 0.3f
    private const val STEP_COST = 0.22500001f
    private const val RESISTANCE_BIAS = 0.3f

    /** parity: `BedBlock`'s own five, the power of a bed that will not let you sleep. */
    public const val BED_POWER: Float = 5f

    /** A block an explosion took out, so the caller can drop it and tell the clients. */
    public class Broken(
        public val x: Int,
        public val y: Int,
        public val z: Int,
        public val stateId: Int,
    )

    /**
     * Blows a hole at ([x], [y], [z]) and answers what it took out. The blocks are already gone
     * from the world; what they drop is the caller's business, as it is in vanilla.
     */
    public fun explode(
        world: LoadedWorld,
        x: Double,
        y: Double,
        z: Double,
        power: Float,
        random: LegacyRandom,
    ): List<Broken> {
        val hit = HashSet<Long>()
        val broken = mutableListOf<Broken>()
        for (rayX in 0 until RAYS) {
            for (rayY in 0 until RAYS) {
                for (rayZ in 0 until RAYS) {
                    val onShell =
                        rayX == 0 ||
                            rayX == RAYS - 1 ||
                            rayY == 0 ||
                            rayY == RAYS - 1 ||
                            rayZ == 0 ||
                            rayZ == RAYS - 1
                    if (!onShell) continue
                    var dx = (rayX / (RAYS - 1.0) * 2.0) - 1.0
                    var dy = (rayY / (RAYS - 1.0) * 2.0) - 1.0
                    var dz = (rayZ / (RAYS - 1.0) * 2.0) - 1.0
                    val length = sqrt(dx * dx + dy * dy + dz * dz)
                    dx /= length
                    dy /= length
                    dz /= length
                    var remaining = power * (0.7f + random.nextFloat() * 0.6f)
                    var px = x
                    var py = y
                    var pz = z
                    while (remaining > 0f) {
                        val blockX = kotlin.math.floor(px).toInt()
                        val blockY = kotlin.math.floor(py).toInt()
                        val blockZ = kotlin.math.floor(pz).toInt()
                        if (blockY < world.minY || blockY > world.maxY) break
                        val stateId = world.state.getBlockState(blockX, blockY, blockZ) ?: break
                        if (stateId != Blocks.AIR.stateId) {
                            remaining -= (resistanceOf(stateId) + RESISTANCE_BIAS) * STEP
                        }
                        if (remaining > 0f && canBreak(stateId)) {
                            hit += packed(blockX, blockY, blockZ)
                        }
                        px += dx * STEP
                        py += dy * STEP
                        pz += dz * STEP
                        remaining -= STEP_COST
                    }
                }
            }
        }
        for (position in hit) {
            val bx = unpackX(position)
            val by = unpackY(position)
            val bz = unpackZ(position)
            val stateId = world.state.getBlockState(bx, by, bz) ?: continue
            if (stateId == Blocks.AIR.stateId) continue
            if (!world.state.setBlockState(bx, by, bz, Blocks.AIR.stateId)) continue
            broken += Broken(bx, by, bz, stateId)
        }
        return broken
    }

    /**
     * How much damage this explosion does to something [distance] blocks away.
     *
     * parity: `ServerExplosion.hurtEntities`' own arithmetic, without the exposure ray cast: the
     * seen fraction is one here, which is what an entity in the open gets anyway.
     */
    public fun damageAt(
        distance: Double,
        power: Float,
        radius: Float,
    ): Float {
        if (distance >= radius) return 0f
        val nearness = 1.0 - distance / radius
        return ((nearness * nearness + nearness) / 2.0 * 7.0 * power + 1.0).toFloat()
    }

    /** parity: `Block.getExplosionResistance`, and bedrock's own refusal. */
    private fun resistanceOf(stateId: Int): Float {
        val block = BlockStateDefinitions.blockOf(stateId) ?: return 0f
        return Blocks.hardnessOf(block)?.explosionResistance ?: 0f
    }

    private fun canBreak(stateId: Int): Boolean {
        if (stateId == Blocks.AIR.stateId) return false
        val block = BlockStateDefinitions.blockOf(stateId) ?: return false
        return Blocks.hardnessOf(block)?.isUnbreakable != true
    }

    private fun packed(
        x: Int,
        y: Int,
        z: Int,
    ): Long = (x.toLong() and 0x3FFFFFF shl 38) or (z.toLong() and 0x3FFFFFF shl 12) or (y.toLong() and 0xFFF)

    private fun unpackX(packed: Long): Int = (packed shr 38).toInt() shl 6 shr 6

    private fun unpackZ(packed: Long): Int = (packed shl 26 shr 38).toInt()

    private fun unpackY(packed: Long): Int = (packed shl 52 shr 52).toInt()
}
