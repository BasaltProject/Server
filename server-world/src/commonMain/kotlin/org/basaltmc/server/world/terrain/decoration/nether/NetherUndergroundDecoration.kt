package org.basaltmc.server.world.terrain.decoration.nether

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.terrain.NetherBiomeSource
import org.basaltmc.wire.common.Identifier
import kotlin.math.abs

/** // parity: Nether placed features in the UNDERGROUND_DECORATION step. */
public object NetherUndergroundDecoration {
    public fun decorate(
        chunk: Chunk,
        seed: Long,
        biomeSource: NetherBiomeSource,
    ) {
        val random = WorldgenRandom(XoroshiroRandom(seed))
        val decorationSeed = random.setDecorationSeed(seed, chunk.chunkX * 16, chunk.chunkZ * 16)
        for (globalIndex in 0..22) {
            random.setFeatureSeed(decorationSeed, globalIndex, UNDERGROUND_DECORATION_STEP)
            if (NetherOreDecoration.placeFeature(globalIndex, chunk, random, biomeSource)) continue
            placeFeature(globalIndex, chunk, random, biomeSource)
        }
    }

    internal fun decorateFeature(
        chunk: Chunk,
        seed: Long,
        biomeSource: NetherBiomeSource,
        globalIndex: Int,
    ) {
        val random = WorldgenRandom(XoroshiroRandom(seed))
        val decorationSeed = random.setDecorationSeed(seed, chunk.chunkX * 16, chunk.chunkZ * 16)
        random.setFeatureSeed(decorationSeed, globalIndex, UNDERGROUND_DECORATION_STEP)
        if (!NetherOreDecoration.placeFeature(globalIndex, chunk, random, biomeSource)) {
            placeFeature(globalIndex, chunk, random, biomeSource)
        }
    }

    private fun placeFeature(
        globalIndex: Int,
        chunk: Chunk,
        random: WorldgenRandom,
        biomeSource: NetherBiomeSource,
    ) {
        when (globalIndex) {
            0 -> placeReplaceBlobs(chunk, random, biomeSource, 75, "basalt")
            1 -> placeReplaceBlobs(chunk, random, biomeSource, 25, "blackstone")
            2 -> placeSprings(chunk, random, biomeSource, 16, 4, 123, DELTA_BIOME, Spring.DELTA)
            3 -> placeSprings(chunk, random, biomeSource, 8, 4, 123, NON_DELTA_BIOMES, Spring.OPEN)
            4 -> placeFirePatch(chunk, random, biomeSource, ALL_BIOMES, "netherrack", "fire")
            5 -> placeFirePatch(chunk, random, biomeSource, SOUL_FIRE_BIOMES, "soul_soil", "soul_fire")
            6 -> placeGlowstoneExtra(chunk, random, biomeSource)
            7 -> placeGlowstone(chunk, random, biomeSource, 10, 0, 127)
            8 -> placeCrimsonRootsPatch(chunk, random, biomeSource)
            9 -> placeMushroomPatch(chunk, random, biomeSource, "brown_mushroom")
            10 -> placeMushroomPatch(chunk, random, biomeSource, "red_mushroom")
            12 -> placeSprings(chunk, random, biomeSource, 32, 10, 117, DELTA_BIOME, Spring.CLOSED)
            15 -> placeSprings(chunk, random, biomeSource, 16, 10, 117, NON_DELTA_BIOMES, Spring.CLOSED)
        }
    }

