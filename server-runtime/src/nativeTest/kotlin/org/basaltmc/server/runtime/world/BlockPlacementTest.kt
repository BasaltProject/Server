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
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNull
import kotlin.test.assertTrue

class BlockPlacementTest {
    private val world by lazy {
        LoadedWorld(
            WorldDeclaration("placement", WorldDimension.OVERWORLD, WorldType.VOID, seed = 0L, isDefault = true),
            WorldSettings.VANILLA,
            WorldMetadata(0L, 0, 64, 0),
            "build/test-data/placement",
            Logger("BlockPlacementTest").apply { minLevel = LogLevel.ERROR },
            1,
        ).also { it.state.getOrGenerateChunk(0, 0) }
    }

    /**
     * parity: `DoorBlock.getHinge`: a door with a solid block on one side hangs away from it, and
     * with nothing to go by it hangs on the side that was clicked.
     */
    @Test
    fun aDoorHangsAwayFromTheWallBesideIt() {
        world.state.setBlockState(4, 70, 4, Blocks.STONE.stateId)
        val hinged =
            BlockPlacement.stateFor(
                block = Identifier.vanilla("oak_door"),
                defaultStateId = checkNotNull(Blocks.REGISTRY.idOf(Identifier.vanilla("oak_door"))),
                face = Direction.UP,
                clickY = 0f,
                yaw = 0f,
                pitch = 0f,
                replacing = Blocks.AIR.stateId,
                world = world,
                x = 5,
                y = 70,
                z = 4,
                clickX = 0.5f,
                clickZ = 0.5f,
            )
        // Facing south puts the stone at x=4 on the door's right, and a door hinges towards the
        // wall beside it so that it opens away from it.
        assertEquals("right", BlockStateDefinitions.propertiesOf(hinged)["hinge"])
        world.stopGenerating(drainTimeoutMillis = 100)
    }

    private fun state(
        name: String,
        properties: Map<String, String> = emptyMap(),
    ): Int = checkNotNull(BlockStateDefinitions.stateIdOf(Identifier.vanilla(name), properties))

    /** The same as [placed], but somewhere in a world so the block can look at its neighbours. */
    private fun placedIn(
        name: String,
        x: Int,
        y: Int,
        z: Int,
        face: Direction = Direction.UP,
    ): Map<String, String> {
        val block = Identifier.vanilla(name)
        val stateId =
            BlockPlacement.stateFor(
                block = block,
                defaultStateId = checkNotNull(Blocks.REGISTRY.idOf(block)),
                face = face,
                clickY = 0f,
                yaw = 0f,
                pitch = 0f,
                replacing = Blocks.AIR.stateId,
                world = world,
                x = x,
                y = y,
                z = z,
            )
        return BlockStateDefinitions.propertiesOf(stateId)
    }

    private fun placed(
        name: String,
        face: Direction = Direction.UP,
        clickY: Float = 0f,
        yaw: Float = 0f,
        pitch: Float = 0f,
        replacing: Int = Blocks.AIR.stateId,
    ): Map<String, String> {
        val block = Identifier.vanilla(name)
        val default = checkNotNull(Blocks.REGISTRY.idOf(block)) { "no block called $name" }
        val stateId = BlockPlacement.stateFor(block, default, face, clickY, yaw, pitch, replacing)
        return BlockStateDefinitions.propertiesOf(stateId)
    }

    /**
     * parity: every `SimpleWaterloggedBlock`'s `getStateForPlacement`: a block put into water keeps
     * it. Without this the water was simply gone, which is the difference between building a path
     * under the sea and draining it.
     */
    @Test
    fun aBlockPutIntoWaterKeepsIt() {
        assertEquals("true", placed("oak_slab", replacing = Blocks.WATER.stateId)["waterlogged"])
        assertEquals("true", placed("oak_stairs", replacing = Blocks.WATER.stateId)["waterlogged"])
        assertEquals("true", placed("oak_fence", replacing = Blocks.WATER.stateId)["waterlogged"])
        assertEquals("false", placed("oak_slab")["waterlogged"], "and dry air leaves it dry")
    }

