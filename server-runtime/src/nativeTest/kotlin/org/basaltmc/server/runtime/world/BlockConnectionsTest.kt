package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.runtime.config.WorldDeclaration
import org.basaltmc.server.runtime.config.WorldDimension
import org.basaltmc.server.runtime.config.WorldSettings
import org.basaltmc.server.runtime.config.WorldType
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.common.logging.LogLevel
import org.basaltmc.wire.common.logging.Logger
import kotlin.test.AfterTest
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNull

class BlockConnectionsTest {
    private val world =
        LoadedWorld(
            WorldDeclaration("connections", WorldDimension.OVERWORLD, WorldType.VOID, seed = 0L, isDefault = true),
            WorldSettings.VANILLA,
            WorldMetadata(0L, 0, 64, 0),
            "build/test-data/connections",
            Logger("ConnectionsTest").apply { minLevel = LogLevel.ERROR },
            1,
        )

    init {
        world.state.getOrGenerateChunk(0, 0)
    }

    @AfterTest
    fun stopGenerating() {
        world.stopGenerating(drainTimeoutMillis = 100)
    }

    private fun state(
        name: String,
        properties: Map<String, String> = emptyMap(),
    ): Int = checkNotNull(BlockStateDefinitions.stateIdOf(Identifier.vanilla(name), properties))

    private fun distanceAt(
        x: Int,
        y: Int,
        z: Int,
    ): String? =
        BlockStateDefinitions
            .propertiesOf(checkNotNull(world.state.getBlockState(x, y, z)))["distance"]

    private fun shapeAt(
        x: Int,
        y: Int,
        z: Int,
    ): String? = BlockStateDefinitions.propertiesOf(checkNotNull(world.state.getBlockState(x, y, z)))["shape"]

    private fun sidesAt(
        x: Int,
        y: Int,
        z: Int,
    ): Map<String, String> {
        val properties = BlockStateDefinitions.propertiesOf(checkNotNull(world.state.getBlockState(x, y, z)))
        return properties.filterKeys { it in listOf("north", "south", "east", "west") }
    }

    @Test
    fun twoFencesInARowJoinUp() {
        world.state.setBlockState(1, 64, 1, state("oak_fence"))
        world.state.setBlockState(1, 64, 2, state("oak_fence"))

        NeighbourUpdates.reshape(world, 1, 64, 1)
        NeighbourUpdates.reshape(world, 1, 64, 2)

        assertEquals("true", sidesAt(1, 64, 1)["south"])
        assertEquals("true", sidesAt(1, 64, 2)["north"])
        assertEquals("false", sidesAt(1, 64, 1)["north"])
    }

    /** parity: `FenceBlock.isSameFence`: wood joins wood, nether brick joins nether brick. */
    @Test
    fun aWoodenFenceIgnoresANetherBrickOne() {
        world.state.setBlockState(3, 64, 1, state("oak_fence"))
        world.state.setBlockState(3, 64, 2, state("nether_brick_fence"))

        NeighbourUpdates.reshape(world, 3, 64, 1)

        assertEquals("false", sidesAt(3, 64, 1)["south"])
    }

    @Test
    fun aFenceGrabsASolidBlockAndNotLeaves() {
        world.state.setBlockState(5, 64, 1, state("oak_fence"))
        world.state.setBlockState(5, 64, 2, Blocks.STONE.stateId)
        world.state.setBlockState(4, 64, 1, state("oak_leaves"))

        NeighbourUpdates.reshape(world, 5, 64, 1)

        assertEquals("true", sidesAt(5, 64, 1)["south"])
        assertEquals("false", sidesAt(5, 64, 1)["west"], "leaves refuse to be connected to")
    }

    /** parity: `FenceGateBlock.connectsToDirection`, which joins the line the gate stands across. */
    @Test
    fun aFenceJoinsAGateThatFacesAcrossIt() {
        world.state.setBlockState(7, 64, 1, state("oak_fence"))
        world.state.setBlockState(7, 64, 2, state("oak_fence_gate", mapOf("facing" to "east")))
        world.state.setBlockState(9, 64, 1, state("oak_fence"))
        world.state.setBlockState(9, 64, 2, state("oak_fence_gate", mapOf("facing" to "north")))

        NeighbourUpdates.reshape(world, 7, 64, 1)
        NeighbourUpdates.reshape(world, 9, 64, 1)

        assertEquals("true", sidesAt(7, 64, 1)["south"], "a gate across the line joins it")
        assertEquals("false", sidesAt(9, 64, 1)["south"], "one along the line does not")
    }

