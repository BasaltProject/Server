package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.BlockFace
import org.basaltmc.server.registry.BlockShapes
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment
import org.basaltmc.wire.common.Identifier

/** Official step-9 placement shells for the seven lush-cave vegetation features. */
public object LushCaveVegetationDecoration {
    public fun decorate(
        chunk: Chunk,
        seed: Long,
        environment: SurfaceRuleEnvironment,
    ) {
        decorateSpecs(chunk, seed, environment, SPECS, null)
    }

    internal fun decorateFeature(
        chunk: Chunk,
        seed: Long,
        environment: SurfaceRuleEnvironment,
        name: String,
        placementObserver: ((DecorationPos) -> Unit)? = null,
    ) {
        decorateSpecs(chunk, seed, environment, listOf(SPECS.single { it.name == name }), placementObserver)
    }

    internal fun featureSpecs(): List<FeatureSpec> = SPECS

    private fun decorateSpecs(
        chunk: Chunk,
        seed: Long,
        environment: SurfaceRuleEnvironment,
        specs: List<FeatureSpec>,
        placementObserver: ((DecorationPos) -> Unit)?,
    ) {
        val random = WorldgenRandom(XoroshiroRandom(seed))
        val decorationSeed = random.setDecorationSeed(seed, chunk.chunkX * 16, chunk.chunkZ * 16)
        for (spec in specs) {
            random.setFeatureSeed(decorationSeed, spec.index, VEGETAL_DECORATION_STEP)
            val count =
                if (spec.minCount == spec.maxCount) {
                    spec.minCount
                } else {
                    spec.minCount + random.nextInt(spec.maxCount - spec.minCount + 1)
                }
            repeat(count) {
                val x = chunk.chunkX * 16 + random.nextInt(16)
                val z = chunk.chunkZ * 16 + random.nextInt(16)
                val y = WorldHeight.MIN_Y + random.nextInt(MAX_PLACEMENT_Y - WorldHeight.MIN_Y + 1)
                val position =
                    DecorationPos(x, y, z)
                val scanned = spec.scan?.let { scan(chunk, position, it) ?: return@repeat } ?: position
                val origin = scanned.copy(y = scanned.y + spec.yOffset)
                if (environment.biomeAt(origin.x, origin.y, origin.z).substringAfter(':') != "lush_caves") {
                    return@repeat
                }
                placementObserver?.invoke(origin)
                spec.place(chunk, random, origin)
            }
        }
    }

    private fun scan(
        chunk: Chunk,
        origin: DecorationPos,
        config: Scan,
    ): DecorationPos? {
        var position = origin
        if (!Blocks.isAir(blockAt(chunk, position))) return null
        for (step in 0 until config.maxSteps) {
            if (config.target.matches(blockAt(chunk, position))) return position
            position = position.copy(y = position.y + config.direction)
            if (position.y !in WorldHeight.MIN_Y..WorldHeight.MAX_Y) return null
            if (!Blocks.isAir(blockAt(chunk, position))) break
        }
        return position.takeIf { config.target.matches(blockAt(chunk, it)) }
    }

    private fun blockAt(
        chunk: Chunk,
        position: DecorationPos,
    ): Int = chunk.getDecorationBlockStateId(position.x, position.y, position.z)

    private fun placeSporeBlossom(
        chunk: Chunk,
        origin: DecorationPos,
    ) {
        if (!Blocks.isAir(blockAt(chunk, origin))) return
        val support = blockAt(chunk, origin.copy(y = origin.y + 1))
        if (!BlockShapes.isFaceSturdy(support, BlockFace.DOWN)) return
        chunk.setDecorationBlockStateId(origin.x, origin.y, origin.z, SPORE_BLOSSOM_STATE_ID)
    }

    internal data class FeatureSpec(
        val name: String,
        val index: Int,
        val minCount: Int,
        val maxCount: Int,
        val scan: Scan?,
        val yOffset: Int,
        val place: (Chunk, WorldgenRandom, DecorationPos) -> Unit,
    )

    internal data class Scan(
        val direction: Int,
        val maxSteps: Int,
        val target: Target,
    )

    internal enum class Target {
        SOLID,
        STURDY_DOWN,
        ;

        fun matches(stateId: Int): Boolean =
            when (this) {
                SOLID -> Chunk.HeightmapType.OCEAN_FLOOR.matches(stateId)
                STURDY_DOWN -> BlockShapes.isFaceSturdy(stateId, BlockFace.DOWN)
            }
    }

    private fun standalone(name: String): (Chunk, WorldgenRandom, DecorationPos) -> Unit =
        { chunk, random, origin -> StandaloneFeatures.placeAt(chunk, random, origin, name) }

    private val UP_SOLID = Scan(direction = 1, maxSteps = 12, target = Target.SOLID)
    private val UP_STURDY = Scan(direction = 1, maxSteps = 12, target = Target.STURDY_DOWN)
    private val DOWN_SOLID = Scan(direction = -1, maxSteps = 12, target = Target.SOLID)
    private val SPECS =
        listOf(
            FeatureSpec(
                "lush_caves_ceiling_vegetation",
                27,
                125,
                125,
                UP_SOLID,
                -1,
                LushVegetationPatchFeature::placeCeilingMoss,
            ),
            FeatureSpec("cave_vines", 28, 188, 188, UP_STURDY, -1) { chunk, random, origin ->
                CaveVineColumnFeature.place(chunk, random, origin)
            },
            FeatureSpec("lush_caves_clay", 29, 62, 62, DOWN_SOLID, 1, LushVegetationPatchFeature::placeClay),
            FeatureSpec(
                "lush_caves_vegetation",
                30,
                125,
                125,
                DOWN_SOLID,
                1,
                LushVegetationPatchFeature::placeFloorMoss,
            ),
            FeatureSpec("rooted_azalea_tree", 31, 1, 2, UP_SOLID, -1) { chunk, random, origin ->
                RootedAzaleaFeature.place(chunk, random, origin)
            },
            FeatureSpec("spore_blossom", 32, 25, 25, UP_SOLID, -1) { chunk, _, origin ->
                placeSporeBlossom(chunk, origin)
            },
            FeatureSpec("classic_vines_cave_feature", 33, 256, 256, null, 0) { chunk, _, origin ->
                VinePatchDecoration.placeCaveAt(chunk, origin)
            },
        )

    private val SPORE_BLOSSOM_STATE_ID =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla("spore_blossom"))?.stateId
            ?: error("unknown block 'spore_blossom'")
    private const val MAX_PLACEMENT_Y = 256
    private const val VEGETAL_DECORATION_STEP = 9
}
