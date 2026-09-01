package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.server.random.WorldgenRandom
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

/** The frozen Kotlin as the oracle for the LAKES step and the template-backed sulfur spring. */
class LakeAndSulfurSpringOracle {
    private class FakeEnvironment(
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
        ): Int = 60

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

    /** A stand-in spring template: a sulfur cube, big enough that a rotation moves its blocks. */
    private fun springTemplate(size: Int): StructureTemplate {
        val palette =
            NbtList(
                NbtType.COMPOUND,
                listOf(NbtCompound().also { it.putString("Name", "minecraft:sulfur") }),
            )
        val blocks = mutableListOf<NbtCompound>()
        for (x in 0 until size) {
            for (y in 0 until 2) {
                for (z in 0 until size + 1) {
                    val entry = NbtCompound()
                    entry["pos"] = NbtList(NbtType.INT, listOf(NbtInt(x), NbtInt(y), NbtInt(z)))
                    entry.putInt("state", 0)
                    blocks += entry
                }
            }
        }
        val nbt = NbtCompound()
        nbt["size"] = NbtList(NbtType.INT, listOf(NbtInt(size), NbtInt(2), NbtInt(size + 1)))
        nbt["palette"] = palette
        nbt["blocks"] = NbtList(NbtType.COMPOUND, blocks)
        return StructureTemplate().also { it.load(nbt) }
    }

    /** The four sizes differ, so which variant the weighted draw picked is visible in the world. */
    private val templates: (Identifier) -> StructureTemplate? = { name ->
        when {
            name.path.contains("extra_large") -> springTemplate(5)
            name.path.contains("large") -> springTemplate(4)
            name.path.contains("medium") -> springTemplate(3)
            else -> springTemplate(2)
        }
    }