    /** The vanilla placed feature names this step draws. */
    public val PLACED_IDS: Set<String> =
        setOf(
            "basalt_blobs",
            "blackstone_blobs",
            "spring_delta",
            "spring_open",
            "patch_fire",
            "patch_soul_fire",
            "glowstone_extra",
            "glowstone",
            "patch_crimson_roots",
            "brown_mushroom_nether",
            "red_mushroom_nether",
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
        when (placedFeature) {
            "basalt_blobs" -> replaceBlob(chunk, random, x, y, z, "basalt")
            "blackstone_blobs" -> replaceBlob(chunk, random, x, y, z, "blackstone")
            "spring_delta" -> placeSpring(chunk, x, y, z, Spring.DELTA)
            "spring_open" -> placeSpring(chunk, x, y, z, Spring.OPEN)
            "patch_fire" -> placePatchAt(chunk, random, x, y, z, "netherrack", "fire")
            "patch_soul_fire" -> placePatchAt(chunk, random, x, y, z, "soul_soil", "soul_fire")
            "glowstone_extra", "glowstone" -> placeGlowstoneBlob(chunk, random, x, y, z)
            "patch_crimson_roots" -> placeRootsAt(chunk, random, x, y, z)
            "brown_mushroom_nether" -> placeMushroomAt(chunk, random, x, y, z, "brown_mushroom")
            "red_mushroom_nether" -> placeMushroomAt(chunk, random, x, y, z, "red_mushroom")
            else -> return false
        }
        return true
    }

    /** The fire patch itself, around an origin the caller chose. */
    private fun placePatchAt(
        chunk: Chunk,
        random: BasaltRandom,
        baseX: Int,
        baseY: Int,
        baseZ: Int,
        support: String,
        fire: String,
    ) {
        repeat(PATCH_ATTEMPTS) {
            val x = baseX + triangle(random, 7)
            val y = baseY + triangle(random, 3)
            val z = baseZ + triangle(random, 7)
            if (isAir(chunk, x, y, z) && blockPathAt(chunk, x, y - 1, z) == support) {
                setBlock(chunk, x, y, z, stateId(fire))
            }
        }
    }

    private fun placeRootsAt(
        chunk: Chunk,
        random: BasaltRandom,
        baseX: Int,
        baseY: Int,
        baseZ: Int,
    ) {
        repeat(PATCH_ATTEMPTS) {
            val x = baseX + triangle(random, 7)
            val y = baseY + triangle(random, 3)
            val z = baseZ + triangle(random, 7)
            val support = blockPathAt(chunk, x, y - 1, z)
            if (isAir(chunk, x, y, z) && support in CRIMSON_ROOT_SUPPORT) {
                setBlock(chunk, x, y, z, stateId("crimson_roots"))
            }
        }
    }

    private fun placeMushroomAt(
        chunk: Chunk,
        random: BasaltRandom,
        baseX: Int,
        baseY: Int,
        baseZ: Int,
        mushroom: String,
    ) {
        repeat(PATCH_ATTEMPTS) {
            val x = baseX + triangle(random, 7)
            val y = baseY + triangle(random, 3)
            val z = baseZ + triangle(random, 7)
            val support = blockPathAt(chunk, x, y - 1, z)
            if (isAir(chunk, x, y, z) && support != null && supportsMushroom(support)) {
                setBlock(chunk, x, y, z, stateId(mushroom))
            }
        }
    }

    private fun placeReplaceBlobs(
        chunk: Chunk,
        random: BasaltRandom,
        biomeSource: NetherBiomeSource,
        count: Int,
        replacement: String,
    ) {
        repeat(count) {
            val x = chunk.chunkX * 16 + random.nextInt(16)
            val z = chunk.chunkZ * 16 + random.nextInt(16)
            val y = random.nextInt(128)
            if (!hasBiome(biomeSource, x, y, z, DELTA_BIOME)) return@repeat
            replaceBlob(chunk, random, x, y.coerceIn(1, chunk.verticalBounds.maxY), z, replacement)
        }
    }

