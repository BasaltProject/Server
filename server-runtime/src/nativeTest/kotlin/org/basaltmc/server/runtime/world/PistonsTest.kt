package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.BlockFace
import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.registry.BlockSupport
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.registry.PushReaction
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
import kotlin.test.assertTrue
import kotlin.uuid.ExperimentalUuidApi
import kotlin.uuid.Uuid

/** parity: `PistonStructureResolver` and `PistonBaseBlock.isPushable`. */
@OptIn(ExperimentalUuidApi::class)
class PistonsTest {
    private val world =
        LoadedWorld(
            WorldDeclaration("pistons", WorldDimension.OVERWORLD, WorldType.VOID, seed = 0L, isDefault = true),
            WorldSettings.VANILLA,
            WorldMetadata(0L, 0, 64, 0),
            "build/test-data/pistons",
            Logger("PistonsTest").apply { minLevel = LogLevel.ERROR },
            1,
        )

    init {
        world.state.getOrGenerateChunk(0, 0)
        world.state.getOrGenerateChunk(1, 0)
    }

    @AfterTest
    fun stopGenerating() {
        world.stopGenerating(drainTimeoutMillis = 100)
    }

    private fun state(
        name: String,
        properties: Map<String, String> = emptyMap(),
    ): Int = checkNotNull(BlockStateDefinitions.stateIdOf(Identifier.vanilla(name), properties))

    private fun pathAt(
        x: Int,
        y: Int,
        z: Int,
    ): String? = BlockStateDefinitions.blockOf(world.state.getBlockState(x, y, z) ?: 0)?.path

    /** A piston at (1,70,1) facing east, with a row of stone in front of it. */
    private fun rowOfStone(
        count: Int,
        y: Int,
    ) {
        world.state.setBlockState(1, y, 1, state("piston", mapOf("facing" to "east", "extended" to "false")))
        for (step in 0 until count) world.state.setBlockState(2 + step, y, 1, Blocks.STONE.stateId)
    }

    @Test
    fun itPushesTheRowOneAlong() {
        rowOfStone(count = 3, y = 70)

        val push = checkNotNull(Pistons.resolve(world, 1, 70, 1, BlockFace.EAST, extending = true))
        assertEquals(3, push.moved.size)
        Pistons.apply(world, push, BlockFace.EAST)

        assertNull(pathAt(2, 70, 1)?.takeIf { it != "air" }, "the first space is free for the head")
        assertEquals("stone", pathAt(3, 70, 1))
        assertEquals("stone", pathAt(5, 70, 1), "and the far end moved along too")
    }

    /** parity: `PistonStructureResolver`'s twelve, which is what a slime block farm is built around. */
    @Test
    fun thirteenBlocksAreTooMany() {
        rowOfStone(count = 12, y = 72)
        assertEquals(
            12,
            checkNotNull(Pistons.resolve(world, 1, 72, 1, BlockFace.EAST, extending = true)).moved.size,
        )

        world.state.setBlockState(14, 72, 1, Blocks.STONE.stateId)
        assertNull(
            Pistons.resolve(world, 1, 72, 1, BlockFace.EAST, extending = true),
            "the thirteenth stops it altogether",
        )
    }

    /** parity: obsidian, which `isPushable` names before it looks at anything else. */
    @Test
    fun obsidianRefusesToMove() {
        world.state.setBlockState(1, 74, 1, state("piston", mapOf("facing" to "east", "extended" to "false")))
        world.state.setBlockState(2, 74, 1, state("obsidian"))

        assertNull(Pistons.resolve(world, 1, 74, 1, BlockFace.EAST, extending = true))
    }

    /** parity: `PushReaction.DESTROY`: a torch in the way breaks instead of stopping the piston. */
    @Test
    fun aTorchInTheWayBreaks() {
        world.state.setBlockState(1, 76, 1, state("piston", mapOf("facing" to "east", "extended" to "false")))
        world.state.setBlockState(2, 76, 1, state("torch"))

        val push = checkNotNull(Pistons.resolve(world, 1, 76, 1, BlockFace.EAST, extending = true))

        assertEquals(1, push.destroyed.size)
        assertEquals(0, push.moved.size)
        Pistons.apply(world, push, BlockFace.EAST)
        assertEquals("air", pathAt(2, 76, 1))
    }

