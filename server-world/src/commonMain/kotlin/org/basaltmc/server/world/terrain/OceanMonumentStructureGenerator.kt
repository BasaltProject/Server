package org.basaltmc.server.world.terrain

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.structure.MonumentBrush
import org.basaltmc.server.world.structure.MonumentDoubleRooms
import org.basaltmc.server.world.structure.MonumentRoomKind
import org.basaltmc.server.world.structure.MonumentRoomPiece
import org.basaltmc.server.world.structure.MonumentShell
import org.basaltmc.server.world.structure.MonumentSingleRooms
import org.basaltmc.server.world.structure.MonumentWings
import org.basaltmc.server.world.structure.StructureBoundingBox
import org.basaltmc.server.world.structure.StructureDirection
import org.basaltmc.server.world.structure.StructurePieceFill
import org.basaltmc.server.world.structure.StructurePlacementGrid
import org.basaltmc.server.world.structure.StructurePos
import org.basaltmc.server.world.structure.StructureStartRecord
import org.basaltmc.server.world.structure.fitMonumentRooms
import org.basaltmc.server.world.structure.generateMonumentRoomGraph
import kotlin.math.abs

/**
 * The ocean monument. // parity: OceanMonumentStructure. Its base is always at y 39 whatever the sea
 * bed does, so unlike every other structure here nothing is measured off the ground: the height is
 * only consulted to decide whether the monument may stand at all.
 */
