package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment
import org.basaltmc.wire.common.Identifier

/** // parity: `forest_rock`, global index 4 of Overworld `LOCAL_MODIFICATIONS`. */
public object ForestRockDecoration {
    public fun decorate(
        chunk: Chunk,
        seed: Long,
        environment: SurfaceRuleEnvironment,
    ) {
        val random = WorldgenRandom(XoroshiroRandom(seed))
        val decorationSeed = random.setDecorationSeed(seed, chunk.chunkX * 16, chunk.chunkZ * 16)
        random.setFeatureSeed(decorationSeed, GLOBAL_INDEX, LOCAL_MODIFICATIONS_STEP)
        repeat(2) {
            val localX = random.nextInt(16)
            val localZ = random.nextInt(16)
            val y = chunk.heightAt(localX, localZ, Chunk.HeightmapType.WORLD_SURFACE)
            val worldX = chunk.chunkX * 16 + localX
            val worldZ = chunk.chunkZ * 16 + localZ
            if (environment.biomeAt(worldX, y, worldZ).substringAfter(':') !in OLD_GROWTH_TAIGA_BIOMES) return@repeat
            placeAt(chunk, random, localX, y, localZ)
        }
    }

    /**
     * The feature itself, at one position: what the decoration pass calls per origin and what
     * `/place feature` calls once. Chunk-local x and z, as everything in this package uses.
     */
    public fun placeAt(
        chunk: Chunk,
        random: WorldgenRandom,
        suppliedX: Int,
        suppliedY: Int,
        suppliedZ: Int,
    ) {
        var originX = suppliedX
        var originY = suppliedY
        var originZ = suppliedZ
        while (originY > WorldHeight.MIN_Y + 3 && !canPlaceOn(get(chunk, originX, originY - 1, originZ))) {
            originY--
        }
        if (originY <= WorldHeight.MIN_Y + 3) return

        repeat(3) {
            val radiusX = random.nextInt(2)
            val radiusY = random.nextInt(2)
            val radiusZ = random.nextInt(2)
            val radius = (radiusX + radiusY + radiusZ) * 0.333f + 0.5f
            val radiusSquared = radius * radius
            for (z in originZ - radiusZ..originZ + radiusZ) {
                for (y in originY - radiusY..originY + radiusY) {
                    for (x in originX - radiusX..originX + radiusX) {
                        val dx = x - originX
                        val dy = y - originY
                        val dz = z - originZ
                        if (dx * dx + dy * dy + dz * dz <= radiusSquared) {
                            set(chunk, x, y, z, mossyCobblestoneStateId)
                        }
                    }
                }
            }
            originX += -1 + random.nextInt(2)
            originY -= random.nextInt(2)
            originZ += -1 + random.nextInt(2)
        }
    }

    private fun get(
        chunk: Chunk,
        x: Int,
        y: Int,
        z: Int,
    ): Int =
        if (x in 0 until 16 && z in 0 until 16 && y in WorldHeight.MIN_Y..WorldHeight.MAX_Y) {
            chunk.getBlockStateId(x, y, z)
        } else {
            Blocks.AIR.stateId
        }

    private fun set(
        chunk: Chunk,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
    ) {
        if (x in 0 until 16 && z in 0 until 16 && y in WorldHeight.MIN_Y..WorldHeight.MAX_Y) {
            chunk.setBlockStateId(x, y, z, stateId)
        }
    }

    private fun canPlaceOn(stateId: Int): Boolean = Blocks.byStateId(stateId)?.block?.path in SUPPORT_BLOCKS

    private const val LOCAL_MODIFICATIONS_STEP = 2
    private const val GLOBAL_INDEX = 4
    private val OLD_GROWTH_TAIGA_BIOMES = setOf("old_growth_pine_taiga", "old_growth_spruce_taiga")
    private val SUPPORT_BLOCKS =
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
            "stone",
            "granite",
            "diorite",
            "andesite",
            "tuff",
            "deepslate",
        )
    private val mossyCobblestoneStateId =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla("mossy_cobblestone"))?.stateId
            ?: error("missing vanilla block minecraft:mossy_cobblestone")
}