    private fun replaceBlob(
        chunk: Chunk,
        random: BasaltRandom,
        x: Int,
        startY: Int,
        z: Int,
        replacement: String,
    ) {
        var centerY = startY
        while (centerY > chunk.verticalBounds.minY + 1 && blockPathAt(chunk, x, centerY, z) != "netherrack") {
            centerY--
        }
        if (centerY <= chunk.verticalBounds.minY + 1) return

        val radiusX = 3 + random.nextInt(5)
        val radiusY = 3 + random.nextInt(5)
        val radiusZ = 3 + random.nextInt(5)
        val maximumRadius = maxOf(radiusX, radiusY, radiusZ)
        val replacementState = stateId(replacement)
        for (offsetX in -radiusX..radiusX) {
            for (offsetY in -radiusY..radiusY) {
                for (offsetZ in -radiusZ..radiusZ) {
                    if (abs(offsetX) + abs(offsetY) + abs(offsetZ) > maximumRadius) continue
                    val worldX = x + offsetX
                    val worldY = centerY + offsetY
                    val worldZ = z + offsetZ
                    if (blockPathAt(chunk, worldX, worldY, worldZ) == "netherrack") {
                        setBlock(chunk, worldX, worldY, worldZ, replacementState)
                    }
                }
            }
        }
    }

    private fun placeSprings(
        chunk: Chunk,
        random: BasaltRandom,
        biomeSource: NetherBiomeSource,
        count: Int,
        minY: Int,
        maxY: Int,
        biomes: Set<String>,
        spring: Spring,
    ) {
        repeat(count) {
            val x = chunk.chunkX * 16 + random.nextInt(16)
            val z = chunk.chunkZ * 16 + random.nextInt(16)
            val y = uniform(random, minY, maxY)
            if (hasBiome(biomeSource, x, y, z, biomes)) placeSpring(chunk, x, y, z, spring)
        }
    }

    private fun placeSpring(
        chunk: Chunk,
        x: Int,
        y: Int,
        z: Int,
        spring: Spring,
    ) {
        if (!spring.isValid(terrainBlockPathAt(chunk, x, y + 1, z))) return
        if (spring.requiresBlockBelow && !spring.isValid(terrainBlockPathAt(chunk, x, y - 1, z))) return
        val current = terrainStateAt(chunk, x, y, z) ?: return
        if (!Blocks.isAir(current) && !spring.isValid(terrainBlockPathAt(chunk, x, y, z))) return

        var rockCount = 0
        var holeCount = 0
        for ((offsetX, offsetY, offsetZ) in SPRING_NEIGHBOURS) {
            val neighbour = terrainStateAt(chunk, x + offsetX, y + offsetY, z + offsetZ)
            if (spring.isValid(neighbour?.let(::blockPath))) rockCount++
            if (neighbour != null && Blocks.isAir(neighbour)) holeCount++
        }
        if (rockCount == spring.rockCount && holeCount == spring.holeCount) {
            setBlock(chunk, x, y, z, Blocks.LAVA.stateId)
        }
    }

    private fun placeFirePatch(
        chunk: Chunk,
        random: BasaltRandom,
        biomeSource: NetherBiomeSource,
        biomes: Set<String>,
        support: String,
        fire: String,
    ) {
        repeat(random.nextInt(6)) {
            val baseX = chunk.chunkX * 16 + random.nextInt(16)
            val baseZ = chunk.chunkZ * 16 + random.nextInt(16)
            val baseY = uniform(random, 4, 123)
            if (!hasBiome(biomeSource, baseX, baseY, baseZ, biomes)) return@repeat
            repeat(PATCH_ATTEMPTS) {
                val x = baseX + triangle(random, 7)
                val y = baseY + triangle(random, 3)
                val z = baseZ + triangle(random, 7)
                if (isAir(chunk, x, y, z) && blockPathAt(chunk, x, y - 1, z) == support) {
                    setBlock(chunk, x, y, z, stateId(fire))
                }
            }
        }
    }

    private fun placeGlowstoneExtra(
        chunk: Chunk,
        random: BasaltRandom,
        biomeSource: NetherBiomeSource,
    ) {
        val count = random.nextInt(random.nextInt(10) + 1)
        placeGlowstone(chunk, random, biomeSource, count, 4, 123)
    }