public class OceanMonumentStructureGenerator(
    private val noiseBiomeAt: (quartX: Int, quartY: Int, quartZ: Int) -> String,
    private val monumentBiomes: Set<String>,
    private val surroundingBiomes: Set<String>,
    private val oceanFloorAt: (x: Int, z: Int) -> Int,
    private val seaLevel: Int = 63,
) {
    public fun generateInto(
        chunk: Chunk,
        seed: Long,
    ) {
        val cellX = chunk.chunkX.floorDiv(GRID.spacing)
        val cellZ = chunk.chunkZ.floorDiv(GRID.spacing)
        for (offsetX in -1..1) {
            for (offsetZ in -1..1) {
                val (candidateX, candidateZ) =
                    GRID.candidateChunk(seed, (cellX + offsetX) * GRID.spacing, (cellZ + offsetZ) * GRID.spacing)
                if (abs(candidateX - chunk.chunkX) > REACH_CHUNKS || abs(candidateZ - chunk.chunkZ) > REACH_CHUNKS) {
                    continue
                }
                placeFrom(chunk, seed, candidateX, candidateZ)
            }
        }
    }

    private fun placeFrom(
        chunk: Chunk,
        seed: Long,
        chunkX: Int,
        chunkZ: Int,
    ) {
        if (!GRID.isStructureChunk(seed, chunkX, chunkZ)) return
        if (!surroundingIsOcean(chunkX, chunkZ)) return

        // parity: onTopOfChunkCenter over `OCEAN_FLOOR_WG`, which only decides where the biome is
        // sampled: the monument itself never moves off y 39.
        val centreX = chunkX * 16 + 8
        val centreZ = chunkZ * 16 + 8
        val groundY = oceanFloorAt(centreX, centreZ) - 1
        if (noiseBiomeAt(centreX shr 2, groundY shr 2, centreZ shr 2) !in monumentBiomes) return

        val startRandom = WorldgenRandom(LegacyRandom(0L))
        startRandom.setLargeFeatureSeed(seed, chunkX, chunkZ)
        val orientation = HORIZONTAL[startRandom.nextInt(HORIZONTAL.size)]
        val graph = generateMonumentRoomGraph(startRandom)
        val rooms = fitMonumentRooms(graph, startRandom)

        val west = chunkX * 16 - 29
        val north = chunkZ * 16 - 29
        val shellBox = StructureBoundingBox(west, BASE_Y, north, west + 57, BASE_Y + 22, north + 57)
        val clip = chunkClip(chunk.chunkX, chunk.chunkZ)
        if (!shellBox.intersects(clip)) return

        val random = WorldgenRandom(XoroshiroRandom(seed))
        val decorationSeed = random.setDecorationSeed(seed, chunk.chunkX * 16, chunk.chunkZ * 16)
        random.setFeatureSeed(decorationSeed, STRUCTURE_INDEX, SURFACE_STRUCTURES_STEP)

        val here = mutableListOf<StructureBoundingBox>()
        val shellFill = StructurePieceFill(shellBox, orientation, chunk, clip)
        MonumentShell(MonumentBrush(shellFill, seaLevel, clip), seaLevel).place()
        here += shellBox

        // parity: the children are placed in the order MonumentBuilding built them, and a room only
        // draws where its own box reaches this chunk, so a room's draws happen once per chunk it
        // touches. The offset is the building's own (9, 0, 22) turned by the monument's facing.
        val offset = StructurePos(shellFill.worldX(9, 22), shellFill.worldY(0), shellFill.worldZ(9, 22))
        for (piece in rooms) {
            val box = boxOf(piece, orientation, shellFill, offset) ?: continue
            if (!box.intersects(clip)) continue
            here += box
            val brush = MonumentBrush(StructurePieceFill(box, orientation, chunk, clip), seaLevel, clip)
            draw(piece, brush, random)
        }
        chunk.structures.add(StructureStartRecord(NAME, chunkX, chunkZ, shellBox, here))
    }

    private fun draw(
        piece: MonumentRoomPiece,
        brush: MonumentBrush,
        random: WorldgenRandom,
    ) {
        val room = piece.room
        when (piece.kind) {
            MonumentRoomKind.ENTRY -> room?.let { MonumentSingleRooms.entry(brush, it) }
            MonumentRoomKind.CORE -> MonumentSingleRooms.core(brush)
            MonumentRoomKind.SIMPLE -> room?.let { MonumentSingleRooms.simple(brush, it, piece.design, random) }
            MonumentRoomKind.SIMPLE_TOP -> room?.let { MonumentSingleRooms.simpleTop(brush, it, random) }
            MonumentRoomKind.DOUBLE_X -> room?.let { MonumentDoubleRooms.doubleX(brush, it) }
            MonumentRoomKind.DOUBLE_XY -> room?.let { MonumentDoubleRooms.doubleXy(brush, it) }
            MonumentRoomKind.DOUBLE_Y -> room?.let { MonumentDoubleRooms.doubleY(brush, it) }
            MonumentRoomKind.DOUBLE_YZ -> room?.let { MonumentDoubleRooms.doubleYz(brush, it) }
            MonumentRoomKind.DOUBLE_Z -> room?.let { MonumentDoubleRooms.doubleZ(brush, it) }
            MonumentRoomKind.LEFT_WING, MonumentRoomKind.RIGHT_WING -> MonumentWings.wing(brush, piece.design)
            MonumentRoomKind.PENTHOUSE -> MonumentWings.penthouse(brush)
        }
    }

    /**
     * // parity: OceanMonumentPiece.makeBoundingBox. A room's box is its lattice cell scaled by eight
     * and four, laid out along the monument's facing and then moved by the building's own offset.
     * The wings and the penthouse are not lattice cells: their corners are named outright.
     */
    private fun boxOf(
        piece: MonumentRoomPiece,
        orientation: StructureDirection,
        shell: StructurePieceFill,
        offset: StructurePos,
    ): StructureBoundingBox? {
        fun corners(
            x0: Int,
            y0: Int,
            z0: Int,
            x1: Int,
            y1: Int,
            z1: Int,
        ) = StructureBoundingBox.fromCorners(
            StructurePos(shell.worldX(x0, z0), shell.worldY(y0), shell.worldZ(x0, z0)),
            StructurePos(shell.worldX(x1, z1), shell.worldY(y1), shell.worldZ(x1, z1)),
        )
        when (piece.kind) {
            MonumentRoomKind.LEFT_WING -> return corners(1, 1, 1, 23, 8, 21)
            MonumentRoomKind.RIGHT_WING -> return corners(34, 1, 1, 56, 8, 21)
            MonumentRoomKind.PENTHOUSE -> return corners(22, 13, 22, 35, 17, 35)
            else -> Unit
        }
        val room = piece.room ?: return null
        val (roomWidth, roomHeight, roomDepth) = SIZES.getValue(piece.kind)
        val roomX = room.index % 5
        val roomZ = room.index / 5 % 5
        val roomY = room.index / 25
        val width = roomWidth * 8
        val height = roomHeight * 4
        val depth = roomDepth * 8
        val base =
            if (orientation == StructureDirection.NORTH || orientation == StructureDirection.SOUTH) {
                StructureBoundingBox(0, 0, 0, width - 1, height - 1, depth - 1)
            } else {
                StructureBoundingBox(0, 0, 0, depth - 1, height - 1, width - 1)
            }
        val step =
            when (orientation) {
                StructureDirection.NORTH -> StructurePos(roomX * 8, roomY * 4, -(roomZ + roomDepth) * 8 + 1)
                StructureDirection.SOUTH -> StructurePos(roomX * 8, roomY * 4, roomZ * 8)
                StructureDirection.WEST -> StructurePos(-(roomZ + roomDepth) * 8 + 1, roomY * 4, roomX * 8)
                else -> StructurePos(roomZ * 8, roomY * 4, roomX * 8)
            }
        return base.move(step).move(offset)
    }

    /**
     * // parity: the radius-29 sweep around the ninth block of the chunk. It is a cube of biome
     * cells, not a circle, and one cell outside the tag is enough to refuse the monument.
     */
    private fun surroundingIsOcean(
        chunkX: Int,
        chunkZ: Int,
    ): Boolean {
        val x = chunkX * 16 + 9
        val z = chunkZ * 16 + 9
        for (quartX in ((x - RADIUS) shr 2)..((x + RADIUS) shr 2)) {
            for (quartY in ((seaLevel - RADIUS) shr 2)..((seaLevel + RADIUS) shr 2)) {
                for (quartZ in ((z - RADIUS) shr 2)..((z + RADIUS) shr 2)) {
                    if (noiseBiomeAt(quartX, quartY, quartZ) !in surroundingBiomes) return false
                }
            }
        }
        return true
    }

    private fun chunkClip(
        chunkX: Int,
        chunkZ: Int,
    ): StructureBoundingBox =
        StructureBoundingBox(
            chunkX * 16,
            WorldHeight.MIN_Y,
            chunkZ * 16,
            chunkX * 16 + 15,
            WorldHeight.MAX_Y,
            chunkZ * 16 + 15,
        )

    public companion object {
        public const val NAME: String = "monument"

        /** Seventh of the 26 structures of `surface_structures`, in registry order. */
        public const val STRUCTURE_INDEX: Int = 6

        private const val SURFACE_STRUCTURES_STEP = 4

        /** The monument is 58 blocks wide and starts 29 west of its chunk, so it reaches two chunks out. */
        private const val REACH_CHUNKS = 2
        private const val RADIUS = 29

        /** // parity: makeBoundingBox(west, 39, north, ...); the roof lands on y 61 whatever the sea bed does. */
        private const val BASE_Y = 39

        public val GRID: StructurePlacementGrid =
            StructurePlacementGrid(
                spacing = 32,
                separation = 5,
                salt = 10387313,
                spreadType = StructurePlacementGrid.SpreadType.TRIANGULAR,
            )

        private val HORIZONTAL =
            listOf(
                StructureDirection.NORTH,
                StructureDirection.EAST,
                StructureDirection.SOUTH,
                StructureDirection.WEST,
            )

        /** Each room kind's size in lattice cells, as its piece constructor names it. */
        private val SIZES: Map<MonumentRoomKind, Triple<Int, Int, Int>> =
            mapOf(
                MonumentRoomKind.ENTRY to Triple(1, 1, 1),
                MonumentRoomKind.CORE to Triple(2, 2, 2),
                MonumentRoomKind.SIMPLE to Triple(1, 1, 1),
                MonumentRoomKind.SIMPLE_TOP to Triple(1, 1, 1),
                MonumentRoomKind.DOUBLE_X to Triple(2, 1, 1),
                MonumentRoomKind.DOUBLE_XY to Triple(2, 2, 1),
                MonumentRoomKind.DOUBLE_Y to Triple(1, 2, 1),
                MonumentRoomKind.DOUBLE_YZ to Triple(1, 2, 2),
                MonumentRoomKind.DOUBLE_Z to Triple(1, 1, 2),
            )
    }
}
