package org.basaltmc.server.world.terrain.decoration.nether

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.terrain.NetherBiomeLookup
import org.basaltmc.wire.common.Identifier

/** // parity: Nether placed features in the VEGETAL_DECORATION step. */
public object NetherVegetalDecoration {
    public fun decorate(
        chunk: Chunk,
        seed: Long,
        biomeSource: NetherBiomeLookup,
    ) {
        val random = WorldgenRandom(XoroshiroRandom(seed))
        val decorationSeed = random.setDecorationSeed(seed, chunk.chunkX * 16, chunk.chunkZ * 16)
        for (globalIndex in FEATURE_ORDER.indices) {
            random.setFeatureSeed(decorationSeed, globalIndex, VEGETAL_DECORATION_STEP)
            placeFeature(chunk, random, biomeSource, globalIndex)
        }
    }

    internal fun decorateFeature(
        chunk: Chunk,
        seed: Long,
        biomeSource: NetherBiomeLookup,
        globalIndex: Int,
    ) {
        val random = WorldgenRandom(XoroshiroRandom(seed))
        val decorationSeed = random.setDecorationSeed(seed, chunk.chunkX * 16, chunk.chunkZ * 16)
        random.setFeatureSeed(decorationSeed, globalIndex, VEGETAL_DECORATION_STEP)
        placeFeature(chunk, random, biomeSource, globalIndex)
    }

    private fun placeFeature(
        chunk: Chunk,
        random: BasaltRandom,
        biomeSource: NetherBiomeLookup,
        globalIndex: Int,
    ) {
        // Dispatch by name: an index on its own says nothing about which fungus it is, and the
        // crimson and warped ones were crossed here for exactly that reason.
        when (FEATURE_ORDER.getOrNull(globalIndex)) {
            "spring_lava" -> placeLavaSprings(chunk, random, biomeSource)
            "brown_mushroom_normal" -> placeNormalMushroom(chunk, random, biomeSource, 256, "brown_mushroom")
            "red_mushroom_normal" -> placeNormalMushroom(chunk, random, biomeSource, 512, "red_mushroom")
            "warped_fungi" -> placeHugeFungi(chunk, random, biomeSource, HugeFungus.WARPED)
            "warped_forest_vegetation" -> placeForestVegetation(chunk, random, biomeSource, ForestVegetation.WARPED)
            "nether_sprouts" -> placeForestVegetation(chunk, random, biomeSource, ForestVegetation.SPROUTS)
            "twisting_vines" -> placeTwistingVines(chunk, random, biomeSource)
            "weeping_vines" -> placeWeepingVines(chunk, random, biomeSource)
            "crimson_fungi" -> placeHugeFungi(chunk, random, biomeSource, HugeFungus.CRIMSON)
            "crimson_forest_vegetation" -> placeForestVegetation(chunk, random, biomeSource, ForestVegetation.CRIMSON)
        }
    }

    /**
     * // parity: the order `FeatureSorter` gives this step, which is what each feature is seeded
     * with. Derived from the five Nether biomes in parameter-list order, so crimson_forest's
     * features are numbered before warped_forest's even though the warped ones sort first.
     */
    internal val FEATURE_ORDER: List<String> =
        listOf(
            "spring_lava",
            "brown_mushroom_normal",
            "red_mushroom_normal",
            "warped_fungi",
            "warped_forest_vegetation",
            "nether_sprouts",
            "twisting_vines",
            "weeping_vines",
            "crimson_fungi",
            "crimson_forest_vegetation",
        )

    /** The vanilla placed feature names this step draws. */
    public val PLACED_IDS: Set<String> =
        setOf(
            "crimson_fungi",
            "warped_fungi",
            "crimson_forest_vegetation",
            "warped_forest_vegetation",
            "nether_sprouts",
            "twisting_vines",
            "weeping_vines",
            "spring_lava_nether",
        )

