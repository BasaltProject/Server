package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.wire.common.Identifier
import kotlin.test.Test

/** The frozen Kotlin as the oracle for the ground vegetation patches. */
class GroundVegetationOracle {
    private fun groundChunk(): Chunk {
        val chunk = Chunk(2, -3)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in -64 until 70 + ((x * 5 + z * 3) % 4)) {
                    chunk.setBlockStateId(x, y, z, Blocks.STONE.stateId)
                }
                val top = 69 + ((x * 5 + z * 3) % 4)
                chunk.setBlockStateId(x, top, z, if ((x + z) % 3 == 0) stateId("sand") else Blocks.GRASS_BLOCK.stateId)
            }
        }
        return chunk
    }

    private fun report(name: String, body: (Chunk, WorldgenRandom) -> Unit) {
        val chunk = groundChunk()
        val random = WorldgenRandom(LegacyRandom(31337L))
        body(chunk, random)
        var checksum = -3750763034362895579L
        var plants = 0
        for (y in 66..78) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    val stateId = chunk.getBlockStateId(x, y, z)
                    checksum = (checksum xor stateId.toLong()) * 1099511628211L
                    if (!Blocks.isAir(stateId) && stateId != Blocks.STONE.stateId &&
                        stateId != Blocks.GRASS_BLOCK.stateId && stateId != stateId("sand")
                    ) {
                        plants++
                    }
                }
            }
        }
        println("ORACLE $name plants=$plants checksum=$checksum after=" + random.nextInt())
    }

    @Test
    fun dump() {
        report("bush") { c, r -> ForestGroundDecoration.BUSH.decorate(c, r) }
        report("forestGrass") { c, r -> ForestGroundDecoration.GRASS.decorate(c, r) }
        report("savannaTall") { c, r -> SavannaGroundDecoration.TALL_GRASS.decorate(c, r) }
        report("savanna") { c, r -> SavannaGroundDecoration.SAVANNA.decorate(c, r) }
        report("windswept") { c, r -> SavannaGroundDecoration.WINDSWEPT.decorate(c, r) }
        report("dryDesert") { c, r -> DryGrassDecoration.DESERT.decorate(c, r) }
        report("dryBadlands") { c, r -> DryGrassDecoration.BADLANDS.decorate(c, r) }
        report("badlands") { c, r -> BadlandsDecoration.decorate(c, r) }
        report("desert") { c, r -> DesertDecoration.decorate(c, r) }
        report("desertAt") { c, r -> DesertDecoration.placeAt(c, r, DecorationPos(40, 70, -40)) }
        report("dryAt") { c, r -> DryGrassDecoration.placeAt(c, r, DecorationPos(40, 72, -40)) }
        report("flowerAt") { c, r -> DefaultVegetationDecoration.placeDefaultPatchAt(c, r, DecorationPos(40, 72, -40)) }
        report("combined") { c, r ->
            combine(ForestGroundDecoration.BUSH, ForestGroundDecoration.GRASS, DryGrassDecoration.DESERT).decorate(c, r)
        }
        println("ORACLE known=" + (BiomeDecoration.forBiome(Identifier.vanilla("plains")) != null) +
            "," + (BiomeDecoration.forBiome(Identifier.vanilla("nether_wastes")) != null))
    }
}
