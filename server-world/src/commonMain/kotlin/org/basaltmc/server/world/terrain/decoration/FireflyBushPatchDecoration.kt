package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment
import org.basaltmc.wire.common.Identifier

/** Exact step-9 placement for the three Overworld firefly-bush patches. */
public object FireflyBushPatchDecoration {
    public fun decorate(
        chunk: Chunk,
        seed: Long,
        environment: SurfaceRuleEnvironment,
        externalBlockState: ((x: Int, y: Int, z: Int) -> Int?)? = null,
        blockWriteObserver: ((x: Int, y: Int, z: Int, stateId: Int) -> Unit)? = null,
    ) {
        decorateSpecs(chunk, seed, environment, SPECS, externalBlockState, blockWriteObserver)
    }

    internal fun decorateFeature(
        chunk: Chunk,
        seed: Long,
        environment: SurfaceRuleEnvironment,
        name: String,
        externalBlockState: ((x: Int, y: Int, z: Int) -> Int?)? = null,
        blockWriteObserver: ((x: Int, y: Int, z: Int, stateId: Int) -> Unit)? = null,
    ) {
        decorateSpecs(
            chunk,
            seed,
            environment,
            listOf(SPECS.single { it.name == name }),
            externalBlockState,
            blockWriteObserver,
        )
    }

    private fun decorateSpecs(
        chunk: Chunk,
        seed: Long,
        environment: SurfaceRuleEnvironment,
        specs: List<PatchSpec>,
        externalBlockState: ((x: Int, y: Int, z: Int) -> Int?)?,
        blockWriteObserver: ((x: Int, y: Int, z: Int, stateId: Int) -> Unit)?,
    ) {
        val random = WorldgenRandom(XoroshiroRandom(seed))
        val decorationSeed = random.setDecorationSeed(seed, chunk.chunkX * 16, chunk.chunkZ * 16)
        val context = PlacementContext(chunk)
        val region = PatchRegion(chunk, externalBlockState, blockWriteObserver)
        for (spec in specs) {
            random.setFeatureSeed(decorationSeed, spec.index, VEGETAL_DECORATION_STEP)
            if (spec.rarity != null && random.nextFloat() >= 1.0f / spec.rarity) continue
            repeat(spec.outerCount) {
                val x = chunk.chunkX * 16 + random.nextInt(16)
                val z = chunk.chunkZ * 16 + random.nextInt(16)
                val y = context.height(x and 15, z and 15, spec.heightmap)
                val origin = DecorationPos(x, y, z)
                if (environment.biomeAt(x, y, z).substringAfter(':') !in spec.biomes) return@repeat
                if (spec.nearWater && !isNearWaterOrigin(region, origin)) return@repeat
                placeCandidates(random, origin, region)
            }
        }
    }

    private fun placeCandidates(
        random: WorldgenRandom,
        origin: DecorationPos,
        region: PatchRegion,
    ) {
        repeat(20) {
            val target =
                DecorationPos(
                    origin.x + random.nextInt(5) - random.nextInt(5),
                    origin.y + random.nextInt(4) - random.nextInt(4),
                    origin.z + random.nextInt(5) - random.nextInt(5),
                )
            if (!Blocks.isAir(region.get(target)) || !canBushSurvive(region, target)) return@repeat
            region.set(target, Blocks.FIREFLY_BUSH.stateId)
        }
    }

    private fun isNearWaterOrigin(
        region: PatchRegion,
        position: DecorationPos,
    ): Boolean {
        if (!Blocks.isAir(region.get(position)) || !canBushSurvive(region, position)) return false
        return HORIZONTAL_OFFSETS.any { (dx, dz) ->
            Blocks.hasWaterFluid(
                region.get(position.copy(x = position.x + dx, y = position.y - 1, z = position.z + dz)),
            )
        }
    }

    private fun canBushSurvive(
        region: PatchRegion,
        position: DecorationPos,
    ): Boolean =
        position.y > WorldHeight.MIN_Y &&
            region.get(position.copy(y = position.y - 1)) in SUPPORTS_VEGETATION

