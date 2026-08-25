package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.registry.BlockFace
import org.basaltmc.server.registry.BlockShapes
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.registry.hasWaterFluid
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.terrain.decoration.tree.TreeConfigs
import org.basaltmc.server.world.terrain.decoration.tree.TreeFeature
import org.basaltmc.wire.common.Identifier

/** Root-system body and bending azalea tree used by `rooted_azalea_tree`. */
internal object RootedAzaleaFeature {
    fun place(
        chunk: Chunk,
        random: WorldgenRandom,
        origin: DecorationPos,
    ): Boolean {
        if (!Blocks.isAir(blockAt(chunk, origin))) return false
        var position = origin
        for (columnOffset in 0 until ROOT_COLUMN_MAX_HEIGHT) {
            position = position.copy(y = position.y + 1)
            if (aboveSearchLimit(chunk, position)) return true
            if (!allowedTreePosition(chunk, position) || !spaceForTree(chunk, position)) continue
            val below = blockAt(chunk, position.copy(y = position.y - 1))
            if (Blocks.byStateId(below)?.block == LAVA_BLOCK ||
                !BlockShapes.isFaceSturdy(below, BlockFace.UP)
            ) {
                return true
            }
            if (!TreeFeature.place(chunk, random, position.x, position.y, position.z, TreeConfigs.AZALEA)) continue
            placeRootedDirt(chunk, random, origin, origin.y + columnOffset)
            placeHangingRoots(chunk, random, origin)
            return true
        }
        return true
    }

    private fun aboveSearchLimit(
        chunk: Chunk,
        position: DecorationPos,
    ): Boolean =
        position.y > WorldHeight.MAX_Y ||
            position.y >
            chunk.heightmap(Chunk.HeightmapType.WORLD_SURFACE)[(position.x and 15) + (position.z and 15) * 16]

    private fun allowedTreePosition(
        chunk: Chunk,
        position: DecorationPos,
    ): Boolean {
        val state = blockAt(chunk, position)
        val below = blockAt(chunk, position.copy(y = position.y - 1))
        return isTreeReplaceable(state) && Blocks.byStateId(below)?.block in AZALEA_GROWS_ON
    }

    private fun spaceForTree(
        chunk: Chunk,
        position: DecorationPos,
    ): Boolean {
        for (distance in 1..REQUIRED_VERTICAL_SPACE) {
            val state = blockAt(chunk, position.copy(y = position.y + distance))
            if (!Blocks.isAir(state) && !(distance + 1 <= ALLOWED_VERTICAL_WATER && Blocks.hasWaterFluid(state))) {
                return false
            }
        }
        return true
    }

    private fun placeRootedDirt(
        chunk: Chunk,
        random: WorldgenRandom,
        origin: DecorationPos,
        targetHeight: Int,
    ) {
        for (y in origin.y until targetHeight) {
            repeat(ROOT_PLACEMENT_ATTEMPTS) {
                val position =
                    DecorationPos(
                        origin.x + random.nextInt(ROOT_RADIUS) - random.nextInt(ROOT_RADIUS),
                        y,
                        origin.z + random.nextInt(ROOT_RADIUS) - random.nextInt(ROOT_RADIUS),
                    )
                if (Blocks.byStateId(blockAt(chunk, position))?.block in ROOT_REPLACEABLE) {
                    set(chunk, position, ROOTED_DIRT_STATE_ID)
                }
            }
        }
    }

    private fun placeHangingRoots(
        chunk: Chunk,
        random: WorldgenRandom,
        origin: DecorationPos,
    ) {
        repeat(HANGING_ROOT_PLACEMENT_ATTEMPTS) {
            val position =
                DecorationPos(
                    origin.x + random.nextInt(HANGING_ROOT_RADIUS) - random.nextInt(HANGING_ROOT_RADIUS),
                    origin.y + random.nextInt(HANGING_ROOT_VERTICAL_SPAN) - random.nextInt(HANGING_ROOT_VERTICAL_SPAN),
                    origin.z + random.nextInt(HANGING_ROOT_RADIUS) - random.nextInt(HANGING_ROOT_RADIUS),
                )
            if (!Blocks.isAir(blockAt(chunk, position))) return@repeat
            val above = blockAt(chunk, position.copy(y = position.y + 1))
            if (BlockShapes.isFaceSturdy(above, BlockFace.DOWN)) set(chunk, position, HANGING_ROOTS_STATE_ID)
        }
    }

