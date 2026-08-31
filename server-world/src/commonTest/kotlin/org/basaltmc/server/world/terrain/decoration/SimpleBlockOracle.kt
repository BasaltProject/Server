package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import kotlin.test.Test

/** The frozen Kotlin as the oracle for the single-block vegetation feature. */
class SimpleBlockOracle {
    private fun groundChunk(): Chunk {
        val chunk = Chunk(0, 0)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in -64 until 63) chunk.setBlockStateId(x, y, z, Blocks.STONE.stateId)
                // A patchwork of supports so every branch of canSurvive is reached.
                val ground = when ((x + z) % 6) {
                    0 -> Blocks.GRASS_BLOCK.stateId
                    1 -> stateId("sand")
                    2 -> stateId("podzol")
                    3 -> Blocks.STONE.stateId
                    4 -> Blocks.WATER.stateId
                    else -> stateId("mycelium")
                }
                chunk.setBlockStateId(x, 63, z, ground)
            }
        }
        return chunk
    }

    private fun run(name: String, feature: SimpleBlockFeature) {
        val chunk = groundChunk()
        val random = WorldgenRandom(LegacyRandom(555L))
        var placed = 0
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                if (feature.place(chunk, random, DecorationPos(x, 64, z))) placed++
            }
        }
        var checksum = -3750763034362895579L
        for (y in 62..66) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    checksum = (checksum xor chunk.getBlockStateId(x, y, z).toLong()) * 1099511628211L
                }
            }
        }
        println("ORACLE $name placed=$placed checksum=$checksum after=" + random.nextInt())
    }

    @Test
    fun dump() {
        run("poppy", SimpleBlockFeature({ r, _ -> if (r.nextInt(3) < 2) stateId("poppy") else stateId("dandelion") }))
        run("deadBush", SimpleBlockFeature({ _, _ -> stateId("dead_bush") }))
        run("lilyPad", SimpleBlockFeature({ _, _ -> stateId("lily_pad") }))
        run("mushroom", SimpleBlockFeature({ _, _ -> stateId("red_mushroom") }))
        run("leafLitter", SimpleBlockFeature({ _, _ -> stateId("leaf_litter") }))
        run("tallGrass", SimpleBlockFeature({ _, _ -> stateId("tall_grass") }, isDoublePlant = true))
        run("paleMoss", SimpleBlockFeature({ _, _ -> stateId("pale_moss_carpet") }))
        run("tickedGrass", SimpleBlockFeature({ _, _ -> stateId("short_grass") }, scheduleTick = true))

        // The carpet only grows sides, and only then draws for its topper, where there is a wall to
        // attach to. Without one every side short-circuits to "none" and the topper path never runs.
        val walled = groundChunk()
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                if (x % 4 == 0 || z % 5 == 0) {
                    for (y in 64..67) walled.setBlockStateId(x, y, z, Blocks.STONE.stateId)
                }
            }
        }
        val random = WorldgenRandom(LegacyRandom(555L))
        val carpet = SimpleBlockFeature({ _, _ -> stateId("pale_moss_carpet") })
        var placed = 0
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                if (carpet.place(walled, random, DecorationPos(x, 64, z))) placed++
            }
        }
        var checksum = -3750763034362895579L
        for (y in 62..68) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    checksum = (checksum xor walled.getBlockStateId(x, y, z).toLong()) * 1099511628211L
                }
            }
        }
        println("ORACLE walledMoss placed=$placed checksum=$checksum after=" + random.nextInt())
    }
}
