package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment
import org.basaltmc.wire.common.Identifier

/** Exact step-9 placement for the five dead-bush and dry-grass patch placed features. */
public object GroundPlantPatchDecoration {
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
            val y = context.height(x and 15, z and 15, spec.heightmap)
            val origin = DecorationPos(x, y, z)
            if (environment.biomeAt(x, y, z).substringAfter(':') !in spec.biomes) return@repeat
            repeat(spec.innerCount) {
                placeCandidate(region, random, origin, spec)
            }
        }
    }

    private fun placeCandidate(
        region: PatchRegion,
        random: WorldgenRandom,
        origin: DecorationPos,
        spec: PatchSpec,
    ) {
        val target =
            DecorationPos(
                origin.x + random.nextInt(8) - random.nextInt(8),
                origin.y + random.nextInt(4) - random.nextInt(4),
                origin.z + random.nextInt(8) - random.nextInt(8),
            )
        if (target.y !in WorldHeight.MIN_Y..WorldHeight.MAX_Y || !Blocks.isAir(region.get(target))) return
        val stateId = spec.provider.state(random)
        if (target.y == WorldHeight.MIN_Y || region.get(target.down()) !in SUPPORTS_DRY_VEGETATION) return
        region.set(target, stateId)
    }

    private fun DecorationPos.down(): DecorationPos = copy(y = y - 1)

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

    private enum class Provider {
        DEAD_BUSH,
        DRY_GRASS,
        ;

        fun state(random: WorldgenRandom): Int =
            when (this) {
                DEAD_BUSH -> DEAD_BUSH_STATE_ID
                DRY_GRASS -> if (random.nextInt(2) == 0) SHORT_DRY_GRASS_STATE_ID else TALL_DRY_GRASS_STATE_ID
            }
    }

    private data class PatchSpec(
        val name: String,
        val index: Int,
        val outerCount: Int,
        val innerCount: Int,
        val biomes: Set<String>,
        val provider: Provider,
        val rarity: Int = 0,
        val heightmap: Chunk.HeightmapType = Chunk.HeightmapType.WORLD_SURFACE,
    )

    private fun states(vararg names: String): Set<Int> =
        names
            .map { name ->
                Blocks.REGISTRY.byIdentifier(Identifier.vanilla(name))?.stateId ?: error("unknown block '$name'")
            }.toSet()

    private val SUPPORTS_DRY_VEGETATION =
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
            "sand",
            "red_sand",
            "suspicious_sand",
            "terracotta",
            "white_terracotta",
            "orange_terracotta",
            "magenta_terracotta",
            "light_blue_terracotta",
            "yellow_terracotta",
            "lime_terracotta",
            "pink_terracotta",
            "gray_terracotta",
            "light_gray_terracotta",
            "cyan_terracotta",
            "purple_terracotta",
            "blue_terracotta",
            "brown_terracotta",
            "green_terracotta",
            "red_terracotta",
            "black_terracotta",
        )
    private val DEAD_BUSH_STATE_ID = Blocks.REGISTRY.byIdentifier(Identifier.vanilla("dead_bush"))!!.stateId
    private val SHORT_DRY_GRASS_STATE_ID = Blocks.REGISTRY.byIdentifier(Identifier.vanilla("short_dry_grass"))!!.stateId
    private val TALL_DRY_GRASS_STATE_ID = Blocks.REGISTRY.byIdentifier(Identifier.vanilla("tall_dry_grass"))!!.stateId
    private val SPECS =
        listOf(
            PatchSpec(
                "patch_dead_bush",
                62,
                1,
                4,
                setOf("mangrove_swamp", "old_growth_pine_taiga", "old_growth_spruce_taiga", "swamp"),
                Provider.DEAD_BUSH,
            ),
            PatchSpec(
                "patch_dry_grass_desert",
                69,
                1,
                64,
                setOf("desert"),
                Provider.DRY_GRASS,
                rarity = 3,
                heightmap = Chunk.HeightmapType.MOTION_BLOCKING,
            ),
            PatchSpec("patch_dead_bush_2", 70, 2, 4, setOf("desert"), Provider.DEAD_BUSH),
            PatchSpec(
                "patch_dry_grass_badlands",
                71,
                1,
                64,
                setOf("badlands", "eroded_badlands", "wooded_badlands"),
                Provider.DRY_GRASS,
                rarity = 6,
                heightmap = Chunk.HeightmapType.MOTION_BLOCKING,
            ),
            PatchSpec(
                "patch_dead_bush_badlands",
                72,
                20,
                4,
                setOf("badlands", "eroded_badlands", "wooded_badlands"),
                Provider.DEAD_BUSH,
            ),
        )
    private const val VEGETAL_DECORATION_STEP = 9
}