    private fun isTreeReplaceable(stateId: Int): Boolean =
        Blocks.isAir(stateId) || Blocks.byStateId(stateId)?.block in TREE_REPLACEABLE

    private fun blockAt(
        chunk: Chunk,
        position: DecorationPos,
    ): Int = chunk.getDecorationBlockStateId(position.x, position.y, position.z)

    private fun set(
        chunk: Chunk,
        position: DecorationPos,
        stateId: Int,
    ) {
        chunk.setDecorationBlockStateId(position.x, position.y, position.z, stateId)
    }

    private fun state(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")

    private val TREE_REPLACEABLE =
        setOf(
            "oak_leaves",
            "spruce_leaves",
            "birch_leaves",
            "jungle_leaves",
            "acacia_leaves",
            "dark_oak_leaves",
            "mangrove_leaves",
            "cherry_leaves",
            "pale_oak_leaves",
            "azalea_leaves",
            "flowering_azalea_leaves",
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
            "short_grass",
            "fern",
            "dead_bush",
            "vine",
            "glow_lichen",
            "sunflower",
            "lilac",
            "rose_bush",
            "peony",
            "hanging_roots",
            "tall_grass",
            "large_fern",
            "pale_moss_carpet",
            "pitcher_plant",
            "water",
            "seagrass",
            "tall_seagrass",
            "bush",
            "firefly_bush",
            "warped_roots",
            "nether_sprouts",
            "crimson_roots",
            "leaf_litter",
            "short_dry_grass",
            "tall_dry_grass",
        ).map(Identifier::vanilla).toSet()
    private val AZALEA_GROWS_ON =
        setOf(
            "stone",
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
            "sand",
            "red_sand",
            "suspicious_sand",
            "terracotta",
            "white_terracotta",
            "orange_terracotta",
            "magenta_terracotta",
            "light_blue_terracotta",
            "yellow_terracotta",
            "lime_terracotta",
            "pink_terracotta",
            "gray_terracotta",
            "light_gray_terracotta",
            "cyan_terracotta",
            "purple_terracotta",
            "blue_terracotta",
            "brown_terracotta",
            "green_terracotta",
            "red_terracotta",
            "black_terracotta",
            "snow_block",
            "powder_snow",
        ).map(Identifier::vanilla).toSet()
    private val ROOT_REPLACEABLE =
        (AZALEA_GROWS_ON - Identifier.vanilla("suspicious_sand")) +
            setOf(
                "granite",
                "diorite",
                "andesite",
                "tuff",
                "deepslate",
                "clay",
                "gravel",
                "white_terracotta",
                "orange_terracotta",
                "magenta_terracotta",
                "light_blue_terracotta",
                "yellow_terracotta",
                "lime_terracotta",
                "pink_terracotta",
                "gray_terracotta",
                "light_gray_terracotta",
                "cyan_terracotta",
                "purple_terracotta",
                "blue_terracotta",
                "brown_terracotta",
                "green_terracotta",
                "red_terracotta",
                "black_terracotta",
                "terracotta",
            ).map(Identifier::vanilla)

    private val ROOTED_DIRT_STATE_ID = state("rooted_dirt")
    private val HANGING_ROOTS_STATE_ID = state("hanging_roots")
    private val LAVA_BLOCK = Identifier.vanilla("lava")
    private const val ROOT_COLUMN_MAX_HEIGHT = 100
    private const val REQUIRED_VERTICAL_SPACE = 3
    private const val ALLOWED_VERTICAL_WATER = 2
    private const val ROOT_PLACEMENT_ATTEMPTS = 20
    private const val ROOT_RADIUS = 3
    private const val HANGING_ROOT_PLACEMENT_ATTEMPTS = 20
    private const val HANGING_ROOT_RADIUS = 3
    private const val HANGING_ROOT_VERTICAL_SPAN = 2
}
