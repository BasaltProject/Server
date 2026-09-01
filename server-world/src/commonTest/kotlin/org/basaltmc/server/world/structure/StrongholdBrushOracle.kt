package org.basaltmc.server.world.structure

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import kotlin.test.Test

/** The frozen Kotlin as the oracle for the stronghold's drawing half. */
class StrongholdBrushOracle {
    private fun stoneChunk(hollow: Boolean): Chunk {
        val chunk = Chunk(0, 0)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in 0..40) chunk.setBlockStateId(x, y, z, Blocks.STONE.stateId)
                // A cave through the middle, so the shell's skip-air rule and its skipped draw show.
                if (hollow) for (y in 12..16) chunk.setBlockStateId(x, y, z, Blocks.AIR.stateId)
            }
        }
        return chunk
    }

    private fun fold(chunk: Chunk): Triple<Long, Int, Int> {
        var checksum = -3750763034362895579L
        var written = 0
        for (y in 0..40) {
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

    private fun report(
        name: String,
        door: StrongholdDoor,
        hollow: Boolean,
        skipAir: Boolean,
    ) {
        val chunk = stoneChunk(hollow)
        val box = StructureBoundingBox(1, 8, 1, 13, 20, 13)
        val clip = StructureBoundingBox(0, 0, 0, 15, 40, 15)
        val fill = StructurePieceFill(box, StructureDirection.NORTH, chunk, clip)
        val random = WorldgenRandom(LegacyRandom(20250831L))
        val brush = StrongholdBrush(fill, chunk, random)
        brush.shell(0, 0, 0, 8, 8, 8, skipAir = skipAir)
        brush.door(door, 2, 1, 0)
        brush.box(3, 2, 3, 5, 4, 5, StrongholdPalette.COBBLESTONE)
        brush.maybe(0.5f, 4, 5, 4, StrongholdPalette.SMOOTH_STONE_SLAB)
        brush.maybe(0.5f, 4, 6, 4, StrongholdPalette.STONE_BRICK_SLAB)
        brush.createChest(4, 1, 4, "minecraft:chests/stronghold_corridor")
        // Against a wall and then into a corner: one solid neighbour turns the chest, two leave it
        // facing north.
        brush.createChest(1, 1, 4, "minecraft:chests/stronghold_corridor")
        brush.createChest(1, 1, 1, "minecraft:chests/stronghold_corridor")
        brush.silverfishSpawner(6, 1, 6)
        val (checksum, written, blockEntities) = fold(chunk)
        println(
            "ORACLE $name written=$written blockEntities=$blockEntities " +
                "deferred=${chunk.takeGeneratedBlockWrites().size} checksum=$checksum after=${random.nextInt()}",
        )
    }

    @Test
    fun dump() {
        report("opening", StrongholdDoor.OPENING, hollow = false, skipAir = true)
        report("woodDoor", StrongholdDoor.WOOD_DOOR, hollow = false, skipAir = true)
        report("grates", StrongholdDoor.GRATES, hollow = false, skipAir = true)
        report("ironDoor", StrongholdDoor.IRON_DOOR, hollow = false, skipAir = true)
        // A cave in the way: the shell leaves those blocks alone and draws nothing for them.
        report("hollow", StrongholdDoor.OPENING, hollow = true, skipAir = true)
        // The same cave with the skip turned off, which both fills it and spends the draws.
        report("hollowFilled", StrongholdDoor.OPENING, hollow = true, skipAir = false)
    }
}
