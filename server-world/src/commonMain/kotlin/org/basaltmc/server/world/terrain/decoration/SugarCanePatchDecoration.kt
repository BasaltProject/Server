package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.registry.hasWaterFluid
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment

/** Exact step-9 placement for the four Overworld sugar-cane patches. */
public object SugarCanePatchDecoration {
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
            val x = chunk.chunkX * 16 + random.nextInt(16)
            val z = chunk.chunkZ * 16 + random.nextInt(16)
            val y = context.height(x and 15, z and 15, Chunk.HeightmapType.MOTION_BLOCKING)
            if (environment.biomeAt(x, y, z).substringAfter(':') !in spec.biomes) continue
            placeAt(random, DecorationPos(x, y, z), region)
        }
    }

    private fun placeAt(
        random: WorldgenRandom,
        origin: DecorationPos,
        region: PatchRegion,
    ) {
        repeat(20) {
            val target =
                DecorationPos(
                    origin.x + random.nextInt(5) - random.nextInt(5),
                    origin.y + random.nextInt(1) - random.nextInt(1),
                    origin.z + random.nextInt(5) - random.nextInt(5),
                )
            if (!canPlaceSugarCane(region, target)) return@repeat
            placeColumn(region, random, target)
        }
    }

    private fun canPlaceSugarCane(
        region: PatchRegion,
        position: DecorationPos,
    ): Boolean {
        if (position.y !in WorldHeight.MIN_Y..WorldHeight.MAX_Y || !Blocks.isAir(region.get(position))) return false
        if (position.y == WorldHeight.MIN_Y) return false
        val below = region.get(position.copy(y = position.y - 1))
        if (!isSugarCane(below) && Blocks.byStateId(below)?.block?.path !in SUGAR_CANE_SUPPORT) return false
        return HORIZONTAL_OFFSETS.any { (dx, dz) ->
            val adjacent = region.get(position.copy(x = position.x + dx, y = position.y - 1, z = position.z + dz))
            Blocks.hasWaterFluid(adjacent) || Blocks.byStateId(adjacent)?.block?.path == "frosted_ice"
        }
    }

    private fun placeColumn(
        region: PatchRegion,
        random: WorldgenRandom,
        origin: DecorationPos,
    ) {
        val height = 2 + random.nextInt(random.nextInt(3) + 1)
        var actualHeight = height
        for (dy in 1..height) {
            val target = origin.copy(y = origin.y + dy)
            if (target.y > WorldHeight.MAX_Y || !Blocks.isAir(region.get(target))) {
                actualHeight = dy - 1
                break
            }
        }
        repeat(actualHeight) { dy -> region.set(origin.copy(y = origin.y + dy), Blocks.SUGAR_CANE.stateId) }
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
                setExternalDecorationBlockStateId(chunk, externalWrites, externalBlockState, position, stateId)
            }
            blockWriteObserver?.invoke(position.x, position.y, position.z, stateId)
        }
    }

    private data class PatchSpec(
        val name: String,
        val index: Int,
        val rarity: Int?,
        val biomes: Set<String>,
    )

    private fun isSugarCane(stateId: Int): Boolean = Blocks.byStateId(stateId)?.block?.path == "sugar_cane"

    private val SUGAR_CANE_SUPPORT =
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
            "sand",
            "red_sand",
            "suspicious_sand",
        )
    private val HORIZONTAL_OFFSETS = listOf(1 to 0, -1 to 0, 0 to 1, 0 to -1)
    private val DEFAULT_BIOMES =
        setOf(
            "bamboo_jungle",
            "beach",
            "birch_forest",
            "cold_ocean",
            "dark_forest",
            "deep_cold_ocean",
            "deep_frozen_ocean",
            "deep_lukewarm_ocean",
            "deep_ocean",
            "flower_forest",
            "forest",
            "frozen_ocean",
            "frozen_river",
            "ice_spikes",
            "jungle",
            "lukewarm_ocean",
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
        )
    private val SPECS =
        listOf(
            PatchSpec("patch_sugar_cane_desert", 75, null, setOf("desert")),
            PatchSpec("patch_sugar_cane_badlands", 76, 5, setOf("badlands", "eroded_badlands", "wooded_badlands")),
            PatchSpec("patch_sugar_cane_swamp", 78, 3, setOf("swamp")),
            PatchSpec("patch_sugar_cane", 88, 6, DEFAULT_BIOMES),
        )
    private const val VEGETAL_DECORATION_STEP = 9
}