    /**
     * parity: `PistonBaseBlock.checkIfExtend` and its two ticks: the signal books the move, the move
     * lands later, and the arm is a block of its own.
     */
    @Test
    fun aPistonExtendsAndPullsBackAgain() {
        world.state.setBlockState(1, 90, 1, state("sticky_piston", mapOf("facing" to "east", "extended" to "false")))
        world.state.setBlockState(2, 90, 1, Blocks.STONE.stateId)
        world.state.setBlockState(1, 91, 1, state("redstone_block"))

        Pistons.trigger(world, 1, 90, 1)
        val pending =
            world.state
                .chunkIfLoaded(0, 0)
                ?.scheduledBlockTicks()
                .orEmpty()
        assertTrue(
            pending.any { it.localX == 1 && it.y == 90 && it.type.path == "piston" && it.delay == 2 },
            "two ticks, like vanilla's slide: $pending",
        )

        Pistons.finish(world, 1, 90, 1)
        assertEquals("piston_head", pathAt(2, 90, 1), "the arm is out")
        assertEquals("stone", pathAt(3, 90, 1), "and the stone moved along")
        assertEquals("true", BlockStateDefinitions.propertiesOf(world.state.getBlockState(1, 90, 1) ?: 0)["extended"])

        // Take the power away and it comes back, bringing the stone with it because it is sticky.
        world.state.setBlockState(1, 91, 1, Blocks.AIR.stateId)
        Pistons.finish(world, 1, 90, 1)
        assertEquals("stone", pathAt(2, 90, 1), "a sticky piston pulls it back")
        assertEquals("air", pathAt(3, 90, 1))
        assertEquals("false", BlockStateDefinitions.propertiesOf(world.state.getBlockState(1, 90, 1) ?: 0)["extended"])
    }

    /**
     * parity: `MovingPistonBlock`: while the arm slides, what is being pushed is a `moving_piston`
     * carrying the state it will become. It is not the block it was, and it is not solid.
     */
    @Test
    fun aBlockOnTheMoveIsAMovingPiston() {
        world.state.setBlockState(1, 96, 1, state("piston", mapOf("facing" to "east", "extended" to "false")))
        world.state.setBlockState(2, 96, 1, Blocks.STONE.stateId)
        world.state.setBlockState(1, 97, 1, state("redstone_block"))

        Pistons.trigger(world, 1, 96, 1)

        assertEquals("moving_piston", pathAt(2, 96, 1), "mid-slide it is neither stone nor air")
        val carried =
            checkNotNull(world.state.chunkIfLoaded(0, 0)?.blockEntityAt(2, 96, 1)) { "no block entity on the move" }
        assertEquals(Blocks.STONE.stateId, (carried.data["blockState"] as? org.basaltmc.wire.nbt.NbtInt)?.value)

        Pistons.finish(world, 1, 96, 1)

        assertEquals("stone", pathAt(3, 96, 1), "and it lands where it was going")
        assertEquals("piston_head", pathAt(2, 96, 1))
    }

    /** parity: `getNeighborSignal`, which will not let a piston be powered by its own face. */
    @Test
    fun aPistonIgnoresASignalOnItsFace() {
        world.state.setBlockState(1, 94, 1, state("piston", mapOf("facing" to "east", "extended" to "false")))
        world.state.setBlockState(2, 94, 1, state("redstone_block"))

        Pistons.trigger(world, 1, 94, 1)

        val pending =
            world.state
                .chunkIfLoaded(0, 0)
                ?.scheduledBlockTicks()
                .orEmpty()
        assertEquals(0, pending.count { it.localX == 1 && it.y == 94 }, "the block it faces does not count")
    }

    /** parity: `addBranchingBlocks`: a slime block takes what is stuck to its sides with it. */
    @Test
    fun aSlimeBlockDragsWhatIsStuckToIt() {
        world.state.setBlockState(1, 84, 1, state("piston", mapOf("facing" to "east", "extended" to "false")))
        world.state.setBlockState(2, 84, 1, state("slime_block"))
        // Off to the side, on the axis the push does not run along.
        world.state.setBlockState(2, 84, 2, Blocks.STONE.stateId)
        world.state.setBlockState(2, 85, 1, Blocks.STONE.stateId)

        val push = checkNotNull(Pistons.resolve(world, 1, 84, 1, BlockFace.EAST, extending = true))

        assertEquals(3, push.moved.size, "the slime block and the two stuck to it")
        Pistons.apply(world, push, BlockFace.EAST)
        assertEquals("slime_block", pathAt(3, 84, 1))
        assertEquals("stone", pathAt(3, 84, 2), "the one beside it came along")
        assertEquals("stone", pathAt(3, 85, 1), "and the one above it too")
    }