    /** The water in a block that was already logged carries over to whatever replaces it. */
    @Test
    fun replacingAWaterloggedBlockKeepsTheWaterToo() {
        val logged =
            checkNotNull(
                BlockStateDefinitions.stateIdOf(
                    Identifier.vanilla("oak_slab"),
                    mapOf("waterlogged" to "true", "type" to "bottom"),
                ),
            )
        assertEquals("true", placed("oak_stairs", replacing = logged)["waterlogged"])
    }

    /** parity: `SnowLayerBlock`, which piles another layer into the one already there. */
    @Test
    fun snowPilesUp() {
        val three =
            checkNotNull(BlockStateDefinitions.stateIdOf(Identifier.vanilla("snow"), mapOf("layers" to "3")))
        assertEquals("4", placed("snow", replacing = three)["layers"])
    }

    /** parity: `SnowyBlock.getStateForPlacement`: grass under snow goes down already white. */
    @Test
    fun grassUnderSnowIsSnowy() {
        world.state.setBlockState(6, 71, 6, state("snow_block"))
        assertEquals("true", placedIn("grass_block", 6, 70, 6)["snowy"])
        assertEquals("false", placedIn("grass_block", 7, 70, 6)["snowy"])
    }

    /** parity: `CampfireBlock.getStateForPlacement`: hay under it makes the tall signal smoke. */
    @Test
    fun aCampfireOverHayIsASignalFire() {
        world.state.setBlockState(8, 69, 6, state("hay_block"))
        assertEquals("true", placedIn("campfire", 8, 70, 6)["signal_fire"])
        assertEquals("false", placedIn("campfire", 9, 70, 6)["signal_fire"])
    }

    /** parity: `BellBlock.getStateForPlacement`, bolted to one wall or to two. */
    @Test
    fun aBellBetweenTwoWallsIsBoltedToBoth() {
        world.state.setBlockState(11, 70, 6, Blocks.STONE.stateId)
        world.state.setBlockState(13, 70, 6, Blocks.STONE.stateId)
        assertEquals("double_wall", placedIn("bell", 12, 70, 6, face = Direction.EAST)["attachment"])
        assertEquals("single_wall", placedIn("bell", 12, 70, 8, face = Direction.EAST)["attachment"])
        assertEquals("ceiling", placedIn("bell", 12, 70, 10, face = Direction.DOWN)["attachment"])
    }

    /** parity: `BambooStalkBlock.getStateForPlacement`, which is a sapling on the ground. */
    @Test
    fun bambooOnTheGroundIsASapling() {
        world.state.setBlockState(3, 69, 8, state("sand"))
        val placedBlock =
            BlockPlacement.stateFor(
                block = Identifier.vanilla("bamboo"),
                defaultStateId = checkNotNull(Blocks.REGISTRY.idOf(Identifier.vanilla("bamboo"))),
                face = Direction.UP,
                clickY = 0f,
                yaw = 0f,
                pitch = 0f,
                replacing = Blocks.AIR.stateId,
                world = world,
                x = 3,
                y = 70,
                z = 8,
            )
        assertEquals("bamboo_sapling", BlockStateDefinitions.blockOf(placedBlock)?.path)
    }

    /** parity: `SpeleothemBlock.getStateForPlacement`: it points away from what it was hung on. */
    @Test
    fun aDripstoneHungFromACeilingPointsDown() {
        assertEquals("down", placed("pointed_dripstone", face = Direction.DOWN)["vertical_direction"])
        assertEquals("up", placed("pointed_dripstone", face = Direction.UP)["vertical_direction"])
    }

    /**
     * parity: `BedBlock.getStateForPlacement`, which is `getHorizontalDirection()` as it comes: the
     * head goes away from the player, not towards them. Placed backwards you sleep facing the wall.
     */
    @Test
    fun aBedRunsAwayFromWhoeverPlacesIt() {
        assertEquals("south", placed("red_bed", yaw = 0f)["facing"], "looking south lays the bed southwards")
        assertEquals("west", placed("red_bed", yaw = 90f)["facing"])
        assertEquals("north", placed("red_bed", yaw = 180f)["facing"])
        assertEquals("east", placed("red_bed", yaw = 270f)["facing"])
    }

