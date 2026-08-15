package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment

/**
 * Frozen-ocean `LOCAL_MODIFICATIONS` and `SURFACE_STRUCTURES`: packed icebergs (1/16), blue
 * icebergs (1/200), then 0..19 blue-ice attempts between Y 30 and 61. Each public entry point
 * resets the exact global feature seed before running the direct 26.2 feature translation.
 */
public object FrozenOceanDecoration {
    public fun decorateLocalModifications(
        chunk: Chunk,
        seed: Long,
        environment: SurfaceRuleEnvironment,
    ) {
        val random = WorldgenRandom(XoroshiroRandom(seed))
        val decorationSeed = random.setDecorationSeed(seed, chunk.chunkX * 16, chunk.chunkZ * 16)
        val region = ChunkFeatureRegion(chunk)
        random.setFeatureSeed(decorationSeed, PACKED_ICEBERG_INDEX, LOCAL_MODIFICATIONS_STEP)
        placeIcebergWithRarity(region, random, environment, 16, Blocks.PACKED_ICE.stateId)
        random.setFeatureSeed(decorationSeed, BLUE_ICEBERG_INDEX, LOCAL_MODIFICATIONS_STEP)
        placeIcebergWithRarity(region, random, environment, 200, Blocks.BLUE_ICE.stateId)
        region.flush()
    }

    public fun decorateSurfaceStructures(
        chunk: Chunk,
        seed: Long,
        environment: SurfaceRuleEnvironment,
    ) {
        val random = WorldgenRandom(XoroshiroRandom(seed))
        val decorationSeed = random.setDecorationSeed(seed, chunk.chunkX * 16, chunk.chunkZ * 16)
        random.setFeatureSeed(decorationSeed, BLUE_ICE_INDEX, SURFACE_STRUCTURES_STEP)
        val region = ChunkFeatureRegion(chunk)
        repeat(random.nextInt(20)) {
            val x = chunk.chunkX * 16 + random.nextInt(16)
            val z = chunk.chunkZ * 16 + random.nextInt(16)
            val origin =
                DecorationPos(
                    x,
                    30 + random.nextInt(32),
                    z,
                )
            if (!isFrozenOcean(environment, origin)) return@repeat
            BlueIceFeature.place(region, random, origin)
        }
        region.flush()
    }

    /** The vanilla placed feature names this step draws. */
    public val PLACED_IDS: Set<String> = setOf("iceberg_packed", "iceberg_blue", "blue_ice")

    /** One of them at one position, which is what `/place feature` runs. */
    public fun placeAt(
        chunk: Chunk,
        random: WorldgenRandom,
        origin: DecorationPos,
        placedFeature: String,
    ): Boolean {
        val region = ChunkFeatureRegion(chunk)
        when (placedFeature) {
            "iceberg_packed" -> IcebergFeature.place(region, random, origin, Blocks.PACKED_ICE.stateId)
            "iceberg_blue" -> IcebergFeature.place(region, random, origin, Blocks.BLUE_ICE.stateId)
            "blue_ice" -> BlueIceFeature.place(region, random, origin)
            else -> return false
        }
        region.flush()
        return true
    }

    private fun placeIcebergWithRarity(
        region: ChunkFeatureRegion,
        random: WorldgenRandom,
        environment: SurfaceRuleEnvironment,
        chance: Int,
        stateId: Int,
    ) {
        if (random.nextFloat() >= 1.0f / chance) return
        val chunk = region.chunk
        val origin =
            DecorationPos(
                chunk.chunkX * 16 + random.nextInt(16),
                SEA_LEVEL,
                chunk.chunkZ * 16 + random.nextInt(16),
            )
        if (!isFrozenOcean(environment, origin.copy(y = WorldHeight.MIN_Y))) return
        IcebergFeature.place(region, random, origin, stateId)
    }

    private fun isFrozenOcean(
        environment: SurfaceRuleEnvironment,
        position: DecorationPos,
    ): Boolean = environment.biomeAt(position.x, position.y, position.z).substringAfter(':') in FROZEN_OCEAN_BIOMES

    private const val SEA_LEVEL = 63
    private const val LOCAL_MODIFICATIONS_STEP = 2
    private const val SURFACE_STRUCTURES_STEP = 4
    private const val PACKED_ICEBERG_INDEX = 0
    private const val BLUE_ICEBERG_INDEX = 1
    private const val BLUE_ICE_INDEX = 3
    private val FROZEN_OCEAN_BIOMES = setOf("frozen_ocean", "deep_frozen_ocean")
}

