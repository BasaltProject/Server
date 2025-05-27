package org.basaltmc.world.terrain.decoration

import org.basaltmc.random.WorldgenRandom
import org.basaltmc.registry.Blocks
import org.basaltmc.registry.Identifier
import org.basaltmc.world.Chunk
import org.basaltmc.world.WorldHeight

/**
 * // parity: net.minecraft.world.level.levelgen.feature.AbstractHugeMushroomFeature plus
 * HugeBrownMushroomFeature/HugeRedMushroomFeature, decompiled from the real jar. Cap and stem
 * blocks preserve their exact six visible-face properties.
 * The real `canPlaceOn` tag (`huge_*_mushroom_can_place_on`) and `replaceable_by_mushrooms` tag
 * are replicated as literal sets, with the real tag closure extracted from the jar's own data
 * (substrate_overworld = dirt/mud/moss_blocks/grass_blocks, plus mycelium/podzol/nyliums).
 */
public object HugeMushroomFeature {
    public enum class Kind { BROWN, RED }

    // parity: AbstractHugeMushroomFeature.place.
    public fun place(
        chunk: Chunk,
        random: WorldgenRandom,
        originX: Int,
        originY: Int,
        originZ: Int,
        kind: Kind,
    ): Boolean {
        // parity: getTreeHeight, drawn before the validity check exactly like the real jar.
        var treeHeight = random.nextInt(3) + 4
        if (random.nextInt(12) == 0) treeHeight *= 2
        if (!isValidPosition(chunk, originX, originY, originZ, treeHeight, kind)) return false
        makeCap(chunk, originX, originY, originZ, treeHeight, kind)
        for (dy in 0 until treeHeight) {
            placeMushroomBlock(chunk, originX, originY + dy, originZ, mushroomStemStateId)
        }
        return true
    }

    private fun isValidPosition(
        chunk: Chunk,
        originX: Int,
        originY: Int,
        originZ: Int,
        treeHeight: Int,
        kind: Kind,
    ): Boolean {
        if (originY < WorldHeight.MIN_Y + 1 || originY + treeHeight + 1 > WorldHeight.MAX_Y) return false
        val belowStateId = chunk.getBlockStateId(originX and 15, originY - 1, originZ and 15)
        val belowBlock = Blocks.byStateId(belowStateId)?.block ?: return false
        if (belowBlock !in canPlaceOnBlocks) return false
        // parity: the real isValidPosition passes treeHeight = -1 into getTreeRadiusForHeight, so
        // the red variant's own radius rule resolves to 0 for every checked layer and brown's to
        // `dy <= 3 ? 0 : 3`. The cap radius therefore only matters for brown here.
        for (dy in 0..treeHeight) {
            val radius =
                when (kind) {
                    Kind.BROWN -> if (dy <= 3) 0 else 3
                    Kind.RED -> 0
                }
            for (dx in -radius..radius) {
                for (dz in -radius..radius) {
                    val y = originY + dy
                    if (y > WorldHeight.MAX_Y) return false
                    val id = chunk.getBlockStateId((originX + dx) and 15, y, (originZ + dz) and 15)
                    if (id != Blocks.AIR.stateId && !Blocks.isLeavesState(id)) return false
                }
            }
        }
        return true
    }