    /** parity: `FenceGateBlock.getStateForPlacement`, the same direction as it comes. */
    @Test
    fun aGateFacesTheWayThePlayerLooks() {
        assertEquals("south", placed("oak_fence_gate", yaw = 0f)["facing"])
    }

    /** And the other half of the pair: a chest faces back at whoever put it down. */
    @Test
    fun aChestLooksBackAtWhoeverPlacesIt() {
        assertEquals("north", placed("chest", yaw = 0f)["facing"])
    }

    /**
     * The whole chain a placement really runs: the state, the half that goes down first and where
     * the other half lands. A bed lays its head one block along the way the player looks.
     */
    @Test
    fun aBedLaysItsHeadAwayFromThePlayer() {
        val bed = Identifier.vanilla("red_bed")
        val placedStateId =
            BlockPlacement.stateFor(
                block = bed,
                defaultStateId = checkNotNull(Blocks.REGISTRY.idOf(bed)),
                face = Direction.UP,
                clickY = 0f,
                yaw = 0f,
                pitch = 0f,
                replacing = Blocks.AIR.stateId,
            )
        val foot = DoubleBlocks.firstHalf(placedStateId)
        val footProperties = BlockStateDefinitions.propertiesOf(foot)
        assertEquals("foot", footProperties["part"], "the foot goes where the player clicked")
        assertEquals("south", footProperties["facing"])

        val head = checkNotNull(DoubleBlocks.secondHalf(foot, 10, 70, 10))
        assertEquals(11, head.z, "and the head one block south of it, away from the player")
        assertEquals(10, head.x)
        val headProperties = BlockStateDefinitions.propertiesOf(head.stateId)
        assertEquals("head", headProperties["part"])
        assertEquals("south", headProperties["facing"], "both halves face the same way")
    }

    /**
     * parity: `StandingAndWallBlockItem`, `SignItem` and `HangingSignItem`: what goes against a side
     * is the wall form of the block, not the standing one refusing to stand.
     */
    @Test
    fun aTorchAgainstAWallIsAWallTorch() {
        assertEquals(Identifier.vanilla("wall_torch"), WallVariants.of(Identifier.vanilla("torch")))
        assertEquals(Identifier.vanilla("soul_wall_torch"), WallVariants.of(Identifier.vanilla("soul_torch")))
        assertEquals(Identifier.vanilla("oak_wall_sign"), WallVariants.of(Identifier.vanilla("oak_sign")))
        assertEquals(
            Identifier.vanilla("oak_wall_hanging_sign"),
            WallVariants.of(Identifier.vanilla("oak_hanging_sign")),
        )
        assertEquals(Identifier.vanilla("white_wall_banner"), WallVariants.of(Identifier.vanilla("white_banner")))
        assertEquals(
            Identifier.vanilla("skeleton_wall_skull"),
            WallVariants.of(Identifier.vanilla("skeleton_skull")),
        )
        assertEquals(Identifier.vanilla("tube_coral_wall_fan"), WallVariants.of(Identifier.vanilla("tube_coral_fan")))
        assertNull(WallVariants.of(Identifier.vanilla("stone")), "an ordinary block has no wall form")
        assertNull(WallVariants.of(Identifier.vanilla("oak_door")))
    }

    /** And it points away from the wall it hangs on. */
    @Test
    fun aWallTorchPointsAwayFromItsWall() {
        assertEquals("north", placed("wall_torch", face = Direction.NORTH)["facing"])
        assertEquals("east", placed("oak_wall_sign", face = Direction.EAST)["facing"])
    }

