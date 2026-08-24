package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.registry.BlockFace
import org.basaltmc.server.registry.BlockShapes
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.wire.common.Identifier

/**
 * The small vanilla features that are one block, one column or one pile: what is left of the
 * `/place feature` registry once the big decoration steps have their own bodies.
 *
 * parity: `SimpleBlockFeature`, `BlockColumnFeature` and `BlockPileFeature`, each rolled at the
 * position it was given. The placement modifiers that decide *where* generation puts one are the
 * command's business, not theirs.
 */
public object SmallFeatures {
    /** Every vanilla placed feature name this file answers, and what it places. */
    public val PLACED_IDS: Set<String>
        get() = SINGLE_BLOCKS.keys + COLUMNS.keys + PILES.keys + CEILING_BLOCKS.keys

    public fun placeAt(
        chunk: Chunk,
        random: WorldgenRandom,
        origin: DecorationPos,
        placedFeature: String,
    ): Boolean {
        SINGLE_BLOCKS[placedFeature]?.let { return placePatch(chunk, random, origin, it) }
        CEILING_BLOCKS[placedFeature]?.let { return placeUnderCeiling(chunk, random, origin, it) }
        COLUMNS[placedFeature]?.let { return placeColumn(chunk, random, origin, it) }
        PILES[placedFeature]?.let { return placePile(chunk, random, origin, it) }
        return false
    }

    /** parity: `random_patch` around the origin, 96 tries with the usual 7/3 spread. */
    private fun placePatch(
        chunk: Chunk,
        random: WorldgenRandom,
        origin: DecorationPos,
        stateProvider: (WorldgenRandom) -> Int,
    ): Boolean {
        var placed = false
        repeat(PATCH_TRIES) {
            val target =
                DecorationPos(
                    origin.x + random.nextInt(8) - random.nextInt(8),
                    origin.y + random.nextInt(4) - random.nextInt(4),
                    origin.z + random.nextInt(8) - random.nextInt(8),
                )
            val stateId = stateProvider(random)
            if (!isAir(chunk, target)) return@repeat
            if (target.y <= WorldHeight.MIN_Y) return@repeat
            if (SimpleBlockFeature({ _, _ -> stateId }).place(chunk, random, target)) placed = true
        }
        return placed
    }

    /** parity: `spore_blossom`, which hangs from the block above rather than standing on one. */
    private fun placeUnderCeiling(
        chunk: Chunk,
        random: WorldgenRandom,
        origin: DecorationPos,
        stateProvider: (WorldgenRandom) -> Int,
    ): Boolean {
        if (!isAir(chunk, origin) || origin.y + 1 > WorldHeight.MAX_Y) return false
        if (!BlockShapes.supportsCentre(stateAt(chunk, origin.copy(y = origin.y + 1)), BlockFace.DOWN)) return false
        set(chunk, origin, stateProvider(random))
        return true
    }

    /** parity: `BlockColumnFeature`, a stack of one block grown upward while there is room. */
    private fun placeColumn(
        chunk: Chunk,
        random: WorldgenRandom,
        origin: DecorationPos,
        column: Column,
    ): Boolean {
        if (!isAir(chunk, origin)) return false
        val below = stateAt(chunk, origin.copy(y = origin.y - 1))
        if (Blocks.byStateId(below)?.block?.path !in column.support) return false
        // parity: `UniformInt.biasedToBottom`, which is what sugar cane's own height provider is.
        val height = column.minHeight + random.nextInt(random.nextInt(column.maxHeight - column.minHeight + 1) + 1)
        var placed = 0
        while (placed < height) {
            val target = origin.copy(y = origin.y + placed)
            if (target.y > WorldHeight.MAX_Y || !isAir(chunk, target)) break
            set(chunk, target, column.stateId)
            placed++
        }
        return placed > 0
    }

    /** parity: `BlockPileFeature`, a squashed blob whose radius is drawn per column. */
    private fun placePile(
        chunk: Chunk,
        random: WorldgenRandom,
        origin: DecorationPos,
        stateProvider: (WorldgenRandom) -> Int,
    ): Boolean {
        if (origin.y < WorldHeight.MIN_Y + 5) return false
        val radiusX = 2 + random.nextInt(2)
        val radiusZ = 2 + random.nextInt(2)
        for (z in origin.z - radiusZ..origin.z + radiusZ) {
            for (y in origin.y..origin.y + 1) {
                for (x in origin.x - radiusX..origin.x + radiusX) {
                    val dx = origin.x - x
                    val dz = origin.z - z
                    val inside = dx * dx + dz * dz <= random.nextFloat() * 10.0f - random.nextFloat() * 6.0f
                    if (inside || random.nextFloat() < STRAY_BLOCK_CHANCE) {
                        tryPlacePile(chunk, random, DecorationPos(x, y, z), stateProvider)
                    }
                }
            }
        }
        return true
    }

