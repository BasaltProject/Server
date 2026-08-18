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
import org.basaltmc.wire.protocol.play.Direction
import kotlin.test.AfterTest
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlin.test.assertTrue

/** parity: `ChestBlock.getStateForPlacement` and `DOUBLE_INVENTORY_COMBINER`. */
class DoubleChestsTest {
    private val world =
        LoadedWorld(
            WorldDeclaration("chests", WorldDimension.OVERWORLD, WorldType.VOID, seed = 0L, isDefault = true),
            WorldSettings.VANILLA,
            WorldMetadata(0L, 0, 64, 0),
            "build/test-data/chests",
            Logger("DoubleChestsTest").apply { minLevel = LogLevel.ERROR },
            1,
        )

    init {
        world.state.getOrGenerateChunk(0, 0)
    }

    @AfterTest
    fun stopGenerating() {
        world.stopGenerating(drainTimeoutMillis = 100)
    }

    private val chest = Identifier.vanilla("chest")

    /** Every chest in 26.2, and none of them invented: each one has to be a real block. */
    @Test
    fun everyChestNameIsARealBlock() {
        assertEquals(10, Containers.CHESTS.size)
        assertTrue("waxed_oxidized_copper_chest" in Containers.CHESTS)
        for (path in Containers.CHESTS) {
            assertNotNull(Blocks.REGISTRY.idOf(Identifier.vanilla(path)), path)
            assertEquals(Containers.CHEST_SIZE, Containers.SIZES[path], path)
            assertTrue(path in Containers.PLAIN, path)
        }
    }

    /** parity: `getChestType`, which pairs a chest with the single one already beside it. */
    @Test
    fun aChestPutBesideAnotherPairsWithIt() {
        // One chest facing north, alone.
        val first =
            BlockPlacement.stateFor(
                block = chest,
                defaultStateId = checkNotNull(Blocks.REGISTRY.idOf(chest)),
                face = Direction.UP,
                clickY = 1f,
                yaw = 180f,
                pitch = 0f,
                replacing = Blocks.AIR.stateId,
                world = world,
                x = 2,
                y = 70,
                z = 2,
            )
        world.state.setBlockState(2, 70, 2, first)
        assertEquals("single", BlockStateDefinitions.propertiesOf(first)["type"])
        val facing = checkNotNull(BlockStateDefinitions.propertiesOf(first)["facing"])

        // A second one right beside it, placed the same way, is half of a double chest.
        val second =
            BlockPlacement.stateFor(
                block = chest,
                defaultStateId = checkNotNull(Blocks.REGISTRY.idOf(chest)),
                face = Direction.UP,
                clickY = 1f,
                yaw = 180f,
                pitch = 0f,
                replacing = Blocks.AIR.stateId,
                world = world,
                x = 3,
                y = 70,
                z = 2,
            )
        world.state.setBlockState(3, 70, 2, second)
        val secondType = checkNotNull(BlockStateDefinitions.propertiesOf(second)["type"])
        assertTrue(secondType == "left" || secondType == "right", "it paired: $secondType")
        assertEquals(facing, BlockStateDefinitions.propertiesOf(second)["facing"])

        // And the first one completes the pair when it is told its neighbour moved.
        NeighbourUpdates.afterChange(world, 3, 70, 2, reshapeSelf = false) { _, _, _, _ -> }
        val firstNow = checkNotNull(world.state.getBlockState(2, 70, 2))
        assertEquals(
            if (secondType == "left") "right" else "left",
            BlockStateDefinitions.propertiesOf(firstNow)["type"],
        )
    }

    /** parity: whichever half is opened, the window shows the same two halves in the same order. */
    @Test
    fun bothHalvesOpenTheSameWindow() {
        val facing = "north"
        world.state.setBlockState(
            6,
            70,
            2,
            checkNotNull(BlockStateDefinitions.stateIdOf(chest, mapOf("facing" to facing, "type" to "left"))),
        )
        world.state.setBlockState(
            7,
            70,
            2,
            checkNotNull(BlockStateDefinitions.stateIdOf(chest, mapOf("facing" to facing, "type" to "right"))),
        )

        val fromLeft = assertNotNull(DoubleChests.halves(world, 6, 70, 2))
        val fromRight = assertNotNull(DoubleChests.halves(world, 7, 70, 2))

        assertEquals(fromLeft.first.toList(), fromRight.first.toList(), "the same half leads")
        assertEquals(fromLeft.second.toList(), fromRight.second.toList())
        // parity: the right half is the one whose slots come first.
        assertEquals(listOf(7, 70, 2), fromLeft.first.toList())
    }

    /** A lone chest is not a pair, whatever its neighbours are. */
    @Test
    fun aSingleChestIsNotAPair() {
        world.state.setBlockState(
            10,
            70,
            2,
            checkNotNull(BlockStateDefinitions.stateIdOf(chest, mapOf("facing" to "north", "type" to "single"))),
        )

        assertNull(DoubleChests.halves(world, 10, 70, 2))
    }
}