    /** parity: `canStickToEachOther`: honey and slime stick to everything except each other. */
    @Test
    fun honeyAndSlimeDoNotStickToEachOther() {
        world.state.setBlockState(1, 88, 1, state("piston", mapOf("facing" to "east", "extended" to "false")))
        world.state.setBlockState(2, 88, 1, state("slime_block"))
        world.state.setBlockState(2, 88, 2, state("honey_block"))

        val push = checkNotNull(Pistons.resolve(world, 1, 88, 1, BlockFace.EAST, extending = true))

        assertEquals(1, push.moved.size, "the honey block stays where it is")
    }

    /**
     * The three ways vanilla stops a piston, which are not the same rule: a reaction of `BLOCK`, an
     * unbreakable block, and a block entity. Bedrock and chests both read `NORMAL` and neither moves.
     */
    @Test
    fun theThreeWaysABlockRefusesToMove() {
        assertEquals(PushReaction.NORMAL, BlockSupport.pushReaction(Blocks.STONE.stateId))
        assertEquals(PushReaction.DESTROY, BlockSupport.pushReaction(state("torch")))

        world.state.setBlockState(1, 78, 1, state("piston", mapOf("facing" to "east", "extended" to "false")))
        for ((offset, name) in listOf(0 to "bedrock", 2 to "chest", 4 to "obsidian")) {
            val y = 78 + offset
            world.state.setBlockState(1, y, 1, state("piston", mapOf("facing" to "east", "extended" to "false")))
            world.state.setBlockState(
                2,
                y,
                1,
                if (name == "chest") state(name, mapOf("facing" to "north", "type" to "single")) else state(name),
            )
            assertNull(Pistons.resolve(world, 1, y, 1, BlockFace.EAST, extending = true), "$name should not move")
        }
        assertEquals(
            PushReaction.NORMAL,
            BlockSupport.pushReaction(state("chest", mapOf("facing" to "north", "type" to "single"))),
            "a chest reads NORMAL: what stops it is holding a block entity",
        )
        assertTrue(
            org.basaltmc.server.registry.BlockConnections.hasBlockEntity(
                state("chest", mapOf("facing" to "north", "type" to "single")),
            ),
        )
    }

    /**
     * parity: `PistonMovingBlockEntity.moveCollidedEntities`: what is standing where a pushed block
     * is going comes along with it. The item is moved here; the player is handed back, because
     * moving one means telling its client.
     */
    @Test
    fun whatIsInTheWayIsShovedAlong() {
        world.state.setBlockState(1, 100, 1, state("piston", mapOf("facing" to "east", "extended" to "false")))
        world.state.setBlockState(2, 100, 1, Blocks.STONE.stateId)
        world.state.setBlockState(1, 101, 1, state("redstone_block"))
        val dropped =
            world.state.spawnItem(
                WorldState.DroppedItem(
                    entityId = 1,
                    uuid = Uuid.random(),
                    item = Identifier.vanilla("stick"),
                    count = 1,
                    x = 3.5,
                    y = 100.0,
                    z = 1.5,
                ),
            )
        val standing = Pistons.PlayerPosition("someone", 3.5, 100.0, 1.5)
        val pushed = mutableListOf<Pistons.PlayerPosition>()

        Pistons.trigger(world, 1, 100, 1)
        Pistons.finish(world, 1, 100, 1, listOf(standing)) { pushed += it }

        assertEquals(4.5, dropped.x, "the item went east with the stone")
        assertEquals(1, pushed.size, "and so did the player")
        assertEquals(4.5, pushed[0].x)
        assertEquals(100.0, pushed[0].y, "sideways only")
    }

    /** Nothing standing in the way means nothing to move, which is the usual case. */
    @Test
    fun anEmptyPathMovesNobody() {
        world.state.setBlockState(1, 104, 1, state("piston", mapOf("facing" to "east", "extended" to "false")))
        world.state.setBlockState(2, 104, 1, Blocks.STONE.stateId)
        world.state.setBlockState(1, 105, 1, state("redstone_block"))
        val away = Pistons.PlayerPosition("elsewhere", 3.5, 104.0, 8.5)
        val pushed = mutableListOf<Pistons.PlayerPosition>()

        Pistons.trigger(world, 1, 104, 1)
        Pistons.finish(world, 1, 104, 1, listOf(away)) { pushed += it }

        assertTrue(pushed.isEmpty(), "three blocks away is not in the way")
    }
}