    private fun makeCap(
        chunk: Chunk,
        originX: Int,
        originY: Int,
        originZ: Int,
        treeHeight: Int,
        kind: Kind,
    ) {
        when (kind) {
            // parity: HugeBrownMushroomFeature.makeCap (foliageRadius 3, single layer at treeHeight, no corners).
            Kind.BROWN ->
                for (dx in -3..3) {
                    for (dz in -3..3) {
                        val xEdge = dx == -3 || dx == 3
                        val zEdge = dz == -3 || dz == 3
                        if (xEdge && zEdge) continue
                        val west = dx == -3 || zEdge && dx == -2
                        val east = dx == 3 || zEdge && dx == 2
                        val north = dz == -3 || xEdge && dz == -2
                        val south = dz == 3 || xEdge && dz == 2
                        placeMushroomBlock(
                            chunk,
                            originX + dx,
                            originY + treeHeight,
                            originZ + dz,
                            mushroomState(
                                brownCapStateId,
                                down = false,
                                east = east,
                                north = north,
                                south = south,
                                up = true,
                                west = west,
                            ),
                        )
                    }
                }
            // parity: HugeRedMushroomFeature.makeCap (foliageRadius 2, four layers, top layer radius 1).
            Kind.RED ->
                for (dy in treeHeight - 3..treeHeight) {
                    val radius = if (dy < treeHeight) 2 else 1
                    for (dx in -radius..radius) {
                        for (dz in -radius..radius) {
                            val xEdge = dx == -radius || dx == radius
                            val zEdge = dz == -radius || dz == radius
                            if (dy >= treeHeight || xEdge != zEdge) {
                                placeMushroomBlock(
                                    chunk,
                                    originX + dx,
                                    originY + dy,
                                    originZ + dz,
                                    mushroomState(
                                        redCapStateId,
                                        down = false,
                                        east = dx > 0,
                                        north = dz < 0,
                                        south = dz > 0,
                                        up = dy >= treeHeight - 1,
                                        west = dx < 0,
                                    ),
                                )
                            }
                        }
                    }
                }
        }
    }

    /** // parity: AbstractHugeMushroomFeature.placeMushroomBlock. */
    private fun placeMushroomBlock(
        chunk: Chunk,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
    ) {
        if (y < WorldHeight.MIN_Y || y > WorldHeight.MAX_Y) return
        val current = chunk.getBlockStateId(x and 15, y, z and 15)
        val currentBlock = Blocks.byStateId(current)?.block
        if (current == Blocks.AIR.stateId || currentBlock != null && currentBlock in replaceableBlocks) {
            chunk.setBlockStateId(x and 15, y, z and 15, stateId)
        }
    }

    private fun state(name: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(name))?.stateId ?: error("unknown block '$name'")

    private val brownCapStateId = state("brown_mushroom_block")
    private val redCapStateId = state("red_mushroom_block")
    private val mushroomStemStateId =
        mushroomState(
            state("mushroom_stem"),
            down = false,
            east = true,
            north = true,
            south = true,
            up = false,
            west = true,
        )

    private fun mushroomState(
        stateId: Int,
        down: Boolean,
        east: Boolean,
        north: Boolean,
        south: Boolean,
        up: Boolean,
        west: Boolean,
    ): Int =
        Blocks.hugeMushroomStateId(stateId, down, east, north, south, up, west)
            ?: error("invalid huge mushroom state '$stateId'")

    // parity: block tags huge_brown/red_mushroom_can_place_on (identical contents for both).
    private val canPlaceOnBlocks: Set<Identifier> =
        listOf(
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
            "crimson_nylium",
            "warped_nylium",
        ).map(Identifier::vanilla).toSet()

    // parity: block tag replaceable_by_mushrooms.
    private val replaceableBlocks: Set<Identifier> =
        listOf(
            "jungle_leaves",
            "oak_leaves",
            "spruce_leaves",
            "pale_oak_leaves",
            "dark_oak_leaves",
            "acacia_leaves",
            "birch_leaves",
            "azalea_leaves",
            "flowering_azalea_leaves",
            "mangrove_leaves",
            "cherry_leaves",
            "dandelion",
            "open_eyeblossom",
            "poppy",
            "blue_orchid",
            "allium",
            "azure_bluet",
            "red_tulip",
            "orange_tulip",
            "white_tulip",
            "pink_tulip",
            "oxeye_daisy",
            "cornflower",
            "lily_of_the_valley",
            "wither_rose",
            "torchflower",
            "closed_eyeblossom",
            "golden_dandelion",
            "pale_moss_carpet",
            "short_grass",
            "fern",
            "dead_bush",
            "vine",
            "glow_lichen",
            "sunflower",
            "lilac",
            "rose_bush",
            "peony",
            "tall_grass",
            "large_fern",
            "hanging_roots",
            "pitcher_plant",
            "water",
            "seagrass",
            "tall_seagrass",
            "brown_mushroom",
            "red_mushroom",
            "brown_mushroom_block",
            "red_mushroom_block",
            "warped_roots",
            "nether_sprouts",
            "crimson_roots",
            "leaf_litter",
            "short_dry_grass",
            "tall_dry_grass",
            "bush",
            "firefly_bush",
        ).map(Identifier::vanilla).toSet()
}
