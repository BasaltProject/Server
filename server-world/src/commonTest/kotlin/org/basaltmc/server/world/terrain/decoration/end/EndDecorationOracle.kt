package org.basaltmc.server.world.terrain.decoration.end

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.densityfunction.BlendedNoise
import org.basaltmc.server.world.densityfunction.DensityFunction
import org.basaltmc.server.world.terrain.EndNoiseRouter
import org.basaltmc.server.world.terrain.NoiseRouterData
import org.basaltmc.server.world.terrain.TheEndBiomeSource
import org.basaltmc.wire.common.Identifier
import kotlin.test.Test

/** The frozen Kotlin as the oracle for the three End decoration steps. */
class EndDecorationOracle {
    private fun base3d(): DensityFunction = BlendedNoise.createUnseeded(0.25, 0.125, 80.0, 160.0, 8.0)

    private fun router(): EndNoiseRouter = NoiseRouterData.end(base3d(), 0x5DEECE66DL)

    private fun stateId(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")

    /** An end stone shelf with open air above it, which is where a chorus plant takes root. */
    private fun endChunk(
        chunkX: Int,
        chunkZ: Int,
    ): Chunk {
        val chunk = Chunk(chunkX, chunkZ)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in 40..(56 + (x + z) % 3)) chunk.setBlockStateId(x, y, z, stateId("end_stone"))
            }
        }
        return chunk
    }

    private fun fold(chunk: Chunk): Triple<Long, Int, Int> {
        var checksum = -3750763034362895579L
        var chorus = 0
        var endStone = 0
        for (y in 30..100) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    val id = chunk.getBlockStateId(x, y, z)
                    checksum = (checksum xor id.toLong()) * 1099511628211L
                    when (Blocks.byStateId(id)?.block?.path) {
                        "chorus_plant", "chorus_flower" -> chorus++
                        "end_stone" -> endStone++
                    }
                }
            }
        }
        return Triple(checksum, chorus, endStone)
    }

    @Test
    fun dump() {
        val biomeSource = TheEndBiomeSource(router().erosion)

        // The raw generation step: small end islands only, and the whole chunk is far out.
        var checksum = -3750763034362895579L
        var endStone = 0
        var deferred = 0
        for (index in 0 until 40) {
            val chunk = Chunk(index * 37 + 400, index * -53 - 300)
            EndRawDecoration.decorate(chunk, 909L + index, biomeSource)
            val (chunkChecksum, _, chunkStone) = fold(chunk)
            checksum = (checksum xor chunkChecksum) * 1099511628211L
            endStone += chunkStone
            deferred += chunk.takeGeneratedBlockWrites().size
        }
        println("ORACLE rawStep endStone=$endStone deferred=$deferred checksum=$checksum")

        checksum = -3750763034362895579L
        endStone = 0
        var trailing = 0
        for (index in 0 until 12) {
            val chunk = Chunk(index, -index)
            val random = WorldgenRandom(LegacyRandom(77L + index * 911L))
            EndRawDecoration.placeAt(chunk, random, index * 16 + 8, 60, -index * 16 + 8)
            val (chunkChecksum, _, chunkStone) = fold(chunk)
            checksum = (checksum xor chunkChecksum) * 1099511628211L
            endStone += chunkStone
            trailing = random.nextInt()
        }
        println("ORACLE islandAt endStone=$endStone checksum=$checksum after=$trailing")

        // The top layer platform, which only ever touches the two chunks around x=100, z=0.
        checksum = -3750763034362895579L
        var obsidian = 0
        for (chunkX in 5..7) {
            for (chunkZ in -1..1) {
                val chunk = Chunk(chunkX, chunkZ)
                for (x in 0 until 16) {
                    for (z in 0 until 16) {
                        for (y in 40..55) chunk.setBlockStateId(x, y, z, stateId("end_stone"))
                    }
                }
                EndTopLayerDecoration.decorate(chunk, biomeSource)
                for (y in 30..60) {
                    for (z in 0 until 16) {
                        for (x in 0 until 16) {
                            val id = chunk.getBlockStateId(x, y, z)
                            checksum = (checksum xor id.toLong()) * 1099511628211L
                            if (Blocks.byStateId(id)?.block?.path == "obsidian") obsidian++
                        }
                    }
                }
            }
        }
        println("ORACLE platform obsidian=$obsidian checksum=$checksum")

        // The chorus plants, on a shelf inside the highlands.
        checksum = -3750763034362895579L
        var chorus = 0
        deferred = 0
        for (index in 0 until 24) {
            val chunk = endChunk(index * 37 + 400, index * -53 - 300)
            EndVegetalDecoration.decorate(chunk, 4242L + index, router(), biomeSource)
            val (chunkChecksum, chunkChorus, _) = fold(chunk)
            checksum = (checksum xor chunkChecksum) * 1099511628211L
            chorus += chunkChorus
            deferred += chunk.takeGeneratedBlockWrites().size
        }
        println("ORACLE chorusStep chorus=$chorus deferred=$deferred checksum=$checksum")

        checksum = -3750763034362895579L
        chorus = 0
        val placed = StringBuilder()
        trailing = 0
        for (index in 0 until 12) {
            val chunk = endChunk(index * 37 + 400, index * -53 - 300)
            val random = WorldgenRandom(LegacyRandom(1234L + index * 7717L))
            val x = chunk.chunkX * 16 + 8
            val z = chunk.chunkZ * 16 + 8
            val y = chunk.heightAt(8, 8, Chunk.HeightmapType.MOTION_BLOCKING)
            placed.append(if (EndVegetalDecoration.placeAt(chunk, random, router(), x, y, z)) '1' else '0')
            val (chunkChecksum, chunkChorus, _) = fold(chunk)
            checksum = (checksum xor chunkChecksum) * 1099511628211L
            chorus += chunkChorus
            trailing = random.nextInt()
        }
        println("ORACLE chorusAt placed=$placed chorus=$chorus checksum=$checksum after=$trailing")

        // The spikes ring the origin, so only the chunks around it see anything.
        checksum = -3750763034362895579L
        var obsidianTotal = 0
        var entities = 0
        val hit = StringBuilder()
        for (chunkX in -4..4) {
            for (chunkZ in -4..4) {
                val chunk = Chunk(chunkX, chunkZ, org.basaltmc.server.world.ChunkHeight.END)
                hit.append(if (EndSurfaceStructuresDecoration.placeSpikesIn(chunk, 987654321L)) '1' else '0')
                for (y in 0..120) {
                    for (z in 0 until 16) {
                        for (x in 0 until 16) {
                            val id = chunk.getBlockStateId(x, y, z)
                            checksum = (checksum xor id.toLong()) * 1099511628211L
                            if (Blocks.byStateId(id)?.block?.path == "obsidian") obsidianTotal++
                        }
                    }
                }
                entities += chunk.generatedEntities().size
            }
        }
        println("ORACLE spikes hit=$hit obsidian=$obsidianTotal entities=$entities checksum=$checksum")

        // The full surface-structures step, which also rolls the return gateways.
        checksum = -3750763034362895579L
        obsidianTotal = 0
        entities = 0
        for (chunkX in -3..3) {
            for (chunkZ in -3..3) {
                val chunk = Chunk(chunkX, chunkZ, org.basaltmc.server.world.ChunkHeight.END)
                EndSurfaceStructuresDecoration.decorate(chunk, 987654321L, router(), biomeSource)
                for (y in 0..120) {
                    for (z in 0 until 16) {
                        for (x in 0 until 16) {
                            val id = chunk.getBlockStateId(x, y, z)
                            checksum = (checksum xor id.toLong()) * 1099511628211L
                            if (Blocks.byStateId(id)?.block?.path == "obsidian") obsidianTotal++
                        }
                    }
                }
                entities += chunk.generatedEntities().size
            }
        }
        println("ORACLE surfaceStep obsidian=$obsidianTotal entities=$entities checksum=$checksum")
    }
}
