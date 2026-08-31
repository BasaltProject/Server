package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.densityfunction.DensityCache
import org.basaltmc.server.world.structure.StructureTemplate
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtInt
import org.basaltmc.wire.nbt.NbtList
import org.basaltmc.wire.nbt.NbtType
import kotlin.test.Test

/** The frozen Kotlin as the oracle for the underground structures step, fossils included. */
class UndergroundStructuresOracle {
    private inner class FakeEnvironment(
        private val biome: String,
    ) : SurfaceRuleEnvironment {
        override fun surfaceDepthNoise(
            x: Int,
            z: Int,
        ): Int = 0

        override fun surfaceSecondaryNoise(
            x: Int,
            z: Int,
        ): Double = 0.0

        override fun sample2d(
            noiseId: String,
            x: Int,
            z: Int,
        ): Double = 0.0

        override fun sample3d(
            noiseId: String,
            x: Int,
            y: Int,
            z: Int,
        ): Double = 0.0

        override fun verticalGradientRandom(
            name: String,
            x: Int,
            y: Int,
            z: Int,
        ): Float = 0.5f

        override fun positionalRandom(
            x: Int,
            y: Int,
            z: Int,
        ): BasaltRandom = XoroshiroRandom(x * 31L + z)

        override fun estimatedSurfaceHeight(
            x: Int,
            z: Int,
        ): Int = 70

        override fun biomeAt(
            x: Int,
            y: Int,
            z: Int,
        ): String = biome

        override fun noiseBiomeAt(
            quartX: Int,
            quartY: Int,
            quartZ: Int,
            cache: DensityCache?,
        ): String = biome

        override fun isSteep(
            x: Int,
            z: Int,
        ): Boolean = false

        override fun clayBand(
            x: Int,
            y: Int,
            z: Int,
        ): Int = 0
    }

    private fun stateId(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")

    /** A stand-in fossil template: bone blocks in a line, and coal ore in the overlay. */
    private fun fossilTemplate(overlay: Boolean): StructureTemplate {
        val palette =
            NbtList(
                NbtType.COMPOUND,
                listOf(
                    NbtCompound().also {
                        it.putString(
                            "Name",
                            if (overlay) "minecraft:coal_ore" else "minecraft:bone_block",
                        )
                    },
                ),
            )
        val blocks = mutableListOf<NbtCompound>()
        for (x in 0 until 5) {
            for (y in 0 until 2) {
                for (z in 0 until 3) {
                    val entry = NbtCompound()
                    entry["pos"] = NbtList(NbtType.INT, listOf(NbtInt(x), NbtInt(y), NbtInt(z)))
                    entry.putInt("state", 0)
                    blocks += entry
                }
            }
        }
        val nbt = NbtCompound()
        nbt["size"] = NbtList(NbtType.INT, listOf(NbtInt(5), NbtInt(2), NbtInt(3)))
        nbt["palette"] = palette
        nbt["blocks"] = NbtList(NbtType.COMPOUND, blocks)
        val template = StructureTemplate()
        template.load(nbt)
        return template
    }

    private val templates: TemplateLookup = { name -> fossilTemplate(name.endsWith("_coal")) }

    private fun stoneChunk(
        chunkX: Int,
        chunkZ: Int,
    ): Chunk {
        val chunk = Chunk(chunkX, chunkZ)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in -64..60) chunk.setBlockStateId(x, y, z, Blocks.STONE.stateId)
                // Narrow corridors rather than an open cavern: a monster room needs a solid floor
                // and ceiling and between one and five openings in its walls, and a hall the width
                // of the chunk gives it far more than five.
                for (y in -56..56) {
                    val slot = y.mod(8) < 2
                    if (slot && (x.mod(8) == 0 || z.mod(8) == 4)) {
                        chunk.setBlockStateId(x, y, z, Blocks.AIR.stateId)
                    }
                }
            }
        }
        return chunk
    }

    private val counted =
        setOf("bone_block", "coal_ore", "deepslate_diamond_ore", "spawner", "mossy_cobblestone", "chest")

    private fun fold(chunk: Chunk): Pair<Long, Int> {
        var checksum = -3750763034362895579L
        var placed = 0
        for (y in -64..68) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    val id = chunk.getBlockStateId(x, y, z)
                    checksum = (checksum xor id.toLong()) * 1099511628211L
                    if (Blocks.byStateId(id)?.block?.path in counted) placed++
                }
            }
        }
        return checksum to placed
    }

    private fun report(
        name: String,
        biome: String,
        withTemplates: Boolean,
        count: Int,
    ) {
        var checksum = -3750763034362895579L
        var placed = 0
        var deferred = 0
        for (chunkIndex in 0 until count) {
            val chunk = stoneChunk(chunkIndex * 3 - 8, chunkIndex * -2 + 5)
            OverworldUndergroundStructures.decorate(
                chunk,
                101L + chunkIndex,
                FakeEnvironment(biome),
                if (withTemplates) templates else null,
            )
            val (chunkChecksum, chunkPlaced) = fold(chunk)
            checksum = (checksum xor chunkChecksum) * 1099511628211L
            placed += chunkPlaced
            deferred += chunk.takeGeneratedBlockWrites().size
        }
        println("ORACLE $name placed=$placed deferred=$deferred checksum=$checksum")
    }

    @Test
    fun dump() {
        report("desert", "minecraft:desert", withTemplates = true, count = 48)
        // Without a loader the fossil draws never happen, which is a different world.
        report("noTemplates", "minecraft:desert", withTemplates = false, count = 48)
        // The rooms share the step but not the biomes, so a non-fossil overworld biome keeps them.
        report("plains", "minecraft:plains", withTemplates = true, count = 48)
        // The Nether is in neither set, so nothing at all is placed.
        report("nether", "minecraft:nether_wastes", withTemplates = true, count = 48)
    }
}
