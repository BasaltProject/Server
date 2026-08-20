package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment
import org.basaltmc.wire.common.Identifier

/** `pale_moss_patch`, including its ground replacement and nested vegetation. */
public object PaleMossDecoration : BiomeVegetalDecoration {
    public fun decorate(
        chunk: Chunk,
        seed: Long,
        environment: SurfaceRuleEnvironment,
    ) {
        decorateFeature(chunk, seed, environment, null)
    }

    internal fun decorateFeature(
        chunk: Chunk,
        seed: Long,
        environment: SurfaceRuleEnvironment,
        placementObserver: ((DecorationPos) -> Unit)?,
    ) {
        val random = WorldgenRandom(XoroshiroRandom(seed))
        val decorationSeed = random.setDecorationSeed(seed, chunk.chunkX * 16, chunk.chunkZ * 16)
        random.setFeatureSeed(decorationSeed, PALE_MOSS_PATCH_INDEX, VEGETAL_DECORATION_STEP)
        val context = PlacementContext(chunk)
        var positions: Sequence<DecorationPos> =
            sequenceOf(DecorationPos(chunk.chunkX * 16, 0, chunk.chunkZ * 16))
        for (
        modifier in
        listOf(
            CountPlacement(1),
            InSquarePlacement,
            HeightmapPlacement(Chunk.HeightmapType.MOTION_BLOCKING_NO_LEAVES),
        )
        ) {
            positions = positions.flatMap { modifier.getPositions(context, random, it) }
        }
        positions.forEach { origin ->
            if (environment.biomeAt(origin.x, origin.y, origin.z).substringAfter(':') != "pale_garden") {
                return@forEach
            }
            placementObserver?.invoke(origin)
            placePatchAt(chunk, random, origin)
        }
    }

    override fun decorate(
        chunk: Chunk,
        random: WorldgenRandom,
    ) {
        val context = PlacementContext(chunk)
        var positions: Sequence<DecorationPos> = sequenceOf(DecorationPos(chunk.chunkX * 16, 0, chunk.chunkZ * 16))
        for (
        modifier in
        listOf(
            CountPlacement(1),
            InSquarePlacement,
            HeightmapPlacement(Chunk.HeightmapType.MOTION_BLOCKING_NO_LEAVES),
        )
        ) {
            positions = positions.flatMap { modifier.getPositions(context, random, it) }
        }
        positions.forEach { placePatchAt(chunk, random, it) }
    }

    internal fun placePatchAt(
        chunk: Chunk,
        random: WorldgenRandom,
        origin: DecorationPos,
    ) {
        // VegetationPatchFeature adds one to each configured UniformInt(2..4) sample.
        val xRadius = 3 + random.nextInt(3)
        val zRadius = 3 + random.nextInt(3)
        val surface = ArrayList<DecorationPos>((xRadius * 2 + 1) * (zRadius * 2 + 1))

        for (dx in -xRadius..xRadius) {
            val xEdge = dx == -xRadius || dx == xRadius
            for (dz in -zRadius..zRadius) {
                val zEdge = dz == -zRadius || dz == zRadius
                val corner = xEdge && zEdge
                val edgeOnly = (xEdge || zEdge) && !corner
                if (corner || edgeOnly && random.nextFloat() > EXTRA_EDGE_COLUMN_CHANCE) continue

                val ground = findGround(chunk, origin.x + dx, origin.y, origin.z + dz) ?: continue
                val previous = chunk.getDecorationBlockStateId(ground.x, ground.y, ground.z)
                if (previous != paleMossBlockStateId && previous !in mossReplaceableStateIds) continue
                if (previous != paleMossBlockStateId) {
                    chunk.setDecorationBlockStateId(ground.x, ground.y, ground.z, paleMossBlockStateId)
                }
                surface += ground
            }
        }

        for (ground in javaHashSetOrder(surface)) {
            if (random.nextFloat() < VEGETATION_CHANCE) placeVegetation(chunk, random, ground)
        }
    }

    private fun findGround(
        chunk: Chunk,
        x: Int,
        startY: Int,
        z: Int,
    ): DecorationPos? {
        var y = startY
        var offset = 0
        while (isAir(chunk, x, y, z) && offset < VERTICAL_RANGE) {
            y--
            offset++
            if (y < WorldHeight.MIN_Y) return null
        }
        offset = 0
        while (!isAir(chunk, x, y, z) && offset < VERTICAL_RANGE) {
            y++
            offset++
            if (y > WorldHeight.MAX_Y) return null
        }
        val belowY = y - 1
        if (!isAir(chunk, x, y, z) || belowY < WorldHeight.MIN_Y) return null
        val below = chunk.getDecorationBlockStateId(x, belowY, z)
        if (!Chunk.HeightmapType.OCEAN_FLOOR.matches(below)) return null
        return DecorationPos(x, belowY, z)
    }

    /** parity: `pale_moss_patch`'s own vegetation, at one position. */
    public fun placeAt(
        chunk: Chunk,
        random: WorldgenRandom,
        origin: DecorationPos,
    ) {
        placeVegetation(chunk, random, origin)
    }

    private fun placeVegetation(
        chunk: Chunk,
        random: WorldgenRandom,
        ground: DecorationPos,
    ) {
        val roll = random.nextInt(60)
        val stateId =
            when {
                roll < 25 -> paleMossCarpetStateId
                roll < 50 -> shortGrassStateId
                else -> tallGrassStateId
            }
        SimpleBlockFeature({ _, _ -> stateId }, isDoublePlant = stateId == tallGrassStateId)
            .place(chunk, random, ground.copy(y = ground.y + 1))
    }

    private fun isAir(
        chunk: Chunk,
        x: Int,
        y: Int,
        z: Int,
    ): Boolean =
        y in WorldHeight.MIN_Y..WorldHeight.MAX_Y &&
            Blocks.isAir(chunk.getDecorationBlockStateId(x, y, z))

    /** HashMap bucket iteration used by vanilla's HashSet<BlockPos>. */
    private fun javaHashSetOrder(positions: List<DecorationPos>): Sequence<DecorationPos> {
        if (positions.isEmpty()) return emptySequence()
        var capacity = 16
        while (positions.size > capacity * 3 / 4) capacity *= 2
        return sequence {
            for (bucket in 0 until capacity) {
                for (position in positions) {
                    val hash = (position.y + position.z * 31) * 31 + position.x
                    val spread = hash xor (hash ushr 16)
                    if (spread and (capacity - 1) == bucket) yield(position)
                }
            }
        }
    }

    private fun stateId(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")

    private const val VERTICAL_RANGE = 5
    private const val VEGETATION_CHANCE = 0.3f
    private const val EXTRA_EDGE_COLUMN_CHANCE = 0.75f
    private const val PALE_MOSS_PATCH_INDEX = 14
    private const val VEGETAL_DECORATION_STEP = 9

    private val paleMossBlockStateId = stateId("pale_moss_block")
    private val paleMossCarpetStateId = stateId("pale_moss_carpet")
    private val shortGrassStateId = stateId("short_grass")
    private val tallGrassStateId = stateId("tall_grass")
    private val mossReplaceableStateIds =
        setOf(
            "stone",
            "granite",
            "diorite",
            "andesite",
            "tuff",
            "deepslate",
            "cave_vines_plant",
            "cave_vines",
            "dirt",
            "coarse_dirt",
            "rooted_dirt",
            "mud",
            "muddy_mangrove_roots",
            "moss_block",
            "pale_moss_block",
            "grass_block",
            "podzol",
            "mycelium",
        ).map(::stateId).toSet()
}
