package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.wire.common.Identifier
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertTrue

class StandaloneFeaturesTest {
    @Test
    fun vinesUseTheCeilingBeforeHorizontalSupports() {
        val chunk = Chunk(0, 0)
        chunk.setBlockStateId(8, 41, 8, Blocks.STONE.stateId)
        chunk.setBlockStateId(8, 40, 7, Blocks.STONE.stateId)

        assertTrue(StandaloneFeatures.placeAt(chunk, random(), ORIGIN, "classic_vines_cave_feature"))

        val vine = Blocks.byStateId(chunk.getBlockStateId(8, 40, 8)) ?: error("missing vine")
        assertEquals("vine", vine.block.path)
        assertEquals("true", vine.properties["up"])
        assertEquals("false", vine.properties["north"])
    }

    @Test
    fun closedNetherSpringSchedulesItsImmediateLavaTick() {
        val chunk = Chunk(0, 0)
        val netherrack = state("netherrack")
        val origin = DecorationPos(8, 40, 8)
        chunk.setBlockStateId(8, 40, 8, netherrack)
        chunk.setBlockStateId(8, 41, 8, netherrack)
        chunk.setBlockStateId(8, 39, 8, netherrack)
        chunk.setBlockStateId(7, 40, 8, netherrack)
        chunk.setBlockStateId(9, 40, 8, netherrack)
        chunk.setBlockStateId(8, 40, 7, netherrack)
        chunk.setBlockStateId(8, 40, 9, netherrack)

        assertTrue(StandaloneFeatures.placeAt(chunk, random(), origin, "spring_closed"))

        assertEquals(Blocks.LAVA.stateId, chunk.getBlockStateId(8, 40, 8))
        val tick = chunk.scheduledFluidTicks().single()
        assertEquals(Identifier.vanilla("lava"), tick.type)
        assertEquals(8, tick.localX)
        assertEquals(40, tick.y)
        assertEquals(8, tick.localZ)
        assertEquals(0, tick.delay)
    }

    @Test
    fun voidStartPlatformUsesItsFixedWorldCentreAndYOffset() {
        val chunk = Chunk(0, 0)

        assertTrue(StandaloneFeatures.placeAt(chunk, random(), ORIGIN, "void_start_platform"))

        for (x in 0 until 16) {
            for (z in 0 until 16) {
                val expected = if (x == 8 && z == 8) "cobblestone" else "stone"
                assertEquals(expected, path(chunk.getBlockStateId(x, 43, z)))
                assertEquals("air", path(chunk.getBlockStateId(x, 40, z)))
            }
        }

        val distant = Chunk(2, 0)
        assertTrue(
            StandaloneFeatures.placeAt(distant, random(), DecorationPos(32, 40, 0), "void_start_platform"),
        )
        for (x in 0 until 16) {
            for (z in 0 until 16) assertEquals("air", path(distant.getBlockStateId(x, 43, z)))
        }
    }

    @Test
    fun monsterRoomMatchesTheOfficialBodyAndPopulatesBlockEntities() {
        val chunk = monsterRoomChunk()

        assertTrue(StandaloneFeatures.placeAt(chunk, random(), ORIGIN, "monster_room"))

        assertEquals("spawner", path(chunk.getBlockStateId(8, 40, 8)))
        // Official 26.2 MonsterRoomFeature oracle for this seed and stone volume.
        assertEquals(140, countBetween(chunk, "cave_air", 4..12, 39..44, 4..12))
        assertEquals(131, countBetween(chunk, "cobblestone", 4..12, 39..44, 4..12))
        assertEquals(41, countBetween(chunk, "mossy_cobblestone", 4..12, 39..44, 4..12))

        val spawner = chunk.blockEntityAt(8, 40, 8) ?: error("missing dungeon spawner")
        assertEquals(Identifier.vanilla("mob_spawner"), spawner.type)
        val mob =
            spawner.data
                .getCompound("SpawnData")
                .getCompound("entity")
                .getString("id")
        assertEquals("minecraft:skeleton", mob)
        val chests = chunk.blockEntities().filter { it.type == Identifier.vanilla("chest") }
        assertEquals(1, chests.size)
        val chest = chests.single()
        assertEquals(10, chest.localX)
        assertEquals(40, chest.y)
        assertEquals(8, chest.localZ)
        assertEquals("minecraft:chests/simple_dungeon", chest.data.getString("LootTable"))
        assertEquals(-7653194288055676639L, chest.data.getLong("LootTableSeed"))
    }

