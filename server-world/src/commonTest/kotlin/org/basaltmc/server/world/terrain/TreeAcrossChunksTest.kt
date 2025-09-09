package org.basaltmc.server.world.terrain

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.terrain.decoration.PlacedFeatures
import org.basaltmc.wire.common.Identifier
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

/**
 * A tree at a chunk edge must be the same tree as one in the middle, delivered partly to a neighbour.
 * Trunk, foliage and roots must match exactly; decorators cannot. // parity: `TreeDecorator.Context`
 * walks in `BlockPos.hashCode()` order, so vanilla grows different vines for the same tree moved one
 * block. All that must hold for them is that nothing is dropped for landing outside the chunk.
 */
class TreeAcrossChunksTest {
    private fun state(path: String) =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")

    private val grass = state("grass_block")
    private val dirt = state("dirt")

    /** Grass over dirt everywhere, so nothing but the trunk position can vary. */
    private fun groundChunk(
        chunkX: Int,
        chunkZ: Int,
    ): Chunk =
        Chunk(chunkX, chunkZ).apply {
            for (x in 0 until 16) {
                for (z in 0 until 16) {
                    for (y in SURFACE_Y - 8 until SURFACE_Y) setBlockStateId(x, y, z, dirt)
                    setBlockStateId(x, SURFACE_Y, z, grass)
                }
            }
        }

    private class Grown(
        val skeleton: Map<Triple<Int, Int, Int>, Int>,
        val decorations: Map<Triple<Int, Int, Int>, Int>,
        val blockEntities: Set<Triple<Int, Int, Int>>,
        val external: Int,
    )

    private fun isLog(stateId: Int): Boolean {
        val path = Blocks.byStateId(stateId)?.block?.path ?: return false
        return path.endsWith("_log") || path.endsWith("_wood")
    }

    private fun isLeaves(stateId: Int): Boolean =
        Blocks
            .byStateId(stateId)
            ?.block
            ?.path
            ?.endsWith("_leaves") == true

    /**
     * A leaf compared without its `distance`, which is not translation invariant either. // parity:
     * `TreeFeature.updateLeaves` walks `HashSet` buckets, so the jar re-writes a leaf it already settled.
     */
    private fun comparable(stateId: Int): Int =
        if (!isLeaves(stateId)) {
            stateId
        } else {
            val block = Blocks.byStateId(stateId)?.block ?: return stateId
            val properties = Blocks.byStateId(stateId)?.properties.orEmpty() - "distance"
            Blocks.stateIdOf(block, properties) ?: stateId
        }

    private fun isSkeleton(stateId: Int): Boolean {
        val path = Blocks.byStateId(stateId)?.block?.path ?: return false
        return isLog(stateId) ||
            isLeaves(stateId) ||
            path == "mangrove_roots" ||
            path == "muddy_mangrove_roots"
    }