/**
 * Small feature-region overlay. Icebergs can extend outside their origin chunk; retaining those
 * writes in memory preserves their shape and smoothing at the returned chunk edge. Only blocks
 * belonging to the returned chunk are flushed because this generator owns no neighboring chunks.
 */
internal class ChunkFeatureRegion(
    internal val chunk: Chunk,
) {
    private val changes = HashMap<DecorationPos, Int>()

    fun get(pos: DecorationPos): Int {
        changes[pos]?.let { return it }
        if (pos.y !in WorldHeight.MIN_Y..WorldHeight.MAX_Y) return Blocks.AIR.stateId
        if ((pos.x shr 4) == chunk.chunkX && (pos.z shr 4) == chunk.chunkZ) {
            return chunk.getBlockStateId(pos.x and 15, pos.y, pos.z and 15)
        }
        return if (pos.y < 63) Blocks.WATER.stateId else Blocks.AIR.stateId
    }

    fun set(
        pos: DecorationPos,
        stateId: Int,
    ) {
        if (pos.y in WorldHeight.MIN_Y..WorldHeight.MAX_Y) changes[pos] = stateId
    }

    fun flush() {
        for ((pos, stateId) in changes) {
            if ((pos.x shr 4) == chunk.chunkX && (pos.z shr 4) == chunk.chunkZ) {
                chunk.setBlockStateId(pos.x and 15, pos.y, pos.z and 15, stateId)
            }
        }
    }
}

internal object BlueIceFeature {
    fun place(
        region: ChunkFeatureRegion,
        random: WorldgenRandom,
        origin: DecorationPos,
    ): Boolean {
        if (origin.y > 62) return false
        if (region.get(origin) != Blocks.WATER.stateId &&
            region.get(origin.down()) != Blocks.WATER.stateId
        ) {
            return false
        }
        if (DIRECTIONS_WITHOUT_DOWN.none { region.get(origin.offset(it)) == Blocks.PACKED_ICE.stateId }) return false

        region.set(origin, Blocks.BLUE_ICE.stateId)
        repeat(200) {
            val yOffset = random.nextInt(5) - random.nextInt(6)
            var horizontalRange = 3
            if (yOffset < 2) horizontalRange += yOffset / 2
            if (horizontalRange >= 1) {
                val target =
                    origin.offset(
                        random.nextInt(horizontalRange) - random.nextInt(horizontalRange),
                        yOffset,
                        random.nextInt(horizontalRange) - random.nextInt(horizontalRange),
                    )
                val stateId = region.get(target)
                if (Blocks.isAir(stateId) ||
                    stateId == Blocks.WATER.stateId ||
                    stateId == Blocks.PACKED_ICE.stateId ||
                    stateId == Blocks.ICE.stateId
                ) {
                    if (ALL_DIRECTIONS.any { region.get(target.offset(it)) == Blocks.BLUE_ICE.stateId }) {
                        region.set(target, Blocks.BLUE_ICE.stateId)
                    }
                }
            }
        }
        return true
    }
}

internal data class DirectionOffset(
    val x: Int,
    val y: Int,
    val z: Int,
)

internal val ALL_DIRECTIONS: List<DirectionOffset> =
    listOf(
        DirectionOffset(0, -1, 0),
        DirectionOffset(0, 1, 0),
        DirectionOffset(0, 0, -1),
        DirectionOffset(0, 0, 1),
        DirectionOffset(-1, 0, 0),
        DirectionOffset(1, 0, 0),
    )
internal val DIRECTIONS_WITHOUT_DOWN: List<DirectionOffset> = ALL_DIRECTIONS.drop(1)

internal fun DecorationPos.offset(
    x: Int,
    y: Int,
    z: Int,
): DecorationPos = DecorationPos(this.x + x, this.y + y, this.z + z)

internal fun DecorationPos.offset(direction: DirectionOffset): DecorationPos =
    offset(direction.x, direction.y, direction.z)

internal fun DecorationPos.down(): DecorationPos = offset(0, -1, 0)

internal fun DecorationPos.up(): DecorationPos = offset(0, 1, 0)