    /** parity: `BaseRailBlock.getStateForPlacement`: the rail lies along the way the player looks. */
    @Test
    fun aRailLiesAlongTheWayThePlayerLooks() {
        assertEquals("north_south", placed("rail", yaw = 0f)["shape"])
        assertEquals("east_west", placed("rail", yaw = 90f)["shape"])
        assertEquals("north_south", placed("powered_rail", yaw = 180f)["shape"])
    }

    /**
     * parity: `ScaffoldingBlockItem.updatePlacementContext`: clicking the side of a tower puts the
     * next scaffolding on top of it, not against the face, unless the player is sneaking.
     */
    @Test
    fun scaffoldingBuildsUpTheTower() {
        world.state.setBlockState(2, 70, 12, state("scaffolding", mapOf("bottom" to "true", "distance" to "0")))
        world.state.setBlockState(2, 71, 12, state("scaffolding", mapOf("bottom" to "false", "distance" to "1")))

        val up =
            ScaffoldingPlacement.target(world, 2, 70, 12, Direction.NORTH, yaw = 0f, sneaking = false, inside = false)
        assertEquals(Triple(2, 72, 12), up, "the side of a tower builds it upwards")

        val sneaked =
            ScaffoldingPlacement.target(world, 2, 70, 12, Direction.NORTH, yaw = 0f, sneaking = true, inside = false)
        assertEquals(Triple(2, 70, 11), sneaked, "sneaking hangs it off the face that was clicked")
    }

    /** parity: `LeavesBlock.getStateForPlacement`: what a player puts down never decays. */
    @Test
    fun leavesAPlayerPlacesArePersistent() {
        assertEquals("true", placed("oak_leaves")["persistent"])
    }

    /** parity: `StandingSignBlock`: sixteen segments of the yaw, turned to face whoever placed it. */
    @Test
    fun aSignFacesWhoeverPutItDown() {
        assertEquals("8", placed("oak_sign", yaw = 0f)["rotation"], "looking south, the sign looks north")
        assertEquals("12", placed("oak_sign", yaw = 90f)["rotation"])
        assertEquals("0", placed("oak_sign", yaw = 180f)["rotation"])
        assertEquals("4", placed("oak_sign", yaw = 270f)["rotation"])
    }

    /**
     * parity: `SkullBlock.getStateForPlacement`, which is `getRotation()` *without* the half turn
     * that `StandingSignBlock` and `BannerBlock` add: a head looks the same way the player does,
     * a sign looks back at them.
     */
    @Test
    fun aHeadLooksTheSameWayThePlayerDoes() {
        assertEquals("0", placed("skeleton_skull", yaw = 0f)["rotation"])
        assertEquals("4", placed("player_head", yaw = 90f)["rotation"])
        assertEquals("8", placed("oak_sign", yaw = 0f)["rotation"], "the sign still turns around")
        assertEquals("8", placed("white_banner", yaw = 0f)["rotation"], "and so does the banner")
    }

    /** parity: `LanternBlock.getStateForPlacement`, which hangs one put against a ceiling. */
    @Test
    fun aLanternUnderABlockHangs() {
        assertEquals("true", placed("lantern", face = Direction.DOWN)["hanging"])
        assertEquals("false", placed("lantern", face = Direction.UP)["hanging"])
    }

    /** parity: `CandleBlock.getStateForPlacement`, which piles another candle into the same block. */
    @Test
    fun candlesStackIntoTheOneAlreadyThere() {
        val two =
            checkNotNull(
                BlockStateDefinitions.stateIdOf(Identifier.vanilla("candle"), mapOf("candles" to "2")),
            )
        assertEquals("3", placed("candle", replacing = two)["candles"])
        assertEquals("1", placed("candle")["candles"], "and the first one is just one")
    }

    /** parity: `RotatedPillarBlock`, whose axis is the axis of the face that was clicked. */
    @Test
    fun aLogLiesAlongTheFaceItWasPutOn() {
        assertEquals("y", placed("oak_log", face = Direction.UP)["axis"])
        assertEquals("x", placed("oak_log", face = Direction.EAST)["axis"])
        assertEquals("z", placed("oak_log", face = Direction.NORTH)["axis"])
    }