    @Test
    fun monsterRoomCarvePreservesFeaturesCannotReplace() {
        val chunk = monsterRoomChunk()
        chunk.setBlockStateId(8, 41, 8, state("bedrock"))

        assertTrue(StandaloneFeatures.placeAt(chunk, random(), ORIGIN, "monster_room"))

        assertEquals("bedrock", path(chunk.getBlockStateId(8, 41, 8)))
    }

    @Test
    fun returnGatewayUsesTheSparseThreeByFiveBedrockCross() {
        val chunk = Chunk(0, 0)
        for (dx in -1..1) {
            for (dz in -1..1) {
                for (dy in -2..2) chunk.setBlockStateId(8 + dx, 40 + dy, 8 + dz, Blocks.STONE.stateId)
            }
        }

        assertTrue(StandaloneFeatures.placeAt(chunk, random(), ORIGIN, "end_gateway_return"))

        var bedrock = 0
        var air = 0
        for (dx in -1..1) {
            for (dz in -1..1) {
                for (dy in -2..2) {
                    val expected =
                        when {
                            dx == 0 && dy == 0 && dz == 0 -> "end_gateway"
                            dy == 0 -> "air"
                            kotlin.math.abs(dy) == 2 && dx == 0 && dz == 0 -> "bedrock"
                            (dx == 0 || dz == 0) && kotlin.math.abs(dy) != 2 -> "bedrock"
                            else -> "air"
                        }
                    val actual = path(chunk.getBlockStateId(8 + dx, 40 + dy, 8 + dz))
                    assertEquals(expected, actual, "offset ($dx,$dy,$dz)")
                    if (actual == "bedrock") bedrock++
                    if (actual == "air") air++
                }
            }
        }
        assertEquals(12, bedrock)
        assertEquals(32, air)
        val blockEntity = chunk.blockEntityAt(8, 40, 8) ?: error("missing end gateway block entity")
        assertEquals(Identifier.vanilla("end_gateway"), blockEntity.type)
        assertEquals("minecraft:end_gateway", blockEntity.data.getString("id"))
        assertEquals(0L, blockEntity.data.getLong("Age"))
        assertTrue(blockEntity.data.getIntArray("exit_portal").contentEquals(intArrayOf(100, 50, 0)))
        assertTrue(blockEntity.data.getBoolean("ExactTeleport"))
    }

