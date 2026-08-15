package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment
import org.basaltmc.wire.common.Identifier

/** Exact step-9 placement for the eight small-mushroom placed features. */
public object MushroomPatchDecoration {
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
            place(context, region, random, environment, spec)
        }
    }

    private fun place(
        context: PlacementContext,
        region: PatchRegion,
        random: WorldgenRandom,
        environment: SurfaceRuleEnvironment,
        spec: PatchSpec,
    ) {
        repeat(spec.outerCount) {
            if (spec.rarity != 0 && random.nextFloat() >= 1.0f / spec.rarity) return@repeat
            val x = context.chunk.chunkX * 16 + random.nextInt(16)
            val z = context.chunk.chunkZ * 16 + random.nextInt(16)
            val y = context.height(x and 15, z and 15, Chunk.HeightmapType.MOTION_BLOCKING)
            if (environment.biomeAt(x, y, z).substringAfter(':') !in spec.biomes) return@repeat
            val origin = DecorationPos(x, y, z)
            repeat(96) {
                placeCandidate(region, random, origin, spec.stateId)
            }
        }
    }

    /** The vanilla placed feature names this step draws, which are the spec names themselves. */
    public val PLACED_IDS: Set<String> get() = SPECS.map(PatchSpec::name).toSet()

    /** The patch at one origin, which is what `/place feature` runs. */
    public fun placeAt(
        chunk: Chunk,
        random: WorldgenRandom,
        origin: DecorationPos,
        placedFeature: String,
    ): Boolean {
        val spec = SPECS.firstOrNull { it.name == placedFeature } ?: return false
        val region = PatchRegion(chunk, null, null)
        repeat(96) { placeCandidate(region, random, origin, spec.stateId) }
        return true
    }

    private fun placeCandidate(
        region: PatchRegion,
        random: WorldgenRandom,
        origin: DecorationPos,
        stateId: Int,
    ) {
        val target =
            DecorationPos(
                origin.x + random.nextInt(8) - random.nextInt(8),
                origin.y + random.nextInt(4) - random.nextInt(4),
                origin.z + random.nextInt(8) - random.nextInt(8),
            )
        if (target.y !in WorldHeight.MIN_Y..WorldHeight.MAX_Y || !Blocks.isAir(region.get(target))) return
        if (target.y == WorldHeight.MIN_Y || !isSolidRender(region.get(target.copy(y = target.y - 1)))) return
        region.set(target, stateId)
    }

    private fun isSolidRender(stateId: Int): Boolean {
        if (!Chunk.HeightmapType.OCEAN_FLOOR.matches(stateId) || Blocks.isLeavesState(stateId)) return false
        return Blocks.byStateId(stateId)?.block?.path !in NON_SOLID_RENDER_BLOCKS
    }

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
        val stateId: Int,
        val rarity: Int,
        val outerCount: Int,
        val biomes: Set<String>,
    )

    private fun stateId(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")

    private val OLD_GROWTH_BIOMES = setOf("old_growth_pine_taiga", "old_growth_spruce_taiga")
    private val SWAMP_BIOMES = setOf("swamp")
    private val TAIGA_BIOMES = setOf("mushroom_fields", "snowy_taiga", "taiga")
    private val NORMAL_BIOMES =
        setOf(
            "badlands",
            "bamboo_jungle",
            "beach",
            "birch_forest",
            "cold_ocean",
            "crimson_forest",
            "dark_forest",
            "deep_cold_ocean",
            "deep_dark",
            "deep_frozen_ocean",
            "deep_lukewarm_ocean",
            "deep_ocean",
            "desert",
            "dripstone_caves",
            "eroded_badlands",
            "flower_forest",
            "forest",
            "frozen_ocean",
            "frozen_river",
            "ice_spikes",
            "jungle",
            "lukewarm_ocean",
            "nether_wastes",
            "ocean",
            "old_growth_birch_forest",
            "old_growth_pine_taiga",
            "old_growth_spruce_taiga",
            "plains",
            "river",
            "savanna",
            "savanna_plateau",
            "snowy_beach",
            "snowy_plains",
            "sparse_jungle",
            "stony_shore",
            "sunflower_plains",
            "swamp",
            "warm_ocean",
            "warped_forest",
            "windswept_forest",
            "windswept_gravelly_hills",
            "windswept_hills",
            "windswept_savanna",
            "wooded_badlands",
        )
    private val BROWN_MUSHROOM_STATE_ID = stateId("brown_mushroom")
    private val RED_MUSHROOM_STATE_ID = stateId("red_mushroom")
    private val SPECS =
        listOf(
            PatchSpec("brown_mushroom_old_growth", 63, BROWN_MUSHROOM_STATE_ID, 4, 3, OLD_GROWTH_BIOMES),
            PatchSpec("red_mushroom_old_growth", 64, RED_MUSHROOM_STATE_ID, 171, 1, OLD_GROWTH_BIOMES),
            PatchSpec("brown_mushroom_swamp", 66, BROWN_MUSHROOM_STATE_ID, 0, 2, SWAMP_BIOMES),
            PatchSpec("red_mushroom_swamp", 67, RED_MUSHROOM_STATE_ID, 64, 1, SWAMP_BIOMES),
            PatchSpec("brown_mushroom_normal", 73, BROWN_MUSHROOM_STATE_ID, 256, 1, NORMAL_BIOMES),
            PatchSpec("red_mushroom_normal", 74, RED_MUSHROOM_STATE_ID, 512, 1, NORMAL_BIOMES),
            PatchSpec("brown_mushroom_taiga", 80, BROWN_MUSHROOM_STATE_ID, 4, 1, TAIGA_BIOMES),
            PatchSpec("red_mushroom_taiga", 81, RED_MUSHROOM_STATE_ID, 256, 1, TAIGA_BIOMES),
        )
    private val NON_SOLID_RENDER_BLOCKS =
        setOf(
            "snow",
            "cactus",
            "cactus_flower",
            "sugar_cane",
            "bamboo",
            "bamboo_sapling",
            "pointed_dripstone",
            "lily_pad",
            "brown_mushroom",
            "red_mushroom",
            "fern",
            "large_fern",
            "dead_bush",
            "sweet_berry_bush",
            "firefly_bush",
            "wildflowers",
            "pink_petals",
            "pale_moss_carpet",
            "closed_eyeblossom",
            "open_eyeblossom",
            "sunflower",
            "lilac",
            "rose_bush",
            "peony",
        )
    private const val VEGETAL_DECORATION_STEP = 9
}
