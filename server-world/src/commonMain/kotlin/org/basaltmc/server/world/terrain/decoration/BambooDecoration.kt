package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment

/** // parity: BambooFeature and the bamboo/bamboo_light placed features. */
public object BambooDecoration {
    private val LIGHT_MODIFIERS = listOf(RarityFilter(4), InSquarePlacement, HeightmapPlacement)
    private val DENSE_MODIFIERS =
        listOf(
            NoiseBasedCountPlacement(noiseToCountRatio = 160, noiseFactor = 80.0, noiseOffset = 0.3),
            InSquarePlacement,
            HeightmapPlacement(Chunk.HeightmapType.WORLD_SURFACE),
        )

    public fun decorate(
        chunk: Chunk,
        seed: Long,
        environment: SurfaceRuleEnvironment,
        externalBlockState: ((x: Int, y: Int, z: Int) -> Int?)? = null,
        blockWriteObserver: ((x: Int, y: Int, z: Int, stateId: Int) -> Unit)? = null,
    ) {
        decorateFeature(BAMBOO_INDEX, chunk, seed, environment, externalBlockState, blockWriteObserver)
        decorateFeature(BAMBOO_LIGHT_INDEX, chunk, seed, environment, externalBlockState, blockWriteObserver)
    }

    internal fun decorateFeature(
        index: Int,
        chunk: Chunk,
        seed: Long,
        environment: SurfaceRuleEnvironment,
        externalBlockState: ((x: Int, y: Int, z: Int) -> Int?)? = null,
        blockWriteObserver: ((x: Int, y: Int, z: Int, stateId: Int) -> Unit)? = null,
    ) {
        val random = WorldgenRandom(XoroshiroRandom(seed))
        val decorationSeed = random.setDecorationSeed(seed, chunk.chunkX * 16, chunk.chunkZ * 16)
        val region = BambooRegion(chunk, externalBlockState, blockWriteObserver)
        random.setFeatureSeed(decorationSeed, index, VEGETAL_DECORATION_STEP)
        when (index) {
            BAMBOO_INDEX -> decorateFeature(chunk, region, random, DENSE_MODIFIERS, 0.2f, environment, "bamboo_jungle")
            BAMBOO_LIGHT_INDEX -> decorateFeature(chunk, region, random, LIGHT_MODIFIERS, 0.0f, environment, "jungle")
            else -> error("unknown bamboo vegetal index $index")
        }
    }

    public val LIGHT: BiomeVegetalDecoration =
        feature(
            podzolProbability = 0.0f,
            modifiers = LIGHT_MODIFIERS,
        )

    public val DENSE: BiomeVegetalDecoration =
        feature(
            podzolProbability = 0.2f,
            modifiers = DENSE_MODIFIERS,
        )

    private fun feature(
        podzolProbability: Float,
        modifiers: List<PlacementModifier>,
    ): BiomeVegetalDecoration =
        BiomeVegetalDecoration { chunk, random ->
            decorateFeature(
                chunk,
                BambooRegion(chunk, null, null),
                random,
                modifiers,
                podzolProbability,
                environment = null,
                requiredBiome = null,
            )
        }

    private fun decorateFeature(
        chunk: Chunk,
        region: BambooRegion,
        random: WorldgenRandom,
        modifiers: List<PlacementModifier>,
        podzolProbability: Float,
        environment: SurfaceRuleEnvironment?,
        requiredBiome: String?,
    ) {
        val context = PlacementContext(chunk)
        var positions: Sequence<DecorationPos> =
            sequenceOf(DecorationPos(chunk.chunkX * 16, 0, chunk.chunkZ * 16))
        for (modifier in modifiers) positions = positions.flatMap { modifier.getPositions(context, random, it) }
        positions.forEach { origin ->
            if (requiredBiome == null ||
                environment?.biomeAt(origin.x, origin.y, origin.z)?.substringAfter(':') == requiredBiome
            ) {
                place(region, random, origin, podzolProbability)
            }
        }
    }