    /**
     * One of them at one position, which is the feature without the modifiers that scatter it over
     * a chunk: what `/place feature` runs.
     */
    public fun placeAt(
        chunk: Chunk,
        random: BasaltRandom,
        x: Int,
        y: Int,
        z: Int,
        placedFeature: String,
    ): Boolean {
        val origin = Pos(x, y, z)
        when (placedFeature) {
            "crimson_fungi" -> placeHugeFungus(chunk, random, origin, HugeFungus.CRIMSON)
            "warped_fungi" -> placeHugeFungus(chunk, random, origin, HugeFungus.WARPED)
            "crimson_forest_vegetation" -> placeVegetationAt(chunk, random, origin, ForestVegetation.CRIMSON)
            "warped_forest_vegetation" -> placeVegetationAt(chunk, random, origin, ForestVegetation.WARPED)
            "nether_sprouts" -> placeVegetationAt(chunk, random, origin, ForestVegetation.SPROUTS)
            "twisting_vines" -> placeTwistingVinesFeature(chunk, random, origin)
            "weeping_vines" -> placeWeepingVinesFeature(chunk, random, origin)
            "spring_lava_nether" -> placeLavaSpring(chunk, x, y, z)
            else -> return false
        }
        return true
    }

    /** Shared by huge fungi and forest vegetation: candidate origins on every ground layer of a chunk. */
    internal fun positionsOnEveryLayer(
        chunk: Chunk,
        random: BasaltRandom,
        count: Int,
    ): List<Pos> {
        val positions = mutableListOf<Pos>()
        var layer = 0
        do {
            var foundAny = false
            repeat(count) {
                val x = chunk.chunkX * 16 + random.nextInt(16)
                val z = chunk.chunkZ * 16 + random.nextInt(16)
                val startY = chunk.heightAt(x and 15, z and 15, Chunk.HeightmapType.MOTION_BLOCKING)
                val y = findGroundLayer(chunk, x, startY, z, layer)
                if (y != null) {
                    positions += Pos(x, y, z)
                    foundAny = true
                }
            }
            layer++
        } while (foundAny)
        return positions
    }

    private fun findGroundLayer(
        chunk: Chunk,
        x: Int,
        startY: Int,
        z: Int,
        targetLayer: Int,
    ): Int? {
        var currentLayer = 0
        var currentState = stateAt(chunk, x, startY, z)
        for (y in startY downTo chunk.verticalBounds.minY + 1) {
            val below = stateAt(chunk, x, y - 1, z)
            val surface = below != null && !isLayerEmpty(below) && currentState != null && isLayerEmpty(currentState)
            if (surface && blockPath(below) != "bedrock") {
                if (currentLayer == targetLayer) return y
                currentLayer++
            }
            currentState = below
        }
        return null
    }

    private fun isLayerEmpty(stateId: Int): Boolean {
        val path = blockPath(stateId)
        return Blocks.isAir(stateId) || path == "water" || path == "lava"
    }

    internal fun terrainStateAt(
        chunk: Chunk,
        x: Int,
        y: Int,
        z: Int,
    ): Int? =
        if (isInChunk(chunk, x, z) && y in chunk.verticalBounds.minY..chunk.verticalBounds.maxY) {
            chunk.getBlockStateId(x and 15, y, z and 15)
        } else {
            null
        }

    internal fun stateAt(
        chunk: Chunk,
        x: Int,
        y: Int,
        z: Int,
    ): Int? =
        if (y in chunk.verticalBounds.minY..chunk.verticalBounds.maxY) {
            chunk.getDecorationBlockStateId(x, y, z)
        } else {
            null
        }

    internal fun blockPathAt(
        chunk: Chunk,
        x: Int,
        y: Int,
        z: Int,
    ): String? = stateAt(chunk, x, y, z)?.let(::blockPath)

    internal fun blockPath(stateId: Int): String? = Blocks.byStateId(stateId)?.block?.path

    internal fun isAir(
        chunk: Chunk,
        x: Int,
        y: Int,
        z: Int,
    ): Boolean = stateAt(chunk, x, y, z)?.let(Blocks::isAir) == true

    internal fun setBlock(
        chunk: Chunk,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
    ) {
        if (y in chunk.verticalBounds.minY..chunk.verticalBounds.maxY) {
            chunk.setDecorationBlockStateId(x, y, z, stateId)
        }
    }

    private fun isInChunk(
        chunk: Chunk,
        x: Int,
        z: Int,
    ): Boolean = x shr 4 == chunk.chunkX && z shr 4 == chunk.chunkZ

    internal fun triangle(
        random: BasaltRandom,
        range: Int,
    ): Int = random.nextInt(range + 1) - random.nextInt(range + 1)

    internal fun stateId(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId
            ?: error("missing vanilla block minecraft:$path")

    private const val VEGETAL_DECORATION_STEP = 9
}

internal data class Pos(
    val x: Int,
    val y: Int,
    val z: Int,
)