    private fun tryPlacePile(
        chunk: Chunk,
        random: WorldgenRandom,
        target: DecorationPos,
        stateProvider: (WorldgenRandom) -> Int,
    ) {
        if (target.y !in WorldHeight.MIN_Y..WorldHeight.MAX_Y || !isAir(chunk, target)) return
        val below = stateAt(chunk, target.copy(y = target.y - 1))
        val belowPath = Blocks.byStateId(below)?.block?.path
        // parity: a dirt path holds a pile only half the time; anything else needs a solid top face.
        val holds =
            if (belowPath ==
                "dirt_path"
            ) {
                random.nextBoolean()
            } else {
                BlockShapes.isFaceSturdy(below, BlockFace.UP)
            }
        if (!holds) return
        set(chunk, target, stateProvider(random))
    }

    private fun isAir(
        chunk: Chunk,
        position: DecorationPos,
    ): Boolean = Blocks.isAir(stateAt(chunk, position))

    private fun stateAt(
        chunk: Chunk,
        position: DecorationPos,
    ): Int {
        if (position.y !in WorldHeight.MIN_Y..WorldHeight.MAX_Y) return Blocks.AIR.stateId
        if (position.x shr 4 != chunk.chunkX || position.z shr 4 != chunk.chunkZ) return Blocks.AIR.stateId
        return chunk.getBlockStateId(position.x and 15, position.y, position.z and 15)
    }

    private fun set(
        chunk: Chunk,
        position: DecorationPos,
        stateId: Int,
    ) {
        if (position.x shr 4 != chunk.chunkX || position.z shr 4 != chunk.chunkZ) return
        chunk.setBlockStateId(position.x and 15, position.y, position.z and 15, stateId)
    }

    private data class Column(
        val stateId: Int,
        val minHeight: Int,
        val maxHeight: Int,
        val support: Set<String>,
    )

    private fun state(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: Blocks.AIR.stateId

    private fun uniformStates(
        path: String,
        property: String,
        values: List<String>,
    ): (WorldgenRandom) -> Int {
        val states =
            values.map { value ->
                Blocks.stateIdOf(Identifier.vanilla(path), mapOf(property to value)) ?: state(path)
            }
        return { random -> states[random.nextInt(states.size)] }
    }

    private fun flowerBedStates(
        path: String,
        amountProperty: String,
        maxAmount: Int,
    ): (WorldgenRandom) -> Int {
        val states =
            (1..maxAmount).flatMap { amount ->
                listOf("north", "east", "south", "west").map { facing ->
                    Blocks.stateIdOf(
                        Identifier.vanilla(path),
                        mapOf("facing" to facing, amountProperty to amount.toString()),
                    ) ?: error("unknown $path state $facing/$amount")
                }
            }
        return { random -> states[random.nextInt(states.size)] }
    }

    private val SINGLE_BLOCKS: Map<String, (WorldgenRandom) -> Int> =
        mapOf(
            "patch_leaf_litter" to flowerBedStates("leaf_litter", "segment_amount", 3),
            "wildflowers_birch_forest" to flowerBedStates("wildflowers", "flower_amount", 4),
            "wildflowers_meadow" to flowerBedStates("wildflowers", "flower_amount", 4),
            "patch_firefly_bush_near_water" to { _ -> state("firefly_bush") },
            "patch_firefly_bush_near_water_swamp" to { _ -> state("firefly_bush") },
            "patch_firefly_bush_swamp" to { _ -> state("firefly_bush") },
        )

    private val CEILING_BLOCKS: Map<String, (WorldgenRandom) -> Int> =
        mapOf("spore_blossom" to { _ -> state("spore_blossom") })

    private val COLUMNS: Map<String, Column> =
        buildMap {
            // parity: `patch_sugar_cane`, two to four tall biased to the bottom, on sand, dirt or moss.
            val sugarCane =
                Column(
                    state("sugar_cane"),
                    minHeight = 2,
                    maxHeight = 4,
                    support = setOf("grass_block", "dirt", "coarse_dirt", "podzol", "sand", "red_sand", "moss_block"),
                )
            for (name in listOf(
                "patch_sugar_cane",
                "patch_sugar_cane_desert",
                "patch_sugar_cane_badlands",
                "patch_sugar_cane_swamp",
            )) {
                put(name, sugarCane)
            }
        }

    private val PILES: Map<String, (WorldgenRandom) -> Int> =
        mapOf(
            "pile_hay" to uniformStates("hay_block", "axis", listOf("x", "y", "z")),
            "pile_snow" to { _ -> state("snow") },
            "pile_melon" to { _ -> state("melon") },
            "pile_ice" to { random -> if (random.nextInt(6) == 0) state("blue_ice") else state("packed_ice") },
            "pile_pumpkin" to { random -> if (random.nextInt(20) == 19) state("jack_o_lantern") else state("pumpkin") },
        )

    private const val PATCH_TRIES = 96
    private const val STRAY_BLOCK_CHANCE = 0.031f
}