    /**
     * Stone to 60 with a sulfur cap, so `sulfur_pool`'s final predicate can pass, and a flat cavern
     * at 20..27 wide enough for the spring's four level probes eight blocks out.
     */
    private fun terrainChunk(
        chunkX: Int,
        chunkZ: Int,
        cavern: Boolean,
        floor: Int,
    ): Chunk {
        val chunk = Chunk(chunkX, chunkZ)
        val stone = Blocks.STONE.stateId
        val sulfur = stateId("sulfur")
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in floor..60) chunk.setBlockStateId(x, y, z, stone)
                chunk.setBlockStateId(x, 60, z, sulfur)
                // A flat cavern the spring can stand in, wide enough for its four level probes.
                if (cavern) for (y in 20..27) chunk.setBlockStateId(x, y, z, Blocks.AIR.stateId)
            }
        }
        // Only the origin block itself, so the root column climbs through stone to the cavern
        // instead of finding a hollow floor one step up and giving up.
        if (cavern) chunk.setBlockStateId(7, 5, 9, Blocks.AIR.stateId)
        return chunk
    }

    /** The undecorated neighbour terrain, so a body that crosses the chunk edge is not refused. */
    private fun neighbourState(
        y: Int,
        cavern: Boolean,
        floor: Int,
    ): Int =
        when {
            y > 60 || y < floor -> Blocks.AIR.stateId
            y == 60 -> stateId("sulfur")
            cavern && y in 20..27 -> Blocks.AIR.stateId
            else -> Blocks.STONE.stateId
        }

    /** The terrain's floor. A shallower world than vanilla's keeps the fold from dwarfing the run. */
    private val floorY = -8

    private val counted = setOf("lava", "sulfur", "sulfur_spike", "potent_sulfur", "tuff", "water")

    private fun fold(
        chunk: Chunk,
        foldFrom: Int,
        foldTo: Int,
    ): Pair<Long, Int> {
        var checksum = -3750763034362895579L
        var placed = 0
        for (y in foldFrom..foldTo) {
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

    private fun reportLakes(
        name: String,
        biome: String,
        count: Int,
        feature: String? = null,
        /**
         * A rare feature needs many chunks, so only the band it can reach is built and folded:
         * filling a whole column 900 times over costs more than the feature itself.
         */
        foldFrom: Int = floorY,
        foldTo: Int = 64,
    ) {
        var checksum = -3750763034362895579L
        var placed = 0
        var deferred = 0
        for (chunkIndex in 0 until count) {
            val chunk = terrainChunk(chunkIndex * 3 - 8, chunkIndex * -2 + 5, cavern = false, floor = foldFrom)
            val environment = FakeEnvironment(biome)
            val seed = 77L + chunkIndex
            chunk.withExternalDecorationBlockState({ _, y, _ -> neighbourState(y, cavern = false, floor = foldFrom) }) {
                if (feature == null) {
                    OverworldLakeDecoration.decorate(chunk, seed, environment)
                } else {
                    OverworldLakeDecoration.decorateFeature(chunk, seed, environment, feature)
                }
            }
            val (chunkChecksum, chunkPlaced) = fold(chunk, foldFrom, foldTo)
            checksum = (checksum xor chunkChecksum) * 1099511628211L
            placed += chunkPlaced
            deferred += chunk.takeGeneratedBlockWrites().size
        }
        println("ORACLE $name placed=$placed deferred=$deferred checksum=$checksum")
    }

    private fun reportSpring(
        name: String,
        withTemplates: Boolean,
        count: Int,
    ) {
        var checksum = -3750763034362895579L
        var placed = 0
        var deferred = 0
        var draws = 0
        val placement = PlacedFeatures.contextual(Identifier.vanilla("rooted_sulfur_spring")) ?: error("missing")
        for (chunkIndex in 0 until count) {
            val chunk = terrainChunk(chunkIndex * 3 - 8, chunkIndex * -2 + 5, cavern = true, floor = floorY)
            val random = WorldgenRandom(XoroshiroRandom(500L + chunkIndex))
            val context =
                PlacedFeatures.FeatureContext(
                    templates = if (withTemplates) templates else null,
                    blockStateAt = { x, y, z ->
                        if ((x shr 4) == chunk.chunkX && (z shr 4) == chunk.chunkZ && y in -64..319) {
                            chunk.getBlockStateId(x and 15, y, z and 15)
                        } else {
                            neighbourState(y, cavern = true, floor = floorY)
                        }
                    },
                )
            val ran =
                chunk.withExternalDecorationBlockState(
                    { _, y, _ -> neighbourState(y, cavern = true, floor = floorY) },
                ) {
                    placement.place(chunk, random, 7, 5, 9, context)
                }
            if (ran) placed++
            val (chunkChecksum, chunkPlaced) = fold(chunk, floorY, 64)
            checksum = (checksum xor chunkChecksum) * 1099511628211L
            placed += chunkPlaced
            deferred += chunk.takeGeneratedBlockWrites().size
            // A trailing draw pins how much of the random each case consumed.
            draws = draws * 31 + random.nextInt(1 shl 20)
        }
        println("ORACLE $name placed=$placed deferred=$deferred draws=$draws checksum=$checksum")
    }

    @Test
    fun dump() {
        // One report per node as well as the whole step: a lava lake is rare enough that a shared
        // run would leave its branch untested.
        reportLakes("lakeUnderground", "minecraft:plains", count = 150, feature = "lake_lava_underground")
        reportLakes("lakeSurface", "minecraft:plains", count = 900, feature = "lake_lava_surface", foldFrom = 50)
        reportLakes("sulfurPool", "minecraft:sulfur_caves", count = 30, feature = "sulfur_pool")
        // The spring's own node runs its placement and then refuses: it needs the template loader,
        // which this door does not carry.
        reportLakes("springNode", "minecraft:sulfur_caves", count = 30, feature = "rooted_sulfur_spring")
        reportLakes("lakesPlains", "minecraft:plains", count = 60)
        reportLakes("lakesSulfur", "minecraft:sulfur_caves", count = 60)
        // deep_dark is the one Overworld biome that lists no lake at all.
        reportLakes("lakesDeepDark", "minecraft:deep_dark", count = 60)
        reportSpring("spring", withTemplates = true, count = 24)
        // Without a loader the feature refuses before drawing anything.
        reportSpring("springNoTemplates", withTemplates = false, count = 24)
    }
}