    /** parity: `AbstractFurnaceBlock`, facing back at whoever placed it. */
    @Test
    fun aFurnaceLooksBackAtThePlayer() {
        // Yaw 0 is looking south, so the front of the furnace looks north, back at the player.
        assertEquals("north", placed("furnace", yaw = 0f)["facing"])
        assertEquals("west", placed("furnace", yaw = -90f)["facing"])
        assertEquals("south", placed("furnace", yaw = 180f)["facing"])
    }

    /** parity: `StairBlock`, which faces the same way as the player rather than back at them. */
    @Test
    fun aStaircaseFacesTheSameWayAsThePlayer() {
        val downhill = placed("oak_stairs", yaw = 0f)
        assertEquals("south", downhill["facing"])
        assertEquals("bottom", downhill["half"])
        assertEquals("top", placed("oak_stairs", face = Direction.DOWN)["half"])
        assertEquals("top", placed("oak_stairs", face = Direction.NORTH, clickY = 0.8f)["half"])
    }

    @Test
    fun aSlabGoesWhereTheCursorWas() {
        assertEquals("bottom", placed("stone_slab", face = Direction.UP)["type"])
        assertEquals("top", placed("stone_slab", face = Direction.DOWN)["type"])
        assertEquals("bottom", placed("stone_slab", face = Direction.WEST, clickY = 0.2f)["type"])
        assertEquals("top", placed("stone_slab", face = Direction.WEST, clickY = 0.9f)["type"])
    }

    /** parity: `SlabBlock`, where a second slab in the same spot makes a double. */
    @Test
    fun aSlabOnASlabIsADoubleSlab() {
        val bottom = checkNotNull(Blocks.REGISTRY.idOf(Identifier.vanilla("stone_slab")))
        assertEquals("double", placed("stone_slab", replacing = bottom)["type"])
    }

    /** parity: `DispenserBlock`, pointing away from wherever the player is looking. */
    @Test
    fun aDispenserPointsAwayFromTheEyes() {
        assertEquals("north", placed("dispenser", yaw = 0f, pitch = 0f)["facing"])
        assertEquals("up", placed("dispenser", pitch = 90f)["facing"])
        assertEquals("down", placed("dispenser", pitch = -90f)["facing"])
    }

    /** parity: `LightningRodBlock`, which points out of the face it was stuck to. */
    @Test
    fun aLightningRodPointsOutOfTheFaceItWasClicked() {
        assertEquals("up", placed("lightning_rod", face = Direction.UP)["facing"])
        assertEquals("west", placed("lightning_rod", face = Direction.WEST)["facing"])
    }

    /** parity: `FaceAttachedHorizontalDirectionalBlock`: what it hangs on is the face that was clicked. */
    @Test
    fun aLeverHangsOnWhateverWasClicked() {
        val onFloor = placed("lever", face = Direction.UP, yaw = 0f)
        assertEquals("floor", onFloor["face"])
        assertEquals("south", onFloor["facing"], "on the floor it looks the way the player does")

        val onWall = placed("lever", face = Direction.WEST)
        assertEquals("wall", onWall["face"])
        assertEquals("west", onWall["facing"], "on a wall it looks out of it")

        assertEquals("ceiling", placed("lever", face = Direction.DOWN)["face"])
    }

    /** parity: `AnvilBlock`, the one that lands across the player rather than facing them. */
    @Test
    fun anAnvilLandsSideways() {
        assertEquals("west", placed("anvil", yaw = 0f)["facing"])
    }

    /** Checked against the real classes: an amethyst bud grows out of the face it was put on. */
    @Test
    fun anAmethystBudPointsOutOfItsFace() {
        assertEquals("up", placed("small_amethyst_bud", face = Direction.UP)["facing"])
        assertEquals("north", placed("large_amethyst_bud", face = Direction.NORTH)["facing"])
    }

    /** parity: `DoorBlock`, which looks the same way the player does. */
    @Test
    fun aDoorFacesTheSameWayAsThePlayer() {
        assertEquals("south", placed("oak_door", yaw = 0f)["facing"])
    }

