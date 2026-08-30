package org.basaltmc.server.world.terrain

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.ChunkGeneratedEntity
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.structure.StructureBoundingBox
import org.basaltmc.server.world.structure.StructureDirection
import org.basaltmc.server.world.structure.StructureMirror
import org.basaltmc.server.world.structure.StructurePlacementGrid
import org.basaltmc.server.world.structure.StructurePos
import org.basaltmc.server.world.structure.StructureRotation
import org.basaltmc.server.world.structure.StructureStartRecord
import org.basaltmc.server.world.structure.StructureTemplate
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtCompound

/**
 * The swamp hut, vanilla's one-piece scattered structure. // parity: SwampHutStructure. The piece is
 * 7 by 9 from the chunk's own minimum corner, so it never leaves its chunk whichever way it faces.
 */
public class SwampHutStructureGenerator(
    private val biomeAt: (x: Int, y: Int, z: Int) -> String,
    /** The noise column's own surface, not the chunk heightmap: the carvers have already run. */
    private val surfaceHeightAt: (x: Int, z: Int) -> Int,
) {
    public fun generateInto(
        chunk: Chunk,
        seed: Long,
    ) {
        val chunkX = chunk.chunkX
        val chunkZ = chunk.chunkZ
        if (!GRID.isStructureChunk(seed, chunkX, chunkZ)) return

        // parity: Structure.GenerationContext's own random, independent of the decoration stream.
        val random = WorldgenRandom(LegacyRandom(0L))
        random.setLargeFeatureSeed(seed, chunkX, chunkZ)

        // parity: onTopOfChunkCenter over WORLD_SURFACE_WG, which is where the biome is tested.
        val centreX = chunkX * 16 + 8
        val centreZ = chunkZ * 16 + 8
        val stubY = surfaceHeightAt(centreX, centreZ) - 1
        if (biomeAt(centreX, stubY, centreZ).substringAfter(':') != BIOME) return

        val orientation = HORIZONTAL[random.nextInt(HORIZONTAL.size)]
        val piece = Piece(chunkX * 16, chunkZ * 16, orientation)
        if (!piece.settleOnGround(chunk)) return
        piece.place(chunk)
        chunk.structures.add(
            StructureStartRecord(NAME, chunkX, chunkZ, piece.bounds, listOf(piece.bounds)),
        )
    }

    /**
     * // parity: ScatteredFeaturePiece plus SwampHutPiece, kept together because the hut is the
     * only user: the shared base class exists in the jar for the pyramids and the igloo, which
     * are not written yet.
     */
    private class Piece(
        west: Int,
        north: Int,
        private val orientation: StructureDirection,
    ) {
        var bounds: StructureBoundingBox = makeBoundingBox(west, FLOOR_Y, north, orientation)
            private set

        /**
         * // parity: updateAverageGroundHeight, which averages MOTION_BLOCKING_NO_LEAVES over the
         * whole footprint and drops the box onto it. Vanilla only averages the columns inside the
         * chunk being written; here that is every column, because the piece never leaves its chunk.
         */
        fun settleOnGround(chunk: Chunk): Boolean {
            var total = 0
            var count = 0
            for (z in bounds.minZ..bounds.maxZ) {
                for (x in bounds.minX..bounds.maxX) {
                    total += chunk.heightAt(x and 15, z and 15, Chunk.HeightmapType.MOTION_BLOCKING_NO_LEAVES)
                    count++
                }
            }
            if (count == 0) return false
            bounds =
                bounds.move(
                    org.basaltmc.server.world.structure
                        .StructurePos(0, total / count - bounds.minY, 0),
                )
            return true
        }

        fun place(chunk: Chunk) {
            box(chunk, 1, 1, 1, 5, 1, 7, SPRUCE_PLANKS)
            box(chunk, 1, 4, 2, 5, 4, 7, SPRUCE_PLANKS)
            box(chunk, 2, 1, 0, 4, 1, 0, SPRUCE_PLANKS)
            box(chunk, 2, 2, 2, 3, 3, 2, SPRUCE_PLANKS)
            box(chunk, 1, 2, 3, 1, 3, 6, SPRUCE_PLANKS)
            box(chunk, 5, 2, 3, 5, 3, 6, SPRUCE_PLANKS)
            box(chunk, 2, 2, 7, 4, 3, 7, SPRUCE_PLANKS)
            box(chunk, 1, 0, 2, 1, 3, 2, OAK_LOG)
            box(chunk, 5, 0, 2, 5, 3, 2, OAK_LOG)
            box(chunk, 1, 0, 7, 1, 3, 7, OAK_LOG)
            box(chunk, 5, 0, 7, 5, 3, 7, OAK_LOG)
            set(chunk, OAK_FENCE, 2, 3, 2)
            set(chunk, OAK_FENCE, 3, 3, 7)
            set(chunk, AIR, 1, 3, 4)
            set(chunk, AIR, 5, 3, 4)
            set(chunk, AIR, 5, 3, 5)
            set(chunk, POTTED_RED_MUSHROOM, 1, 3, 5)
            set(chunk, CRAFTING_TABLE, 3, 2, 6)
            set(chunk, CAULDRON, 4, 2, 6)
            set(chunk, OAK_FENCE, 1, 2, 1)
            set(chunk, OAK_FENCE, 5, 2, 1)
            placeRoof(chunk)
            // The four stilts, poured down from under the floor until they hit something solid.
            for (z in 2..7 step 5) {
                for (x in 1..5 step 4) fillColumnDown(chunk, OAK_LOG, x, -1, z)
            }
            spawnResidents(chunk)
        }

        private fun placeRoof(chunk: Chunk) {
            box(chunk, 0, 4, 1, 6, 4, 1, stairs("north", "straight"))
            box(chunk, 0, 4, 2, 0, 4, 7, stairs("east", "straight"))
            box(chunk, 6, 4, 2, 6, 4, 7, stairs("west", "straight"))
            box(chunk, 0, 4, 8, 6, 4, 8, stairs("south", "straight"))
            set(chunk, stairs("north", "outer_right"), 0, 4, 1)
            set(chunk, stairs("north", "outer_left"), 6, 4, 1)
            set(chunk, stairs("south", "outer_left"), 0, 4, 8)
            set(chunk, stairs("south", "outer_right"), 6, 4, 8)
        }

        /**
         * // parity: the witch and the cat, both persistent and both at the same block. Vanilla
         * guards each with a flag on the piece so a re-entered chunk does not spawn a second one;
         * a chunk here generates once, so the guard has nothing to remember.
         */
        private fun spawnResidents(chunk: Chunk) {
            val x = worldX(2, 5) + 0.5
            val y = worldY(2).toDouble()
            val z = worldZ(2, 5) + 0.5
            for (type in listOf("witch", "cat")) {
                chunk.addGeneratedEntity(
                    ChunkGeneratedEntity(
                        type = Identifier.vanilla(type),
                        x = x,
                        y = y,
                        z = z,
                        yaw = 0.0f,
                        pitch = 0.0f,
                        data =
                            org.basaltmc.wire.nbt
                                .NbtCompound()
                                .apply { putBoolean("PersistenceRequired", true) },
                    ),
                )
            }
        }

        private fun box(
            chunk: Chunk,
            x0: Int,
            y0: Int,
            z0: Int,
            x1: Int,
            y1: Int,
            z1: Int,
            stateId: Int,
        ) {
            // The hut fills every box solid, so vanilla's edge/fill split collapses to one state.
            for (y in y0..y1) {
                for (x in x0..x1) {
                    for (z in z0..z1) set(chunk, stateId, x, y, z)
                }
            }
        }

        private fun set(
            chunk: Chunk,
            stateId: Int,
            x: Int,
            y: Int,
            z: Int,
        ) {
            val worldY = worldY(y)
            if (worldY !in WorldHeight.MIN_Y..WorldHeight.MAX_Y) return
            chunk.setBlockStateId(worldX(x, z) and 15, worldY, worldZ(x, z) and 15, transformed(stateId))
        }

        private fun fillColumnDown(
            chunk: Chunk,
            stateId: Int,
            x: Int,
            startY: Int,
            z: Int,
        ) {
            val localX = worldX(x, z) and 15
            val localZ = worldZ(x, z) and 15
            var y = worldY(startY)
            while (y > WorldHeight.MIN_Y + 1 && isReplaceableByStructures(chunk.getBlockStateId(localX, y, localZ))) {
                chunk.setBlockStateId(localX, y, localZ, stateId)
                y--
            }
        }

        /** // parity: StructurePiece.isReplaceableByStructures. */
        private fun isReplaceableByStructures(stateId: Int): Boolean {
            if (Blocks.isAir(stateId)) return true
            return Blocks.byStateId(stateId)?.block?.path in REPLACEABLE
        }

        /** // parity: the mirror and rotation setOrientation derives from the piece's facing. */
        private val mirror =
            when (orientation) {
                StructureDirection.SOUTH, StructureDirection.WEST -> StructureMirror.LEFT_RIGHT
                else -> StructureMirror.NONE
            }
        private val rotation =
            when (orientation) {
                StructureDirection.WEST, StructureDirection.EAST ->
                    StructureRotation.CLOCKWISE_90
                else -> StructureRotation.NONE
            }

        private fun transformed(stateId: Int): Int {
            val properties = Blocks.byStateId(stateId)?.properties ?: return stateId
            if (properties.isEmpty()) return stateId
            return TEMPLATE.transformedStateId(stateId, properties, mirror, rotation)
        }

        private fun worldX(
            x: Int,
            z: Int,
        ): Int =
            when (orientation) {
                StructureDirection.NORTH, StructureDirection.SOUTH -> bounds.minX + x
                StructureDirection.WEST -> bounds.maxX - z
                StructureDirection.EAST -> bounds.minX + z
                else -> x
            }

        private fun worldY(y: Int): Int = y + bounds.minY

        private fun worldZ(
            x: Int,
            z: Int,
        ): Int =
            when (orientation) {
                StructureDirection.NORTH -> bounds.maxZ - z
                StructureDirection.SOUTH -> bounds.minZ + z
                StructureDirection.WEST, StructureDirection.EAST -> bounds.minZ + x
                else -> z
            }

        private companion object {
            /** // parity: StructurePiece.makeBoundingBox, which swaps width and depth on the X axis. */
            fun makeBoundingBox(
                x: Int,
                y: Int,
                z: Int,
                orientation: StructureDirection,
            ): StructureBoundingBox =
                if (orientation == StructureDirection.NORTH || orientation == StructureDirection.SOUTH) {
                    StructureBoundingBox(x, y, z, x + WIDTH - 1, y + HEIGHT - 1, z + DEPTH - 1)
                } else {
                    StructureBoundingBox(x, y, z, x + DEPTH - 1, y + HEIGHT - 1, z + WIDTH - 1)
                }
        }
    }

    public companion object {
        public const val NAME: String = "swamp_hut"

        /** Twenty-first of the 26 structures of `surface_structures`, in registry order. */
        public const val STRUCTURE_INDEX: Int = 20
        public const val SURFACE_STRUCTURES_STEP: Int = 4

        public val GRID: StructurePlacementGrid =
            StructurePlacementGrid(spacing = 32, separation = 8, salt = 14357620)

        private const val BIOME = "swamp"
        private const val WIDTH = 7
        private const val HEIGHT = 7
        private const val DEPTH = 9
        private const val FLOOR_Y = 64

        private val HORIZONTAL =
            listOf(
                StructureDirection.NORTH,
                StructureDirection.EAST,
                StructureDirection.SOUTH,
                StructureDirection.WEST,
            )

        private val TEMPLATE =
            org.basaltmc.server.world.structure
                .StructureTemplate()

        private fun state(path: String): Int =
            Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: Blocks.AIR.stateId

        private fun stairs(
            facing: String,
            shape: String,
        ): Int =
            Blocks.stateIdOf(
                Identifier.vanilla("spruce_stairs"),
                mapOf("facing" to facing, "half" to "bottom", "shape" to shape, "waterlogged" to "false"),
            ) ?: state("spruce_stairs")

        private val AIR = Blocks.AIR.stateId
        private val SPRUCE_PLANKS = state("spruce_planks")
        private val OAK_LOG = state("oak_log")
        private val OAK_FENCE = state("oak_fence")
        private val POTTED_RED_MUSHROOM = state("potted_red_mushroom")
        private val CRAFTING_TABLE = state("crafting_table")
        private val CAULDRON = state("cauldron")
        private val REPLACEABLE = setOf("water", "lava", "glow_lichen", "seagrass", "tall_seagrass")
    }
}
