package org.basaltmc.server.world.structure

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import kotlin.test.Test

/** The frozen Kotlin as the oracle for every stronghold room's blocks. */
class StrongholdPiecesOracle {
    private fun stoneChunk(): Chunk {
        val chunk = Chunk(0, 0)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in 0..60) chunk.setBlockStateId(x, y, z, Blocks.STONE.stateId)
            }
        }
        return chunk
    }

    private fun fold(chunk: Chunk): Triple<Long, Int, Int> {
        var checksum = -3750763034362895579L
        var written = 0
        for (y in 0..60) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    val id = chunk.getBlockStateId(x, y, z)
                    checksum = (checksum xor id.toLong()) * 1099511628211L
                    if (id != Blocks.STONE.stateId && id != Blocks.AIR.stateId) written++
                }
            }
        }
        return Triple(checksum, written, chunk.blockEntities().size)
    }

    private fun draw(
        brush: StrongholdBrush,
        piece: StrongholdPiece,
    ) {
        when (piece.kind) {
            StrongholdPieceKind.START, StrongholdPieceKind.STAIRS_DOWN -> StrongholdCorridors.stairsDown(brush, piece)
            StrongholdPieceKind.STRAIGHT -> StrongholdCorridors.straight(brush, piece)
            StrongholdPieceKind.STRAIGHT_STAIRS_DOWN -> StrongholdCorridors.straightStairsDown(brush, piece)
            StrongholdPieceKind.LEFT_TURN, StrongholdPieceKind.RIGHT_TURN -> StrongholdCorridors.turn(brush, piece)
            StrongholdPieceKind.CHEST_CORRIDOR -> StrongholdCorridors.chestCorridor(brush, piece)
            StrongholdPieceKind.FILLER_CORRIDOR -> StrongholdCorridors.fillerCorridor(brush, piece)
            StrongholdPieceKind.PRISON_HALL -> StrongholdRooms.prisonHall(brush, piece)
            StrongholdPieceKind.ROOM_CROSSING -> StrongholdRooms.roomCrossing(brush, piece)
            StrongholdPieceKind.FIVE_CROSSING -> StrongholdRooms.fiveCrossing(brush, piece)
            StrongholdPieceKind.LIBRARY -> StrongholdLibrary.place(brush, piece)
            StrongholdPieceKind.PORTAL_ROOM -> StrongholdPortalRoom.place(brush)
        }
    }

    private fun report(
        name: String,
        kind: StrongholdPieceKind,
        orientation: StructureDirection,
        door: StrongholdDoor,
        roomType: Int = 0,
        tall: Boolean = false,
        steps: Int = 0,
        leftChild: Boolean = false,
        rightChild: Boolean = false,
        seed: Long = 777L,
    ) {
        val chunk = stoneChunk()
        val box = StructureBoundingBox(0, 6, 0, 15, 40, 15)
        val clip = StructureBoundingBox(0, 0, 0, 15, 60, 15)
        val fill = StructurePieceFill(box, orientation, chunk, clip)
        val random = WorldgenRandom(LegacyRandom(seed))
        val brush = StrongholdBrush(fill, chunk, random)
        val piece =
            StrongholdPiece(
                kind,
                box,
                orientation,
                depth = 1,
                entryDoor = door,
                roomType = roomType,
                tall = tall,
                steps = steps,
                leftChild = leftChild,
                rightChild = rightChild,
                leftLow = true,
                rightHigh = true,
            )
        draw(brush, piece)
        val (checksum, written, blockEntities) = fold(chunk)
        println(
            "ORACLE $name written=$written blockEntities=$blockEntities " +
                "deferred=${chunk.takeGeneratedBlockWrites().size} checksum=$checksum after=${random.nextInt()}",
        )
    }

    /** The corridor again over eight seeds, folded into one report. */
    private fun reportTorches() {
        var checksum = -3750763034362895579L
        var written = 0
        var draws = 0
        for (seed in 1L..8L) {
            val chunk = stoneChunk()
            val box = StructureBoundingBox(0, 6, 0, 15, 40, 15)
            val clip = StructureBoundingBox(0, 0, 0, 15, 60, 15)
            val fill = StructurePieceFill(box, StructureDirection.EAST, chunk, clip)
            val random = WorldgenRandom(LegacyRandom(seed))
            val brush = StrongholdBrush(fill, chunk, random)
            StrongholdCorridors.straight(
                brush,
                StrongholdPiece(
                    StrongholdPieceKind.STRAIGHT,
                    box,
                    StructureDirection.EAST,
                    depth = 1,
                    entryDoor = StrongholdDoor.GRATES,
                    leftChild = true,
                    rightChild = true,
                ),
            )
            val (chunkChecksum, chunkWritten, _) = fold(chunk)
            checksum = (checksum xor chunkChecksum) * 1099511628211L
            written += chunkWritten
            draws = draws * 31 + random.nextInt(1 shl 20)
        }
        println("ORACLE straightTorches written=$written blockEntities=0 deferred=0 draws=$draws checksum=$checksum")
    }

    @Test
    fun dump() {
        report("stairsDown", StrongholdPieceKind.STAIRS_DOWN, StructureDirection.NORTH, StrongholdDoor.OPENING)
        report(
            "straightStairs",
            StrongholdPieceKind.STRAIGHT_STAIRS_DOWN,
            StructureDirection.SOUTH,
            StrongholdDoor.WOOD_DOOR,
        )
        report(
            "straight",
            StrongholdPieceKind.STRAIGHT,
            StructureDirection.EAST,
            StrongholdDoor.GRATES,
            leftChild = true,
            rightChild = true,
        )
        // Eight seeds folded together for the corridor's four torch rolls: one roll landing in a
        // shifted chance's window is enough, and one seed rarely does.
        reportTorches()
        // The two turns disagree on which side opens, and the facing flips it again.
        report("leftTurnNorth", StrongholdPieceKind.LEFT_TURN, StructureDirection.NORTH, StrongholdDoor.OPENING)
        report("leftTurnWest", StrongholdPieceKind.LEFT_TURN, StructureDirection.WEST, StrongholdDoor.OPENING)
        report("rightTurnNorth", StrongholdPieceKind.RIGHT_TURN, StructureDirection.NORTH, StrongholdDoor.OPENING)
        report("chestCorridor", StrongholdPieceKind.CHEST_CORRIDOR, StructureDirection.NORTH, StrongholdDoor.IRON_DOOR)
        report(
            "filler",
            StrongholdPieceKind.FILLER_CORRIDOR,
            StructureDirection.NORTH,
            StrongholdDoor.OPENING,
            steps = 4,
        )
        report("prisonHall", StrongholdPieceKind.PRISON_HALL, StructureDirection.NORTH, StrongholdDoor.OPENING)
        // The room crossing's five interiors, the last two of which are simply an empty room.
        for (roomType in 0 until 5) {
            report(
                "roomCrossing$roomType",
                StrongholdPieceKind.ROOM_CROSSING,
                StructureDirection.NORTH,
                StrongholdDoor.OPENING,
                roomType = roomType,
            )
        }
        report("fiveCrossing", StrongholdPieceKind.FIVE_CROSSING, StructureDirection.NORTH, StrongholdDoor.OPENING)
        report("libraryShort", StrongholdPieceKind.LIBRARY, StructureDirection.NORTH, StrongholdDoor.OPENING)
        report(
            "libraryTall",
            StrongholdPieceKind.LIBRARY,
            StructureDirection.NORTH,
            StrongholdDoor.OPENING,
            tall = true,
        )
        report("portalRoom", StrongholdPieceKind.PORTAL_ROOM, StructureDirection.NORTH, StrongholdDoor.GRATES)
        // Another seed for the twelve eye draws. Filling the portal takes all twelve, which is one
        // chance in a million million, so that branch is unreachable in practice and by design.
        report(
            "portalRoomOtherSeed",
            StrongholdPieceKind.PORTAL_ROOM,
            StructureDirection.NORTH,
            StrongholdDoor.GRATES,
            seed = 0L,
        )
    }
}