    /** Everything [feature] writes from [origin], as offsets from it, local and deferred together. */
    private fun grow(
        feature: String,
        origin: Triple<Int, Int, Int>,
    ): Grown {
        val (originX, originY, originZ) = origin
        val chunk = groundChunk(originX shr 4, originZ shr 4)
        val cache = NeighbourTerrainCache(generateTerrain = ::groundChunk)
        val view = cache.around(chunk.chunkX, chunk.chunkZ)
        val placement = PlacedFeatures[Identifier.vanilla(feature)] ?: error("unknown feature '$feature'")

        chunk.withExternalDecorationBlockState(reader = { x, y, z -> view.blockStateIdAt(x, y, z) }) {
            placement.place(chunk, WorldgenRandom(XoroshiroRandom(SEED)), originX, originY, originZ)
        }

        val skeleton = HashMap<Triple<Int, Int, Int>, Int>()
        val decorations = HashMap<Triple<Int, Int, Int>, Int>()

        fun record(
            worldX: Int,
            y: Int,
            worldZ: Int,
            stateId: Int,
        ) {
            val offset = Triple(worldX - originX, y - originY, worldZ - originZ)
            if (isSkeleton(stateId)) skeleton[offset] = comparable(stateId) else decorations[offset] = stateId
        }

        val untouched = groundChunk(chunk.chunkX, chunk.chunkZ)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in SURFACE_Y - 8..SURFACE_Y + 40) {
                    val stateId = chunk.getBlockStateId(x, y, z)
                    if (stateId == untouched.getBlockStateId(x, y, z)) continue
                    record((chunk.chunkX shl 4) + x, y, (chunk.chunkZ shl 4) + z, stateId)
                }
            }
        }
        var external = 0
        for (write in chunk.takeGeneratedBlockWrites()) {
            record(write.x, write.y, write.z, write.stateId)
            external++
        }
        val blockEntities = HashSet<Triple<Int, Int, Int>>()
        // A nest inside the chunk is installed directly, one outside is deferred; both count.
        for (entity in chunk.blockEntities()) {
            val worldX = (chunk.chunkX shl 4) + entity.localX
            val worldZ = (chunk.chunkZ shl 4) + entity.localZ
            blockEntities += Triple(worldX - originX, entity.y - originY, worldZ - originZ)
        }
        for (entity in chunk.takeGeneratedBlockEntityWrites()) {
            blockEntities += Triple(entity.x - originX, entity.y - originY, entity.z - originZ)
        }
        return Grown(skeleton, decorations, blockEntities, external)
    }

    /** Only what differs, so a failure names the blocks instead of dumping both trees. */
    private fun differences(
        centred: Map<Triple<Int, Int, Int>, Int>,
        moved: Map<Triple<Int, Int, Int>, Int>,
    ): String {
        val report = mutableListOf<String>()
        for (offset in (centred.keys + moved.keys).sortedBy { it.toString() }) {
            if (centred[offset] != moved[offset]) {
                report += "$offset centred=${centred[offset]} moved=${moved[offset]}"
            }
        }
        return report.take(12).joinToString("; ")
    }

    @Test
    fun `every tree family keeps its trunk and foliage at a chunk edge`() {
        for (feature in FAMILIES) {
            val centred = grow(feature, Triple(8, SURFACE_Y + 1, 8))
            assertTrue(centred.skeleton.isNotEmpty(), "$feature grew nothing on plain ground")

            for ((cornerX, cornerZ) in CORNERS) {
                val grown = grow(feature, Triple(cornerX, SURFACE_Y + 1, cornerZ))
                assertEquals(
                    "",
                    differences(centred.skeleton, grown.skeleton),
                    "$feature does not grow the same trunk and foliage at ($cornerX, $cornerZ)",
                )
                assertTrue(grown.external > 0, "$feature at ($cornerX, $cornerZ) must reach a neighbour")
            }
        }
    }

    @Test
    fun `no decorator output is lost at a chunk edge`() {
        for (feature in FAMILIES) {
            val centred = grow(feature, Triple(8, SURFACE_Y + 1, 8))
            if (centred.decorations.isEmpty() && centred.blockEntities.isEmpty()) continue
            for ((cornerX, cornerZ) in CORNERS) {
                val grown = grow(feature, Triple(cornerX, SURFACE_Y + 1, cornerZ))
                if (centred.decorations.isNotEmpty()) {
                    assertTrue(
                        grown.decorations.isNotEmpty(),
                        "$feature drops all of its decorator output at ($cornerX, $cornerZ)",
                    )
                }
                assertEquals(
                    centred.blockEntities.size,
                    grown.blockEntities.size,
                    "$feature loses a block entity at ($cornerX, $cornerZ)",
                )
            }
        }
    }

    @Test
    fun `no tree writes leaves without its trunk`() {
        for (feature in FAMILIES) {
            for ((cornerX, cornerZ) in CORNERS) {
                val grown = grow(feature, Triple(cornerX, SURFACE_Y + 1, cornerZ))
                val leaves = grown.skeleton.values.count { isLeaves(it) }
                if (leaves == 0) continue
                val logs = grown.skeleton.values.count { isLog(it) }
                assertTrue(logs > 0, "$feature wrote $leaves leaves and no log at ($cornerX, $cornerZ)")
            }
        }
    }

    private companion object {
        const val SEED = 20260829L
        const val SURFACE_Y = 70
        val CORNERS = listOf(0 to 0, 15 to 0, 0 to 15, 15 to 15)

        /** One reachable feature per trunk/foliage placer family the step-9 selectors can pick. */
        val FAMILIES =
            listOf(
                "oak",
                "fancy_oak_checked",
                "birch_checked",
                "super_birch_bees",
                "spruce",
                "pine",
                "mega_spruce_checked",
                "mega_pine_checked",
                "jungle_tree",
                "jungle_bush",
                "mega_jungle_tree_checked",
                "acacia",
                "dark_oak_checked",
                "pale_oak_checked",
                "mangrove_checked",
                "tall_mangrove_checked",
                "cherry_checked",
                "trees_swamp",
                "oak_leaf_litter",
            )
    }
}