    private fun placeGlowstone(
        chunk: Chunk,
        random: BasaltRandom,
        biomeSource: NetherBiomeSource,
        count: Int,
        minY: Int,
        maxY: Int,
    ) {
        repeat(count) {
            val x = chunk.chunkX * 16 + random.nextInt(16)
            val z = chunk.chunkZ * 16 + random.nextInt(16)
            val y = uniform(random, minY, maxY)
            if (hasBiome(biomeSource, x, y, z, ALL_BIOMES)) placeGlowstoneBlob(chunk, random, x, y, z)
        }
    }

    private fun placeGlowstoneBlob(
        chunk: Chunk,
        random: BasaltRandom,
        originX: Int,
        originY: Int,
        originZ: Int,
    ) {
        if (!isAir(chunk, originX, originY, originZ)) return
        if (blockPathAt(chunk, originX, originY + 1, originZ) !in GLOWSTONE_SUPPORT) return
        val glowstone = stateId("glowstone")
        setBlock(chunk, originX, originY, originZ, glowstone)
        repeat(1500) {
            val x = originX + random.nextInt(8) - random.nextInt(8)
            val y = originY - random.nextInt(12)
            val z = originZ + random.nextInt(8) - random.nextInt(8)
            if (!isAir(chunk, x, y, z)) return@repeat
            var neighbours = 0
            for ((offsetX, offsetY, offsetZ) in ALL_NEIGHBOURS) {
                if (blockPathAt(chunk, x + offsetX, y + offsetY, z + offsetZ) == "glowstone") neighbours++
                if (neighbours > 1) break
            }
            if (neighbours == 1) setBlock(chunk, x, y, z, glowstone)
        }
    }

    private fun placeCrimsonRootsPatch(
        chunk: Chunk,
        random: BasaltRandom,
        biomeSource: NetherBiomeSource,
    ) {
        val baseX = chunk.chunkX * 16
        val baseZ = chunk.chunkZ * 16
        val baseY = random.nextInt(128)
        if (!hasBiome(biomeSource, baseX, baseY, baseZ, SOUL_BIOME)) return
        repeat(PATCH_ATTEMPTS) {
            val x = baseX + triangle(random, 7)
            val y = baseY + triangle(random, 3)
            val z = baseZ + triangle(random, 7)
            val support = blockPathAt(chunk, x, y - 1, z)
            if (isAir(chunk, x, y, z) && support in CRIMSON_ROOT_SUPPORT) {
                setBlock(chunk, x, y, z, stateId("crimson_roots"))
            }
        }
    }

    private fun placeMushroomPatch(
        chunk: Chunk,
        random: BasaltRandom,
        biomeSource: NetherBiomeSource,
        mushroom: String,
    ) {
        if (random.nextFloat() >= 0.5f) return
        val baseX = chunk.chunkX * 16 + random.nextInt(16)
        val baseZ = chunk.chunkZ * 16 + random.nextInt(16)
        val baseY = random.nextInt(128)
        if (!hasBiome(biomeSource, baseX, baseY, baseZ, NETHER_MUSHROOM_BIOMES)) return
        repeat(PATCH_ATTEMPTS) {
            val x = baseX + triangle(random, 7)
            val y = baseY + triangle(random, 3)
            val z = baseZ + triangle(random, 7)
            val support = blockPathAt(chunk, x, y - 1, z)
            if (isAir(chunk, x, y, z) && support != null && supportsMushroom(support)) {
                setBlock(chunk, x, y, z, stateId(mushroom))
            }
        }
    }

    private fun supportsMushroom(path: String): Boolean =
        path in MUSHROOM_LIGHT_OVERRIDE || path in MUSHROOM_SOLID_SUPPORT

    private fun hasBiome(
        biomeSource: NetherBiomeSource,
        x: Int,
        y: Int,
        z: Int,
        biomes: Set<String>,
    ): Boolean = biomeSource.biomeAt(x, y, z).id.path in biomes

