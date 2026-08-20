package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.registry.BlockFace
import org.basaltmc.server.registry.BlockShapes
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.WorldHeight

/**
 * // parity: net.minecraft.world.level.levelgen.feature.SimpleBlockFeature. Places a single block
 * at [origin] if it "can survive" there. Real vanilla's `BlockState.canSurvive` dispatches to each
 * block's own placement rule. Overworld vegetation dispatches to `supports_vegetation`, dry
 * vegetation to `supports_dry_vegetation`, and leaf litter to its sturdy-upper-face rule; other
 * block families retain the generic solid-ground fallback. For [isDoublePlant] blocks, the block
 * above must also be air, matching the feature's special placement of both halves at once.
 */
public class SimpleBlockFeature(
    private val toPlace: (random: WorldgenRandom, origin: DecorationPos) -> Int,
    private val isDoublePlant: Boolean = false,
) {
    public fun place(
        chunk: Chunk,
        random: WorldgenRandom,
        origin: DecorationPos,
    ): Boolean {
        // parity: vanilla asks the state provider before evaluating canSurvive, so weighted/noise
        // providers consume RNG even when the candidate ultimately cannot be placed.
        val stateId = toPlace(random, origin)
        if (origin.y !in WorldHeight.MIN_Y..WorldHeight.MAX_Y) return false
        if (!Blocks.isAir(chunk.getDecorationBlockStateId(origin.x, origin.y, origin.z))) return false
        if (origin.y - 1 < WorldHeight.MIN_Y) return false
        val belowStateId = chunk.getDecorationBlockStateId(origin.x, origin.y - 1, origin.z)
        if (!canSurvive(stateId, belowStateId)) {
            return false
        }
        if (isDoublePlant) {
            if (origin.y + 1 > WorldHeight.MAX_Y ||
                !Blocks.isAir(chunk.getDecorationBlockStateId(origin.x, origin.y + 1, origin.z))
            ) {
                return false
            }
        }

        chunk.setDecorationBlockStateId(origin.x, origin.y, origin.z, stateId)
        if (isDoublePlant) {
            val upperStateId =
                Blocks.upperDoublePlantStateId(stateId) ?: error("state $stateId is not a lower double plant")
            chunk.setDecorationBlockStateId(origin.x, origin.y + 1, origin.z, upperStateId)
        }
        return true
    }

    private fun canSurvive(
        stateId: Int,
        belowStateId: Int,
    ): Boolean {
        val path = Blocks.byStateId(stateId)?.block?.path
        val belowPath = Blocks.byStateId(belowStateId)?.block?.path
        return when {
            path in DRY_VEGETATION_PATHS -> belowPath in SUPPORTS_DRY_VEGETATION_PATHS
            path in VEGETATION_PATHS -> belowPath in SUPPORTS_VEGETATION_PATHS
            path == "leaf_litter" -> BlockShapes.isFaceSturdy(belowStateId, BlockFace.UP)
            else -> Chunk.HeightmapType.OCEAN_FLOOR.matches(belowStateId)
        }
    }

    private companion object {
        val VEGETATION_PATHS =
            setOf(
                "short_grass",
                "fern",
                "tall_grass",
                "large_fern",
                "bush",
                "firefly_bush",
                "sweet_berry_bush",
                "dandelion",
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
                "closed_eyeblossom",
                "open_eyeblossom",
                "sunflower",
                "lilac",
                "rose_bush",
                "peony",
                "pink_petals",
                "wildflowers",
            )
        val DRY_VEGETATION_PATHS = setOf("dead_bush", "short_dry_grass", "tall_dry_grass")
        val SUPPORTS_VEGETATION_PATHS =
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
                "farmland",
            )
        val SUPPORTS_DRY_VEGETATION_PATHS =
            SUPPORTS_VEGETATION_PATHS +
                setOf(
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
                )
    }
}
