package org.basaltmc.registry

import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlin.test.assertTrue

class BlocksTest {
    @Test
    fun lavaHasANonEmptyFluidState() {
        assertTrue(Blocks.hasFluid(Blocks.LAVA.stateId))
        assertFalse(Blocks.hasWaterFluid(Blocks.LAVA.stateId))
    }

    @Test
    fun allThreeVanillaAirBlocksAreAir() {
        assertTrue(Blocks.isAir(Blocks.AIR.stateId))
        assertTrue(Blocks.isAir(Blocks.VOID_AIR.stateId))
        assertTrue(Blocks.isAir(Blocks.CAVE_AIR.stateId))
        assertFalse(Blocks.isAir(Blocks.STONE.stateId))
        assertEquals(15292, Blocks.VOID_AIR.stateId)
        assertEquals(15293, Blocks.CAVE_AIR.stateId)
    }

    @Test
    fun exposesTheExpectedStateIds() {
        assertEquals(0, Blocks.AIR.stateId)
        assertEquals(1, Blocks.STONE.stateId)
        assertEquals(9, Blocks.GRASS_BLOCK.stateId)
        assertEquals(10, Blocks.DIRT.stateId)
        assertEquals(85, Blocks.BEDROCK.stateId)
    }

    @Test
    fun blocksAreFoundByIdentifierInTheSharedRegistry() {
        assertEquals(Blocks.STONE, Blocks.REGISTRY.byIdentifier(Identifier.vanilla("stone")))
        assertEquals(Blocks.STONE, Blocks.REGISTRY.byId(1))
    }

    @Test
    fun leafStatesExposeExactPropertiesAndDistanceUpdates() {
        val defaultOakLeaves = Blocks.OAK_LEAVES.stateId
        val distanceOneWaterlogged = defaultOakLeaves - 27
        val distanceThreeNaturalDry = defaultOakLeaves - 16

        assertTrue(Blocks.isLeavesState(distanceOneWaterlogged))
        assertEquals(1, Blocks.leafDistance(distanceOneWaterlogged))
        assertTrue(Blocks.hasWaterFluid(distanceOneWaterlogged))
        assertEquals(
            linkedMapOf("distance" to "3", "persistent" to "false", "waterlogged" to "false"),
            Blocks.byStateId(distanceThreeNaturalDry)?.properties,
        )
        assertEquals(distanceThreeNaturalDry, Blocks.withLeafDistance(defaultOakLeaves, 3))
        assertFalse(Blocks.hasWaterFluid(distanceThreeNaturalDry))
        assertEquals(null, Blocks.leafDistance(Blocks.STONE.stateId))
    }

    @Test
    fun worldgenPlantStatesExposeTheirExactProperties() {
        val sunflower = stateId("sunflower")
        assertEquals(mapOf("half" to "lower"), Blocks.byStateId(sunflower)?.properties)
        assertEquals(mapOf("half" to "upper"), Blocks.byStateId(sunflower - 1)?.properties)
        assertEquals(sunflower - 1, Blocks.upperDoublePlantStateId(sunflower))

        val wildflowers = stateId("wildflowers")
        assertEquals(
            linkedMapOf("facing" to "east", "flower_amount" to "4"),
            Blocks.byStateId(wildflowers + 15)?.properties,
        )
        val pinkPetals = stateId("pink_petals")
        assertTrue(Blocks.isPinkPetalsState(pinkPetals + 9))

        assertEquals(mapOf("age" to "23"), Blocks.byStateId(Blocks.KELP.stateId + 23)?.properties)
        assertTrue(Blocks.hasWaterFluid(Blocks.KELP.stateId + 23))
        for (path in listOf("weeping_vines", "twisting_vines")) {
            val defaultStateId = stateId(path)
            assertEquals(defaultStateId + 23, Blocks.growingVineHeadStateId(defaultStateId, 23))
            assertEquals(mapOf("age" to "23"), Blocks.byStateId(defaultStateId + 23)?.properties)
        }
        assertEquals(
            linkedMapOf("pickles" to "4", "waterlogged" to "true"),
            Blocks.byStateId(Blocks.SEA_PICKLE.stateId + 6)?.properties,
        )
        assertTrue(Blocks.hasWaterFluid(Blocks.SEA_PICKLE.stateId + 6))
        assertFalse(Blocks.hasWaterFluid(Blocks.SEA_PICKLE.stateId + 7))
    }

    @Test
    fun snowySurfaceBlocksExposeBothExactStates() {
        for (path in listOf("grass_block", "podzol", "mycelium")) {
            val bareStateId = stateId(path)
            val snowyStateId = bareStateId - 1

            assertEquals(mapOf("snowy" to "false"), Blocks.byStateId(bareStateId)?.properties)
            assertEquals(mapOf("snowy" to "true"), Blocks.byStateId(snowyStateId)?.properties)
            assertEquals(snowyStateId, Blocks.withSnowy(bareStateId, true))
            assertEquals(bareStateId, Blocks.withSnowy(snowyStateId, false))
        }
        assertEquals(null, Blocks.withSnowy(Blocks.DIRT.stateId, true))
    }

    @Test
    fun coralAndHugeMushroomStatesExposeExactProperties() {
        val tubeCoral = stateId("tube_coral")
        assertEquals(mapOf("waterlogged" to "true"), Blocks.byStateId(tubeCoral)?.properties)
        assertEquals(mapOf("waterlogged" to "false"), Blocks.byStateId(tubeCoral + 1)?.properties)
        assertTrue(Blocks.hasWaterFluid(tubeCoral))
        assertFalse(Blocks.hasWaterFluid(tubeCoral + 1))

        val wallCoral = stateId("tube_coral_wall_fan")
        val eastDryWallCoral = assertNotNull(Blocks.coralWallFanStateId(wallCoral, "east", waterlogged = false))
        assertEquals(wallCoral + 7, eastDryWallCoral)
        assertEquals(
            linkedMapOf("facing" to "east", "waterlogged" to "false"),
            Blocks.byStateId(eastDryWallCoral)?.properties,
        )
        assertFalse(Blocks.hasWaterFluid(eastDryWallCoral))

        val mushroomStem = stateId("mushroom_stem")
        val generatedStem =
            assertNotNull(
                Blocks.hugeMushroomStateId(
                    mushroomStem,
                    down = false,
                    east = true,
                    north = true,
                    south = true,
                    up = false,
                    west = true,
                ),
            )
        assertEquals(mushroomStem + 34, generatedStem)
        assertEquals("false", Blocks.byStateId(generatedStem)?.properties?.get("down"))
        assertEquals("false", Blocks.byStateId(generatedStem)?.properties?.get("up"))
        assertTrue(Blocks.isHugeMushroomState(generatedStem))
    }

    private fun stateId(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")

    @Test
    fun stateCountIsStableAcrossReads() {
        assertEquals(Blocks.stateCount, Blocks.stateCount)
    }

    @Test
    fun stateCountCoversEveryRegisteredAndDerivedState() {
        val count = Blocks.stateCount
        assertTrue(count > 0, "expected a non-empty block state palette")
        for (stateId in 0 until count) Blocks.byStateId(stateId)
        assertNull(Blocks.byStateId(count), "state ids are expected to stop below stateCount")
    }
}