    /** The vanilla placed feature names this step draws. */
    public val PLACED_IDS: Set<String> = setOf("bamboo", "bamboo_light", "bamboo_vegetation")

    /** One clump at one position, which is what `/place feature` runs. */
    public fun placeAt(
        chunk: Chunk,
        random: WorldgenRandom,
        origin: DecorationPos,
        placedFeature: String,
    ): Boolean {
        // parity: `bamboo_light` places no podzol under it, the dense one a fifth of the time.
        val podzol = if (placedFeature == "bamboo_light") 0.0f else 0.2f
        if (placedFeature !in PLACED_IDS) return false
        place(BambooRegion(chunk, null, null), random, origin, podzol)
        return true
    }

    private fun place(
        region: BambooRegion,
        random: WorldgenRandom,
        origin: DecorationPos,
        podzolProbability: Float,
    ) {
        if (!origin.inWorld() ||
            !Blocks.isAir(region.get(origin) ?: return) ||
            !canBambooSurvive(region, origin)
        ) {
            return
        }

        val height = random.nextInt(12) + 5
        if (random.nextFloat() < podzolProbability) {
            val radius = random.nextInt(4) + 1
            for (x in origin.x - radius..origin.x + radius) {
                for (z in origin.z - radius..origin.z + radius) {
                    val dx = x - origin.x
                    val dz = z - origin.z
                    if (dx * dx + dz * dz <= radius * radius) {
                        val surfaceY = region.worldSurfaceHeight(x, z) - 1
                        val surface = DecorationPos(x, surfaceY, z)
                        if (surface.inWorld() &&
                            region.get(surface)?.let { Blocks.byStateId(it)?.block?.path in PODZOL_REPLACEABLE } == true
                        ) {
                            region.set(surface, Blocks.PODZOL.stateId)
                        }
                    }
                }
            }
        }

        var pos = origin
        var placed = 0
        while (placed < height && pos.inWorld() && region.get(pos)?.let(Blocks::isAir) == true) {
            region.set(pos, BAMBOO_TRUNK_STATE_ID)
            pos = pos.up()
            placed++
        }
        if (placed >= 3 && pos.inWorld()) {
            region.set(pos, BAMBOO_FINAL_LARGE_STATE_ID)
            region.set(pos.down(), BAMBOO_TOP_LARGE_STATE_ID)
            region.set(pos.down(2), BAMBOO_TOP_SMALL_STATE_ID)
        }
    }

    private fun canBambooSurvive(
        region: BambooRegion,
        origin: DecorationPos,
    ): Boolean =
        origin.y > WorldHeight.MIN_Y &&
            region.get(origin.down())?.let { stateId ->
                Blocks.byStateId(stateId)?.block?.path in SUPPORTS_BAMBOO
            } == true

    private fun DecorationPos.inWorld(): Boolean = y in WorldHeight.MIN_Y..WorldHeight.MAX_Y

    private fun DecorationPos.up(): DecorationPos = DecorationPos(x, y + 1, z)

    private fun DecorationPos.down(distance: Int = 1): DecorationPos = DecorationPos(x, y - distance, z)