    @Test
    fun lavaLakeUsesTheCompleteSixteenByEightEllipsoidMask() {
        val chunk = filledChunk(state("dirt"))

        assertTrue(StandaloneFeatures.placeAt(chunk, random(), ORIGIN, "lake_lava_underground"))

        // Official 26.2 LakeFeature oracle: Xoroshiro seed 1234 over this exact dirt volume.
        assertEquals(54, count(chunk, "lava"))
        assertEquals(83, count(chunk, "cave_air"))
        assertTrue(count(chunk, "stone") > 0)
        assertEquals(0, count(chunk, "air"))
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in BASE_Y until BASE_Y + 8) {
                    when (path(chunk.getBlockStateId(x, y, z))) {
                        "lava" -> assertTrue(y < ORIGIN.y)
                        "cave_air" -> assertTrue(y >= ORIGIN.y)
                    }
                }
            }
        }
    }

    @Test
    fun lakeBodyAndBarrierRespectTheirReplacementTags() {
        val protected = filledChunk(state("bedrock"))
        assertTrue(StandaloneFeatures.placeAt(protected, random(), ORIGIN, "lake_lava_surface"))
        assertEquals(16 * 16 * 8, count(protected, "bedrock"))
        assertEquals(0, count(protected, "lava"))
        assertEquals(0, count(protected, "stone"))

        val logs = filledChunk(state("oak_log"))
        assertTrue(StandaloneFeatures.placeAt(logs, random(), ORIGIN, "lake_lava_surface"))
        assertTrue(count(logs, "lava") > 0)
        assertTrue(count(logs, "cave_air") > 0)
        assertEquals(0, count(logs, "stone"))
    }

    @Test
    fun sulfurPoolUsesWaterAndFinishesWithOneWetVent() {
        val chunk = filledChunk(state("sulfur"))

        assertTrue(StandaloneFeatures.placeAt(chunk, random(), ORIGIN, "sulfur_pool"))

        assertTrue(count(chunk, "water") > 0)
        assertTrue(count(chunk, "cave_air") > 0)
        assertEquals(0, count(chunk, "lava"))
        val vents = positions(chunk, "potent_sulfur")
        assertEquals(1, vents.size)
        val vent = vents.single()
        val ventState = Blocks.byStateId(chunk.getBlockStateId(vent.x, vent.y, vent.z))
        assertEquals("wet", ventState?.properties?.get("potent_sulfur_state"))
        assertTrue(hasSourceWater(chunk.getBlockStateId(vent.x, vent.y + 1, vent.z)))
        assertTrue(vent.y in ORIGIN.y - 4..ORIGIN.y)
    }

    @Test
    fun sulfurSpikeOnTheBoundaryRejectsTheWholePoolBeforeCarving() {
        val chunk = filledChunk(state("sulfur_spike"))

        assertFalse(StandaloneFeatures.placeAt(chunk, random(), ORIGIN, "sulfur_pool"))

        assertEquals(16 * 16 * 8, count(chunk, "sulfur_spike"))
        assertEquals(0, count(chunk, "water"))
        assertEquals(0, count(chunk, "cave_air"))
    }

    private fun filledChunk(stateId: Int): Chunk =
        Chunk(0, 0).apply {
            for (x in 0 until 16) {
                for (z in 0 until 16) {
                    for (y in BASE_Y until BASE_Y + 8) setBlockStateId(x, y, z, stateId)
                }
            }
        }

    private fun monsterRoomChunk(): Chunk =
        Chunk(0, 0).apply {
            for (x in 4..12) {
                for (z in 4..12) {
                    for (y in 38..44) setBlockStateId(x, y, z, Blocks.STONE.stateId)
                }
            }
            // Seed 1234 chooses either x radius 2 or 3; leave one valid opening for both.
            for (x in 4..5) {
                setBlockStateId(x, 40, 8, Blocks.AIR.stateId)
                setBlockStateId(x, 41, 8, Blocks.AIR.stateId)
            }
        }

    private fun count(
        chunk: Chunk,
        blockPath: String,
    ): Int = positions(chunk, blockPath).size

    private fun countBetween(
        chunk: Chunk,
        blockPath: String,
        xRange: IntRange,
        yRange: IntRange,
        zRange: IntRange,
    ): Int {
        var count = 0
        for (x in xRange) {
            for (z in zRange) {
                for (y in yRange) if (path(chunk.getBlockStateId(x, y, z)) == blockPath) count++
            }
        }
        return count
    }

    private fun positions(
        chunk: Chunk,
        blockPath: String,
    ): List<DecorationPos> =
        buildList {
            for (x in 0 until 16) {
                for (z in 0 until 16) {
                    for (y in BASE_Y until BASE_Y + 8) {
                        if (path(chunk.getBlockStateId(x, y, z)) == blockPath) add(DecorationPos(x, y, z))
                    }
                }
            }
        }

    private fun hasSourceWater(stateId: Int): Boolean =
        Blocks.hasWaterFluid(stateId) && stateId !in Blocks.WATER.stateId + 1..Blocks.WATER.stateId + 15

    private fun path(stateId: Int): String? = Blocks.byStateId(stateId)?.block?.path

    private fun random(): WorldgenRandom = WorldgenRandom(XoroshiroRandom(1234L))

    private fun state(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")

    private companion object {
        const val BASE_Y = 36
        val ORIGIN = DecorationPos(8, 40, 8)
    }
}