    private class PatchRegion(
        private val chunk: Chunk,
        private val externalBlockState: ((x: Int, y: Int, z: Int) -> Int?)?,
        private val blockWriteObserver: ((x: Int, y: Int, z: Int, stateId: Int) -> Unit)?,
    ) {
        private val externalWrites = HashMap<DecorationPos, Int>()

        fun get(position: DecorationPos): Int =
            when {
                position.y !in WorldHeight.MIN_Y..WorldHeight.MAX_Y -> Blocks.AIR.stateId
                position.x shr 4 == chunk.chunkX && position.z shr 4 == chunk.chunkZ ->
                    chunk.getBlockStateId(position.x and 15, position.y, position.z and 15)
                else ->
                    externalWrites[position] ?: externalBlockState?.invoke(position.x, position.y, position.z)
                        ?: Blocks.AIR.stateId
            }

        fun set(
            position: DecorationPos,
            stateId: Int,
        ) {
            if (position.x shr 4 == chunk.chunkX && position.z shr 4 == chunk.chunkZ) {
                chunk.setBlockStateId(position.x and 15, position.y, position.z and 15, stateId)
            } else {
                externalWrites[position] = stateId
            }
            blockWriteObserver?.invoke(position.x, position.y, position.z, stateId)
        }
    }

    private data class PatchSpec(
        val name: String,
        val index: Int,
        val rarity: Int?,
        val outerCount: Int,
        val heightmap: Chunk.HeightmapType,
        val nearWater: Boolean,
        val biomes: Set<String>,
    )

    private fun states(vararg names: String): Set<Int> =
        names
            .map { name ->
                Blocks.REGISTRY.byIdentifier(Identifier.vanilla(name))?.stateId ?: error("unknown block '$name'")
            }.toSet()

    private val SUPPORTS_VEGETATION =
        states(
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
            "farmland",
        )
    private val HORIZONTAL_OFFSETS = listOf(1 to 0, -1 to 0, 0 to 1, 0 to -1)
    private val DEFAULT_BIOMES =
        setOf(
            "badlands",
            "bamboo_jungle",
            "beach",
            "birch_forest",
            "cold_ocean",
            "dark_forest",
            "deep_cold_ocean",
            "deep_frozen_ocean",
            "deep_lukewarm_ocean",
            "deep_ocean",
            "eroded_badlands",
            "flower_forest",
            "forest",
            "frozen_ocean",
            "frozen_river",
            "ice_spikes",
            "jungle",
            "lukewarm_ocean",
            "mangrove_swamp",
            "mushroom_fields",
            "ocean",
            "old_growth_birch_forest",
            "old_growth_pine_taiga",
            "old_growth_spruce_taiga",
            "pale_garden",
            "plains",
            "river",
            "savanna",
            "savanna_plateau",
            "snowy_beach",
            "snowy_plains",
            "snowy_taiga",
            "sparse_jungle",
            "stony_shore",
            "sunflower_plains",
            "taiga",
            "warm_ocean",
            "windswept_forest",
            "windswept_gravelly_hills",
            "windswept_hills",
            "windswept_savanna",
            "wooded_badlands",
        )
    private val SPECS =
        listOf(
            PatchSpec(
                "patch_firefly_bush_swamp",
                85,
                8,
                1,
                Chunk.HeightmapType.MOTION_BLOCKING,
                false,
                setOf("swamp"),
            ),
            PatchSpec(
                "patch_firefly_bush_near_water_swamp",
                86,
                null,
                3,
                Chunk.HeightmapType.MOTION_BLOCKING,
                true,
                setOf("swamp"),
            ),
            PatchSpec(
                "patch_firefly_bush_near_water",
                89,
                null,
                2,
                Chunk.HeightmapType.MOTION_BLOCKING_NO_LEAVES,
                true,
                DEFAULT_BIOMES,
            ),
        )
    private const val VEGETAL_DECORATION_STEP = 9
}