    @Test
    fun panesJoinPanesAndWallsAndSolidBlocks() {
        world.state.setBlockState(11, 64, 1, state("glass_pane"))
        world.state.setBlockState(11, 64, 2, state("iron_bars"))
        world.state.setBlockState(10, 64, 1, state("cobblestone_wall"))

        NeighbourUpdates.reshape(world, 11, 64, 1)

        assertEquals("true", sidesAt(11, 64, 1)["south"])
        assertEquals("true", sidesAt(11, 64, 1)["west"])
    }

    /** parity: `LeavesBlock.updateDistance`: a trunk is zero and every ring out counts one more. */
    @Test
    fun leavesCountTheirDistanceFromTheTrunk() {
        world.state.setBlockState(1, 70, 1, state("oak_log", mapOf("axis" to "y")))
        for (offset in 1..3) {
            world.state.setBlockState(1 + offset, 70, 1, state("oak_leaves", mapOf("distance" to "7")))
        }
        for (offset in 1..3) NeighbourUpdates.reshape(world, 1 + offset, 70, 1)

        assertEquals("1", distanceAt(2, 70, 1))
        assertEquals("2", distanceAt(3, 70, 1))
        assertEquals("3", distanceAt(4, 70, 1))
    }

    /** Cutting the trunk pushes every ring one further out, which is what starts a canopy decaying. */
    @Test
    fun fellingTheTrunkPushesTheLeavesOut() {
        world.state.setBlockState(1, 75, 1, state("oak_log", mapOf("axis" to "y")))
        for (offset in 1..3) {
            world.state.setBlockState(1 + offset, 75, 1, state("oak_leaves", mapOf("distance" to "7")))
        }
        for (offset in 1..3) NeighbourUpdates.reshape(world, 1 + offset, 75, 1)

        world.state.setBlockState(1, 75, 1, Blocks.AIR.stateId)
        NeighbourUpdates.afterChange(world, 1, 75, 1) { _, _, _, _ -> }

        assertEquals("7", distanceAt(2, 75, 1), "the ring next to the trunk is now on its own")
    }

    /**
     * parity: `StairBlock.getStairsShape`. Two flights meeting at a right angle turn a corner, and
     * which corner depends on which way the other flight faces.
     */
    @Test
    fun stairsTurnTheirCorners() {
        // Facing north with stairs to the north facing east: the far one turns the corner out.
        world.state.setBlockState(6, 70, 6, state("oak_stairs", mapOf("facing" to "north", "half" to "bottom")))
        world.state.setBlockState(6, 70, 5, state("oak_stairs", mapOf("facing" to "east", "half" to "bottom")))

        NeighbourUpdates.reshape(world, 6, 70, 6)

        assertEquals("outer_right", shapeAt(6, 70, 6))
    }

    @Test
    fun stairsBehindTheFlightMakeAnInnerCorner() {
        world.state.setBlockState(9, 70, 6, state("oak_stairs", mapOf("facing" to "north", "half" to "bottom")))
        world.state.setBlockState(9, 70, 7, state("oak_stairs", mapOf("facing" to "east", "half" to "bottom")))

        NeighbourUpdates.reshape(world, 9, 70, 6)

        assertEquals("inner_right", shapeAt(9, 70, 6))
    }

    @Test
    fun aLoneFlightStaysStraight() {
        world.state.setBlockState(12, 70, 6, state("oak_stairs", mapOf("facing" to "north", "half" to "bottom")))
        NeighbourUpdates.reshape(world, 12, 70, 6)
        assertEquals("straight", shapeAt(12, 70, 6))
    }

    /**
     * parity: `WallBlock.shouldRaisePost`: a wall in a straight line drops its post, a corner keeps
     * it, and a full block on top makes the sides tall.
     */
    @Test
    fun wallsRunLowInALineAndKeepTheirPostOnACorner() {
        for (dz in 0..2) world.state.setBlockState(2, 72, 4 + dz, state("cobblestone_wall"))
        for (dz in 0..2) NeighbourUpdates.reshape(world, 2, 72, 4 + dz)

        val middle = BlockStateDefinitions.propertiesOf(checkNotNull(world.state.getBlockState(2, 72, 5)))
        assertEquals("low", middle["north"])
        assertEquals("low", middle["south"])
        assertEquals("false", middle["up"], "a straight run loses its post")

        val end = BlockStateDefinitions.propertiesOf(checkNotNull(world.state.getBlockState(2, 72, 4)))
        assertEquals("true", end["up"], "an end keeps it")
    }

    @Test
    fun aBlockOnTopOfAWallMakesItsSidesTall() {
        world.state.setBlockState(6, 72, 4, state("cobblestone_wall"))
        world.state.setBlockState(6, 72, 5, state("cobblestone_wall"))
        world.state.setBlockState(6, 73, 4, Blocks.STONE.stateId)

        NeighbourUpdates.reshape(world, 6, 72, 4)

        val under = BlockStateDefinitions.propertiesOf(checkNotNull(world.state.getBlockState(6, 72, 4)))
        assertEquals("tall", under["south"])
        assertEquals("true", under["up"])
    }

