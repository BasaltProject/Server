package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.wire.common.Identifier
import kotlin.math.cos
import kotlin.math.floor
import kotlin.math.sin

/**
 * Buckets. parity: `BucketItem.use` plus `Item.getPlayerPOVHitResult`.
 *
 * A fluid is not a block target, so the client sends no position and the server traces the look
 * vector itself.
 */
public object Buckets {
    public val EMPTY: Identifier = Identifier.vanilla("bucket")
    public val WATER: Identifier = Identifier.vanilla("water_bucket")
    public val LAVA: Identifier = Identifier.vanilla("lava_bucket")

    /** parity: `Player.blockInteractionRange`, the 4.5 blocks a hand reaches, 5 in creative. */
    private const val REACH = 4.5
    private const val CREATIVE_REACH = 5.0
    private const val STEP = 0.05

    public data class Result(
        val changes: List<IntArray>,
        val replacement: Identifier?,
    )

    /** What a bucket holds, or null if this item is not one. */
    public fun contentOf(item: Identifier): FluidKind? =
        when (item) {
            WATER -> FluidKind.WATER
            LAVA -> FluidKind.LAVA
            else -> null
        }

    public fun isBucket(item: Identifier): Boolean = item == EMPTY || item == WATER || item == LAVA

    /**
     * Uses [item] along the look vector: what changed, and what the bucket becomes.
     *
     * parity: a full bucket traces past fluids onto the first solid face; an empty one stops at
     * the first source.
     */
    public fun use(
        world: LoadedWorld,
        item: Identifier,
        eyeX: Double,
        eyeY: Double,
        eyeZ: Double,
        yaw: Float,
        pitch: Float,
        creative: Boolean,
    ): Result? {
        val content = contentOf(item)
        val reach = if (creative) CREATIVE_REACH else REACH
        val hit = trace(world, eyeX, eyeY, eyeZ, yaw, pitch, reach, stopAtFluid = content == null) ?: return null

        if (content == null) return fill(world, hit)
        return pour(world, hit, content, creative)
    }

    /** parity: `BucketItem.emptyContents`: onto the face it hit, or into the block itself if that can hold it. */
    private fun pour(
        world: LoadedWorld,
        hit: Hit,
        content: FluidKind,
        creative: Boolean,
    ): Result? {
        val clicked = world.state.getBlockState(hit.x, hit.y, hit.z) ?: return null
        val target = if (replaceable(clicked)) hit else hit.offset()
        val existing = world.state.getBlockState(target.x, target.y, target.z) ?: return null
        if (!replaceable(existing)) return null

        val source = content.baseStateId
        if (!world.state.setBlockState(target.x, target.y, target.z, source)) return null
        val changes = mutableListOf(intArrayOf(target.x, target.y, target.z, source))
        changes += FluidFlow.onBlockChanged(world, target.x, target.y, target.z)
        return Result(changes, if (creative) null else EMPTY)
    }

    /** parity: `BucketPickup.pickupBlock` on `LiquidBlock`: only a source goes into the bucket. */
    private fun fill(
        world: LoadedWorld,
        hit: Hit,
    ): Result? {
        val state = world.state.getBlockState(hit.x, hit.y, hit.z) ?: return null
        val kind = FluidFlow.kindOf(state) ?: return null
        if (state != kind.baseStateId) return null
        if (!world.state.setBlockState(hit.x, hit.y, hit.z, Blocks.AIR.stateId)) return null
        val changes = mutableListOf(intArrayOf(hit.x, hit.y, hit.z, Blocks.AIR.stateId))
        changes += FluidFlow.onBlockChanged(world, hit.x, hit.y, hit.z)
        return Result(changes, if (kind == FluidKind.WATER) WATER else LAVA)
    }

    /** Air and fluids give way to a poured bucket; anything a player walks into does not. */
    private fun replaceable(stateId: Int): Boolean =
        Blocks.isAir(stateId) ||
            FluidFlow.kindOf(stateId) != null ||
            !Chunk.HeightmapType.MOTION_BLOCKING.matches(stateId)

    private data class Hit(
        val x: Int,
        val y: Int,
        val z: Int,
        val faceX: Int,
        val faceY: Int,
        val faceZ: Int,
    ) {
        fun offset(): Hit = copy(x = x + faceX, y = y + faceY, z = z + faceZ)
    }

    /** Steps along the look vector until it meets something; no shapes here to clip against. */
    private fun trace(
        world: LoadedWorld,
        eyeX: Double,
        eyeY: Double,
        eyeZ: Double,
        yaw: Float,
        pitch: Float,
        reach: Double,
        stopAtFluid: Boolean,
    ): Hit? {
        val yawRadians = yaw.toDouble() * PI_OVER_180
        val pitchRadians = pitch.toDouble() * PI_OVER_180
        val dirX = -sin(yawRadians) * cos(pitchRadians)
        val dirY = -sin(pitchRadians)
        val dirZ = cos(yawRadians) * cos(pitchRadians)

        var previousX = floor(eyeX).toInt()
        var previousY = floor(eyeY).toInt()
        var previousZ = floor(eyeZ).toInt()
        var travelled = 0.0
        while (travelled <= reach) {
            val x = floor(eyeX + dirX * travelled).toInt()
            val y = floor(eyeY + dirY * travelled).toInt()
            val z = floor(eyeZ + dirZ * travelled).toInt()
            if (x != previousX || y != previousY || z != previousZ) {
                val state = world.state.getBlockState(x, y, z) ?: return null
                val fluid = FluidFlow.kindOf(state)
                val solid = !Blocks.isAir(state) && fluid == null && Chunk.HeightmapType.MOTION_BLOCKING.matches(state)
                if (solid || (stopAtFluid && fluid != null)) {
                    return Hit(x, y, z, previousX - x, previousY - y, previousZ - z)
                }
                previousX = x
                previousY = y
                previousZ = z
            }
            travelled += STEP
        }
        return null
    }

    private const val PI_OVER_180 = 0.017453292519943295
}
