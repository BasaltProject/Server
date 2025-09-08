package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.registry.upperDoublePlantStateId
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment
import org.basaltmc.wire.common.Identifier

/** Exact step-9 placement for the sunflower and large-fern patches. */
public object DoublePlantPatchDecoration {
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
            if (random.nextFloat() >= 1.0f / spec.rarity) continue
            val x = chunk.chunkX * 16 + random.nextInt(16)
            val z = chunk.chunkZ * 16 + random.nextInt(16)
            val y = context.height(x and 15, z and 15, Chunk.HeightmapType.MOTION_BLOCKING)
            if (environment.biomeAt(x, y, z).substringAfter(':') !in spec.biomes) continue
            val origin = DecorationPos(x, y, z)
            repeat(96) {
                placeCandidate(region, random, origin, spec)
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
        repeat(96) { placeCandidate(region, random, origin, spec) }
        return true
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
        if (target.y !in WorldHeight.MIN_Y until WorldHeight.MAX_Y || !Blocks.isAir(region.get(target))) return
        val upper = target.copy(y = target.y + 1)
        if (!Blocks.isAir(region.get(upper)) ||
            Blocks.byStateId(region.get(target.copy(y = target.y - 1)))?.block?.path !in SUPPORTS_VEGETATION
        ) {
            return
        }
        region.set(target, spec.lowerStateId)
        region.set(upper, spec.upperStateId)
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
        val rarity: Int,
        val biomes: Set<String>,
        val lowerStateId: Int,
        val upperStateId: Int,
    )

    private fun stateId(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")

    private fun spec(
        name: String,
        index: Int,
        rarity: Int,
        biomes: Set<String>,
        path: String,
    ): PatchSpec {
        val lower = stateId(path)
        val upper = Blocks.upperDoublePlantStateId(lower) ?: error("'$path' has no upper half")
        return PatchSpec(name, index, rarity, biomes, lower, upper)
    }

    private val SUPPORTS_VEGETATION =
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
            "farmland",
        )
    private val SPECS =
        listOf(
            spec("patch_sunflower", 34, 3, setOf("sunflower_plains"), "sunflower"),
            spec(
                "patch_large_fern",
                40,
                5,
                setOf("old_growth_pine_taiga", "old_growth_spruce_taiga", "snowy_taiga", "taiga"),
                "large_fern",
            ),
        )
    private const val VEGETAL_DECORATION_STEP = 9
}
