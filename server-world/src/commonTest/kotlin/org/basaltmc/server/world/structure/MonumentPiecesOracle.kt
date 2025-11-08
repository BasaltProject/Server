package org.basaltmc.server.world.structure

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import kotlin.test.Test

/** The frozen Kotlin as the oracle for the monument's blocks. */
class MonumentPiecesOracle {
    private val packedIce =
        Blocks.REGISTRY
            .byIdentifier(
                org.basaltmc.wire.common.Identifier
                    .vanilla("packed_ice"),
            )?.stateId
            ?: error("unknown block 'packed_ice'")

    /**
     * Deep water on a single course of stone: the pillars then run all the way to the world's floor
     * and meet the guard that stops them, and the packed ice gives the water box something it is
     * not allowed to touch.
     */
    private fun seaChunk(): Chunk {
        val chunk = Chunk(0, 0)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                chunk.setBlockStateId(x, -64, z, Blocks.STONE.stateId)
                for (y in -63..62) chunk.setBlockStateId(x, y, z, Blocks.WATER.stateId)
                for (y in 55..56) chunk.setBlockStateId(x, y, z, packedIce)
                // A shelf of stone above sea level, so how far the shell floods is observable.
                for (y in 60..62) chunk.setBlockStateId(x, y, z, Blocks.STONE.stateId)
            }
        }
        return chunk
    }

    private fun fold(chunk: Chunk): Triple<Long, Int, Int> {
        var checksum = -3750763034362895579L
        var written = 0
        for (y in -64..70) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    val id = chunk.getBlockStateId(x, y, z)
                    checksum = (checksum xor id.toLong()) * 1099511628211L
                    if (id != Blocks.STONE.stateId &&
                        id != Blocks.WATER.stateId &&
                        id != Blocks.AIR.stateId
                    ) {
                        written++
                    }
                }
            }
        }
        return Triple(checksum, written, chunk.generatedEntities().size)
    }

    private fun brushOver(
        chunk: Chunk,
        origin: StructureBoundingBox,
    ): MonumentBrush {
        // A sea level below the vanilla floor of 64, so the shell's own `max` is observable.
        val clip = StructureBoundingBox(0, -64, 0, 15, 70, 15)
        return MonumentBrush(StructurePieceFill(origin, StructureDirection.NORTH, chunk, clip), 40, clip)
    }

    private fun report(
        name: String,
        origin: StructureBoundingBox,
        seed: Long = 4711L,
        draw: (MonumentBrush, WorldgenRandom) -> Unit,
    ) {
        val chunk = seaChunk()
        val brush = brushOver(chunk, origin)
        val random = WorldgenRandom(LegacyRandom(seed))
        draw(brush, random)
        val (checksum, written, entities) = fold(chunk)
        println(
            "ORACLE $name written=$written entities=$entities deferred=${chunk.takeGeneratedBlockWrites().size} " +
                "checksum=$checksum after=${random.nextInt()}",
        )
    }

    /** A lattice room to draw against: the graph's own rooms, so the doorways are the real ones. */
    private fun latticeRoom(
        seed: Long,
        pick: (List<MonumentRoomPiece>) -> MonumentRoomPiece?,
    ): Pair<MonumentRoomPiece, MonumentRoomGraph> {
        val random = WorldgenRandom(LegacyRandom(seed))
        val graph = generateMonumentRoomGraph(random)
        val fitted = fitMonumentRooms(graph, random)
        return (pick(fitted) ?: fitted.first()) to graph
    }

    private fun roomOf(kind: MonumentRoomKind): MonumentRoomPiece {
        for (seed in 1L..64L) {
            val (piece, _) = latticeRoom(seed) { list -> list.firstOrNull { it.kind == kind } }
            if (piece.kind == kind) return piece
        }
        error("no $kind room in the first 64 seeds")
    }

    private val roomBox = StructureBoundingBox(0, 20, 0, 15, 40, 15)

    @Test
    fun dump() {
        // Placed so the one pillar the grid skips would fall inside this chunk if it were not.
        report("shell", StructureBoundingBox(-20, 25, -47, 37, 48, 10)) { brush, _ ->
            MonumentShell(brush, 40).place()
        }
        report("entry", roomBox) { brush, _ -> MonumentSingleRooms.entry(brush, roomOf(MonumentRoomKind.ENTRY).room!!) }
        report("core", roomBox) { brush, _ -> MonumentSingleRooms.core(brush) }
        for (design in 0 until 3) {
            report("simple$design", roomBox) { brush, random ->
                MonumentSingleRooms.simple(brush, roomOf(MonumentRoomKind.SIMPLE).room!!, design, random)
            }
        }
        report("simpleTop", roomBox) { brush, random ->
            MonumentSingleRooms.simpleTop(brush, roomOf(MonumentRoomKind.SIMPLE_TOP).room!!, random)
        }
        report("doubleX", roomBox) { brush, _ ->
            MonumentDoubleRooms.doubleX(brush, roomOf(MonumentRoomKind.DOUBLE_X).room!!)
        }
        report("doubleXy", roomBox) { brush, _ ->
            MonumentDoubleRooms.doubleXy(brush, roomOf(MonumentRoomKind.DOUBLE_XY).room!!)
        }
        report("doubleY", roomBox) { brush, _ ->
            MonumentDoubleRooms.doubleY(brush, roomOf(MonumentRoomKind.DOUBLE_Y).room!!)
        }
        report("doubleYz", roomBox) { brush, _ ->
            MonumentDoubleRooms.doubleYz(brush, roomOf(MonumentRoomKind.DOUBLE_YZ).room!!)
        }
        report("doubleZ", roomBox) { brush, _ ->
            MonumentDoubleRooms.doubleZ(brush, roomOf(MonumentRoomKind.DOUBLE_Z).room!!)
        }
        report("wingHall", roomBox) { brush, _ -> MonumentWings.wing(brush, 0) }
        report("wingGallery", roomBox) { brush, _ -> MonumentWings.wing(brush, 1) }
        report("penthouse", roomBox) { brush, _ -> MonumentWings.penthouse(brush) }
    }
}