    private fun stateAt(
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

    private fun terrainStateAt(
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

    private fun terrainBlockPathAt(
        chunk: Chunk,
        x: Int,
        y: Int,
        z: Int,
    ): String? = terrainStateAt(chunk, x, y, z)?.let(::blockPath)

    private fun blockPathAt(
        chunk: Chunk,
        x: Int,
        y: Int,
        z: Int,
    ): String? = stateAt(chunk, x, y, z)?.let(::blockPath)

    private fun blockPath(stateId: Int): String? = Blocks.byStateId(stateId)?.block?.path

    private fun isAir(
        chunk: Chunk,
        x: Int,
        y: Int,
        z: Int,
    ): Boolean = stateAt(chunk, x, y, z)?.let(Blocks::isAir) == true

    private fun setBlock(
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

    private fun stateId(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId
            ?: error("missing vanilla block minecraft:$path")

    private fun uniform(
        random: BasaltRandom,
        min: Int,
        max: Int,
    ): Int = min + random.nextInt(max - min + 1)

    private fun triangle(
        random: BasaltRandom,
        range: Int,
    ): Int = random.nextInt(range + 1) - random.nextInt(range + 1)

    private enum class Spring(
        val validBlocks: Set<String>,
        val requiresBlockBelow: Boolean,
        val rockCount: Int,
        val holeCount: Int,
    ) {
        DELTA(SPRING_LAVA_SUPPORT, true, 4, 1),
        OPEN(setOf("netherrack"), false, 4, 1),
        CLOSED(setOf("netherrack"), false, 5, 0),
        ;

        fun isValid(path: String?): Boolean = path in validBlocks
    }

    private const val UNDERGROUND_DECORATION_STEP = 7
    private const val PATCH_ATTEMPTS = 96
    private val ALL_BIOMES =
        setOf("nether_wastes", "soul_sand_valley", "crimson_forest", "warped_forest", "basalt_deltas")
    private val DELTA_BIOME = setOf("basalt_deltas")
    private val SOUL_BIOME = setOf("soul_sand_valley")
    private val NON_DELTA_BIOMES = ALL_BIOMES - "basalt_deltas"
    private val SOUL_FIRE_BIOMES = ALL_BIOMES - "crimson_forest"
    private val NETHER_MUSHROOM_BIOMES = setOf("nether_wastes", "basalt_deltas")
    private val GLOWSTONE_SUPPORT = setOf("netherrack", "basalt", "blackstone")
    private val SPRING_LAVA_SUPPORT = setOf("netherrack", "soul_sand", "gravel", "magma_block", "blackstone")
    private val CRIMSON_ROOT_SUPPORT =
        setOf(
            "dirt",
            "coarse_dirt",
            "rooted_dirt",
            "mud",
            "muddy_mangrove_roots",
            "moss_block",
            "pale_moss_block",
            "grass_block",
            "mycelium",
            "podzol",
            "farmland",
            "crimson_nylium",
            "warped_nylium",
            "soul_soil",
        )
    private val MUSHROOM_LIGHT_OVERRIDE = setOf("mycelium", "podzol", "crimson_nylium", "warped_nylium")
    private val MUSHROOM_SOLID_SUPPORT =
        setOf(
            "netherrack",
            "basalt",
            "blackstone",
            "bedrock",
            "soul_sand",
            "soul_soil",
            "gravel",
            "magma_block",
            "nether_gold_ore",
            "nether_quartz_ore",
            "ancient_debris",
        )
    private val SPRING_NEIGHBOURS =
        listOf(
            Triple(-1, 0, 0),
            Triple(1, 0, 0),
            Triple(0, 0, -1),
            Triple(0, 0, 1),
            Triple(0, -1, 0),
        )
    private val ALL_NEIGHBOURS =
        listOf(
            Triple(0, -1, 0),
            Triple(0, 1, 0),
            Triple(0, 0, -1),
            Triple(0, 0, 1),
            Triple(-1, 0, 0),
            Triple(1, 0, 0),
        )
}