    private class BambooRegion(
        private val chunk: Chunk,
        private val externalBlockState: ((x: Int, y: Int, z: Int) -> Int?)?,
        private val blockWriteObserver: ((x: Int, y: Int, z: Int, stateId: Int) -> Unit)?,
    ) {
        private val externalWrites = HashMap<DecorationPos, Int>()
        private val externalWorldSurfaceHeights = HashMap<Pair<Int, Int>, Int>()
        private val localWorldSurfaceHeights = chunk.heightmap(Chunk.HeightmapType.WORLD_SURFACE)

        fun get(position: DecorationPos): Int? =
            when {
                position.y !in WorldHeight.MIN_Y..WorldHeight.MAX_Y -> Blocks.AIR.stateId
                position.x shr 4 == chunk.chunkX && position.z shr 4 == chunk.chunkZ ->
                    chunk.getBlockStateId(position.x and 15, position.y, position.z and 15)
                // Outside the chunk this used to read as air, so anything needing a block beneath it
                // was refused everywhere but its own chunk. Falling through to the chunk reaches the
                // phase-wide neighbour reader, which is what vanilla's own level lookup sees.
                else ->
                    externalWrites[position] ?: externalBlockState?.invoke(position.x, position.y, position.z)
                        ?: chunk.getDecorationBlockStateId(position.x, position.y, position.z)
            }

        fun set(
            position: DecorationPos,
            stateId: Int,
        ) {
            if (position.y !in WorldHeight.MIN_Y..WorldHeight.MAX_Y) return
            if (position.x shr 4 == chunk.chunkX && position.z shr 4 == chunk.chunkZ) {
                chunk.setBlockStateId(position.x and 15, position.y, position.z and 15, stateId)
                val column = (position.x and 15) + (position.z and 15) * 16
                if (!Blocks.isAir(stateId) && position.y + 1 > localWorldSurfaceHeights[column]) {
                    localWorldSurfaceHeights[column] = position.y + 1
                }
            } else {
                setExternalDecorationBlockStateId(chunk, externalWrites, externalBlockState, position, stateId)
                val column = position.x to position.z
                val currentHeight =
                    externalWorldSurfaceHeights[column] ?: scanExternalWorldSurface(position.x, position.z)
                externalWorldSurfaceHeights[column] =
                    when {
                        !Blocks.isAir(stateId) && position.y + 1 > currentHeight -> position.y + 1
                        position.y + 1 == currentHeight && Blocks.isAir(stateId) ->
                            scanExternalWorldSurface(position.x, position.z)
                        else -> currentHeight
                    }
            }
            blockWriteObserver?.invoke(position.x, position.y, position.z, stateId)
        }

        fun worldSurfaceHeight(
            x: Int,
            z: Int,
        ): Int {
            if (x shr 4 == chunk.chunkX && z shr 4 == chunk.chunkZ) {
                return localWorldSurfaceHeights[(x and 15) + (z and 15) * 16]
            }
            return externalWorldSurfaceHeights[x to z] ?: scanExternalWorldSurface(x, z).also {
                externalWorldSurfaceHeights[x to z] = it
            }
        }

        private fun scanExternalWorldSurface(
            x: Int,
            z: Int,
        ): Int {
            for (y in WorldHeight.MAX_Y downTo WorldHeight.MIN_Y) {
                val position = DecorationPos(x, y, z)
                val stateId = externalWrites[position] ?: externalBlockState?.invoke(x, y, z) ?: Blocks.AIR.stateId
                if (!Blocks.isAir(stateId)) return y + 1
            }
            return WorldHeight.MIN_Y
        }
    }

    private val PODZOL_REPLACEABLE =
        setOf(
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
        )

    private val SUPPORTS_BAMBOO =
        PODZOL_REPLACEABLE +
            setOf(
                "sand",
                "red_sand",
                "suspicious_sand",
                "bamboo",
                "bamboo_sapling",
                "gravel",
                "suspicious_gravel",
            )

    private const val BAMBOO_INDEX = 2
    private const val BAMBOO_LIGHT_INDEX = 6
    private const val VEGETAL_DECORATION_STEP = 9
    private val BAMBOO_TRUNK_STATE_ID = Blocks.BAMBOO.stateId + 6
    private val BAMBOO_TOP_SMALL_STATE_ID = Blocks.BAMBOO.stateId + 8
    private val BAMBOO_TOP_LARGE_STATE_ID = Blocks.BAMBOO.stateId + 10
    private val BAMBOO_FINAL_LARGE_STATE_ID = Blocks.BAMBOO.stateId + 11
}