    /** The complaint from the screenshots: a block went down inside the player and walled them in. */
    @Test
    fun aBlockDoesNotGoDownInsideAPlayer() {
        val standing = BlockPlacement.playerBox(x = 0.5, y = 64.0, z = 0.5, sneaking = false)

        assertTrue(BlockPlacement.isObstructed(Blocks.STONE.stateId, 0, 64, 0, listOf(standing)), "at the feet")
        assertTrue(BlockPlacement.isObstructed(Blocks.STONE.stateId, 0, 65, 0, listOf(standing)), "at the head")
        assertFalse(BlockPlacement.isObstructed(Blocks.STONE.stateId, 0, 66, 0, listOf(standing)), "above the head")
        assertFalse(BlockPlacement.isObstructed(Blocks.STONE.stateId, 1, 64, 0, listOf(standing)), "beside them")
    }

    /** parity: `Shapes.joinIsNotEmpty` with AND: touching is not overlapping. */
    @Test
    fun theBlockUnderfootIsNotInTheWay() {
        val standing = BlockPlacement.playerBox(x = 0.5, y = 64.0, z = 0.5, sneaking = false)
        assertFalse(BlockPlacement.isObstructed(Blocks.STONE.stateId, 0, 63, 0, listOf(standing)))
    }

    /** parity: 1.8 tall standing and 1.5 crouching, which is a real quarter of a block of room. */
    @Test
    fun crouchingIsShorterThanStanding() {
        val topSlab = checkNotNull(Blocks.REGISTRY.idOf(Identifier.vanilla("stone_slab"))) - 2
        val standing = BlockPlacement.playerBox(x = 0.5, y = 64.0, z = 0.5, sneaking = false)
        val crouching = BlockPlacement.playerBox(x = 0.5, y = 64.0, z = 0.5, sneaking = true)

        // The upside-down slab fills 65.5 to 66.0: the standing head reaches it, the crouching one does not.
        assertTrue(BlockPlacement.isObstructed(topSlab, 0, 65, 0, listOf(standing)))
        assertFalse(BlockPlacement.isObstructed(topSlab, 0, 65, 0, listOf(crouching)))
    }

    /** parity: an empty collision shape never obstructs anyone, so a torch goes down inside you. */
    @Test
    fun somethingWithNoCollisionIsNeverInTheWay() {
        val torch = checkNotNull(Blocks.REGISTRY.idOf(Identifier.vanilla("torch")))
        val standing = BlockPlacement.playerBox(x = 0.5, y = 64.0, z = 0.5, sneaking = false)
        assertFalse(BlockPlacement.isObstructed(torch, 0, 64, 0, listOf(standing)))
    }

    /** A bottom slab at the feet of someone standing on the ground does overlap them. */
    @Test
    fun aSlabAtTheFeetIsStillInTheWay() {
        val slab = checkNotNull(Blocks.REGISTRY.idOf(Identifier.vanilla("stone_slab")))
        val standing = BlockPlacement.playerBox(x = 0.5, y = 64.0, z = 0.5, sneaking = false)
        assertTrue(BlockPlacement.isObstructed(slab, 0, 64, 0, listOf(standing)))
        // The top half of the same block is not: a bottom slab only fills the lower half.
        val onSlab = BlockPlacement.playerBox(x = 0.5, y = 64.5, z = 0.5, sneaking = false)
        assertFalse(BlockPlacement.isObstructed(slab, 0, 64, 0, listOf(onSlab)))
    }

    @Test
    fun aBlockWithNothingToOrientIsPlacedAsItIs() {
        val stone = Blocks.STONE.stateId
        assertEquals(
            stone,
            BlockPlacement.stateFor(
                Identifier.vanilla("stone"),
                stone,
                Direction.UP,
                clickY = 0f,
                yaw = 0f,
                pitch = 0f,
                replacing = Blocks.AIR.stateId,
            ),
        )
    }
}
