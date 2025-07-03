package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.registry.BlockFace
import org.basaltmc.server.registry.BlockShapes
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.wire.common.Identifier

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
    private val scheduleTick: Boolean = false,
) {
    public fun place(
        chunk: Chunk,
        random: WorldgenRandom,
        origin: DecorationPos,
        mossyCarpetRandom: BasaltRandom = random,
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

        if (Blocks.byStateId(stateId)?.block == PALE_MOSS_CARPET_ID) {
            placePaleMossCarpet(chunk, mossyCarpetRandom, origin)
        } else {
            chunk.setDecorationBlockStateId(origin.x, origin.y, origin.z, stateId)
            if (isDoublePlant) {
                val upperStateId =
                    Blocks.upperDoublePlantStateId(stateId) ?: error("state $stateId is not a lower double plant")
                chunk.setDecorationBlockStateId(origin.x, origin.y + 1, origin.z, upperStateId)
            }
        }
        if (scheduleTick) {
            val placedStateId = chunk.getDecorationBlockStateId(origin.x, origin.y, origin.z)
            val block = Blocks.byStateId(placedStateId)?.block ?: return true
            chunk.scheduleDecorationBlockTick(
                origin.x,
                origin.y,
                origin.z,
                expectedStateId = placedStateId,
                type = block,
                delay = 1,
            )
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
            path == "pale_moss_carpet" -> !Blocks.isAir(belowStateId)
            path in DRY_VEGETATION_PATHS -> belowPath in SUPPORTS_DRY_VEGETATION_PATHS
            path in VEGETATION_PATHS -> belowPath in SUPPORTS_VEGETATION_PATHS
            path == "leaf_litter" -> BlockShapes.isFaceSturdy(belowStateId, BlockFace.UP)
            else -> Chunk.HeightmapType.OCEAN_FLOOR.matches(belowStateId)
        }
    }

    /** parity: `MossyCarpetBlock.placeAt`, including its optional wall-only upper layer. */
    private fun placePaleMossCarpet(
        chunk: Chunk,
        random: BasaltRandom,
        origin: DecorationPos,
    ) {
        val bottom = updatedPaleMossState(PALE_MOSS_CARPET_STATE_ID, chunk, origin, createSides = true)
        chunk.setDecorationBlockStateId(origin.x, origin.y, origin.z, bottom)

        val topper = createPaleMossTopper(chunk, random, origin)
        if (topper == null) return
        chunk.setDecorationBlockStateId(origin.x, origin.y + 1, origin.z, topper)
        val updatedBottom = updatedPaleMossState(bottom, chunk, origin, createSides = true)
        chunk.setDecorationBlockStateId(origin.x, origin.y, origin.z, updatedBottom)
    }

    private fun createPaleMossTopper(
        chunk: Chunk,
        random: BasaltRandom,
        origin: DecorationPos,
    ): Int? {
        val above = origin.copy(y = origin.y + 1)
        val previous = chunk.getDecorationBlockStateId(above.x, above.y, above.z)
        val previousState = Blocks.byStateId(previous)
        val previousIsCarpet = previousState?.block == PALE_MOSS_CARPET_ID
        val previousIsBase = previousIsCarpet && previousState.properties["bottom"] == "true"
        if ((!previousIsCarpet || !previousIsBase) &&
            (previousIsCarpet || BlockShapes.isReplaceable(previous))
        ) {
            var topper = updatedPaleMossState(PALE_MOSS_TOPPER_STATE_ID, chunk, above, createSides = true)
            var properties = checkNotNull(Blocks.byStateId(topper)).properties
            for (direction in HORIZONTAL_DIRECTIONS) {
                if (properties[direction.property] == "none" || random.nextBoolean()) continue
                properties = properties + (direction.property to "none")
                topper = paleMossState(properties)
            }
            val hasFaces = HORIZONTAL_DIRECTIONS.any { properties[it.property] != "none" }
            if (hasFaces && topper != previous) return topper
        }
        return null
    }

    private fun updatedPaleMossState(
        stateId: Int,
        chunk: Chunk,
        origin: DecorationPos,
        createSides: Boolean,
    ): Int {
        val state = checkNotNull(Blocks.byStateId(stateId))
        var properties = state.properties
        val isBase = properties["bottom"] == "true"
        val shouldCreateSides = createSides || isBase
        var aboveProperties: Map<String, String>? = null
        var belowProperties: Map<String, String>? = null

        for (direction in HORIZONTAL_DIRECTIONS) {
            var side =
                if (canSupportPaleMossAtFace(chunk, origin, direction.face)) {
                    if (shouldCreateSides) "low" else properties.getValue(direction.property)
                } else {
                    "none"
                }
            if (side == "low") {
                if (aboveProperties == null) {
                    val above = chunk.getDecorationBlockStateId(origin.x, origin.y + 1, origin.z)
                    aboveProperties =
                        if (Blocks.byStateId(above)?.block == PALE_MOSS_CARPET_ID) {
                            Blocks.byStateId(above)?.properties
                        } else {
                            emptyMap()
                        }
                }
                if (aboveProperties?.get(direction.property) != "none" &&
                    aboveProperties?.get("bottom") == "false"
                ) {
                    side = "tall"
                }
                if (!isBase) {
                    if (belowProperties == null) {
                        val below = chunk.getDecorationBlockStateId(origin.x, origin.y - 1, origin.z)
                        belowProperties =
                            if (Blocks.byStateId(below)?.block == PALE_MOSS_CARPET_ID) {
                                Blocks.byStateId(below)?.properties
                            } else {
                                emptyMap()
                            }
                    }
                    if (belowProperties?.get(direction.property) == "none") side = "none"
                }
            }
            properties = properties + (direction.property to side)
        }
        return paleMossState(properties)
    }

    private fun canSupportPaleMossAtFace(
        chunk: Chunk,
        origin: DecorationPos,
        face: BlockFace,
    ): Boolean {
        val neighbour =
            chunk.getDecorationBlockStateId(
                origin.x + face.stepX,
                origin.y + face.stepY,
                origin.z + face.stepZ,
            )
        val attachmentFace = face.opposite
        return BlockShapes.isFaceSturdy(neighbour, attachmentFace) ||
            BlockShapes.faceCovers(
                neighbour,
                attachmentFace,
                0,
                BlockShapes.UNITS,
                0,
                BlockShapes.UNITS,
            )
    }

    private fun paleMossState(properties: Map<String, String>): Int =
        Blocks.stateIdOf(PALE_MOSS_CARPET_ID, properties)
            ?: error("unknown pale moss carpet state $properties")

    private companion object {
        data class HorizontalDirection(
            val face: BlockFace,
            val property: String,
        )

        val PALE_MOSS_CARPET_ID = Identifier.vanilla("pale_moss_carpet")
        val PALE_MOSS_CARPET_STATE_ID =
            Blocks.REGISTRY.byIdentifier(PALE_MOSS_CARPET_ID)?.stateId
                ?: error("missing pale moss carpet")
        val PALE_MOSS_TOPPER_STATE_ID =
            Blocks.stateIdOf(PALE_MOSS_CARPET_ID, mapOf("bottom" to "false"))
                ?: error("missing pale moss carpet topper")
        val HORIZONTAL_DIRECTIONS =
            listOf(
                HorizontalDirection(BlockFace.NORTH, "north"),
                HorizontalDirection(BlockFace.EAST, "east"),
                HorizontalDirection(BlockFace.SOUTH, "south"),
                HorizontalDirection(BlockFace.WEST, "west"),
            )
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