    /** parity: `FenceGateBlock.updateShape`, which sinks a gate into the wall beside it. */
    @Test
    fun aGateBesideAWallSinksIntoIt() {
        world.state.setBlockState(2, 78, 2, state("oak_fence_gate", mapOf("facing" to "north", "in_wall" to "false")))
        world.state.setBlockState(1, 78, 2, state("cobblestone_wall"))

        NeighbourUpdates.reshape(world, 2, 78, 2)

        assertEquals(
            "true",
            BlockStateDefinitions.propertiesOf(checkNotNull(world.state.getBlockState(2, 78, 2)))["in_wall"],
        )
    }

    /**
     * parity: `ChestBlock.updateShape`. It follows a neighbour that has already claimed its side,
     * and a half whose partner has gone goes back to single. Making a double chest in the first
     * place is the placement's job, not this one's.
     */
    @Test
    fun aChestFollowsItsPartnerAndLetsGoWhenItLeaves() {
        world.state.setBlockState(5, 78, 2, state("chest", mapOf("facing" to "north", "type" to "single")))
        // parity: `ChestBlock.getConnectedDirection`: facing north, a right half pairs westwards,
        // so this one is the partner of the chest beside it and that one becomes the left half.
        world.state.setBlockState(6, 78, 2, state("chest", mapOf("facing" to "north", "type" to "right")))

        NeighbourUpdates.reshape(world, 5, 78, 2)
        assertEquals(
            "left",
            BlockStateDefinitions.propertiesOf(checkNotNull(world.state.getBlockState(5, 78, 2)))["type"],
            "it should have taken the other half",
        )

        world.state.setBlockState(6, 78, 2, Blocks.AIR.stateId)
        NeighbourUpdates.reshape(world, 5, 78, 2)
        assertEquals(
            "single",
            BlockStateDefinitions.propertiesOf(checkNotNull(world.state.getBlockState(5, 78, 2)))["type"],
            "and let go once its partner went",
        )
    }

    /** parity: `BedBlock.updateShape`: a half without its other half goes. */
    @Test
    fun halfABedDoesNotStay() {
        world.state.setBlockState(9, 78, 2, state("white_bed", mapOf("part" to "foot", "facing" to "north")))
        world.state.setBlockState(9, 78, 1, state("white_bed", mapOf("part" to "head", "facing" to "north")))

        world.state.setBlockState(9, 78, 1, Blocks.AIR.stateId)
        NeighbourUpdates.reshape(world, 9, 78, 2)

        assertEquals(Blocks.AIR.stateId, world.state.getBlockState(9, 78, 2))
    }

    /** parity: `CampfireBlock.updateShape`, whose signal fire is a hay bale underneath. */
    @Test
    fun aCampfireOverHayBecomesASignalFire() {
        world.state.setBlockState(12, 78, 2, state("campfire", mapOf("signal_fire" to "false", "lit" to "true")))
        world.state.setBlockState(12, 77, 2, state("hay_block", mapOf("axis" to "y")))

        NeighbourUpdates.reshape(world, 12, 78, 2)

        assertEquals(
            "true",
            BlockStateDefinitions.propertiesOf(checkNotNull(world.state.getBlockState(12, 78, 2)))["signal_fire"],
        )
    }

    /** parity: `DoorBlock.updateShape`: the halves keep each other in step, and alone they go. */
    @Test
    fun theHalvesOfADoorKeepEachOtherInStep() {
        world.state.setBlockState(
            14,
            78,
            2,
            state("oak_door", mapOf("half" to "lower", "facing" to "north", "open" to "false")),
        )
        world.state.setBlockState(
            14,
            79,
            2,
            state("oak_door", mapOf("half" to "upper", "facing" to "north", "open" to "true")),
        )

        NeighbourUpdates.reshape(world, 14, 78, 2)

        assertEquals(
            "true",
            BlockStateDefinitions.propertiesOf(checkNotNull(world.state.getBlockState(14, 78, 2)))["open"],
            "the bottom should have taken the top's state",
        )

        world.state.setBlockState(14, 79, 2, Blocks.AIR.stateId)
        NeighbourUpdates.reshape(world, 14, 78, 2)
        assertEquals(Blocks.AIR.stateId, world.state.getBlockState(14, 78, 2), "and half a door does not stay")
    }

    @Test
    fun anOrdinaryBlockHasNothingToReshape() {
        world.state.setBlockState(13, 64, 1, Blocks.STONE.stateId)
        assertNull(NeighbourUpdates.reshape(world, 13, 64, 1))
    }
}
