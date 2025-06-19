package org.basaltmc.server.runtime.world

import org.basaltmc.server.random.LegacyRandom
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
import kotlin.test.assertTrue

class BlockGrowthTest {
    private val world =
        LoadedWorld(
            WorldDeclaration("growth", WorldDimension.OVERWORLD, WorldType.VOID, seed = 0L, isDefault = true),
            WorldSettings.VANILLA,
            WorldMetadata(0L, 0, 64, 0),
            "build/test-data/growth",
            Logger("GrowthTest").apply { minLevel = LogLevel.ERROR },
            1,
        )

    private val ticks = RandomTicks()

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

    private fun pathAt(
        x: Int,
        y: Int,
        z: Int,
    ): String? = BlockStateDefinitions.blockOf(world.state.getBlockState(x, y, z) ?: 0)?.path

    private fun propertyAt(
        x: Int,
        y: Int,
        z: Int,
        property: String,
    ): String? = BlockStateDefinitions.propertiesOf(world.state.getBlockState(x, y, z) ?: 0)[property]

    private fun run(
        x: Int,
        y: Int,
        z: Int,
        rounds: Int = 2000,
        until: () -> Boolean,
    ): Boolean {
        val random = LegacyRandom(11L)
        repeat(rounds) {
            ticks.tickBlock(world, x, y, z, random)
            if (until()) return true
        }
        return until()
    }

    /** parity: `RedStoneOreBlock.randomTick`, the one thing it does. */
    @Test
    fun litRedstoneOreGoesDark() {
        world.state.setBlockState(1, 70, 1, state("redstone_ore", mapOf("lit" to "true")))

        assertTrue(run(1, 70, 1, rounds = 20) { propertyAt(1, 70, 1, "lit") == "false" })
    }

    /** parity: `NyliumBlock.randomTick`: covered nylium reverts to netherrack. */
    @Test
    fun coveredNyliumRevertsToNetherrack() {
        world.state.setBlockState(3, 70, 1, state("crimson_nylium"))
        world.state.setBlockState(3, 71, 1, Blocks.STONE.stateId)

        assertTrue(run(3, 70, 1, rounds = 20) { pathAt(3, 70, 1) == "netherrack" })
    }

    @Test
    fun cocoaAndBerriesRipen() {
        world.state.setBlockState(5, 70, 1, state("cocoa", mapOf("age" to "0", "facing" to "north")))
        assertTrue(run(5, 70, 1) { propertyAt(5, 70, 1, "age") == "2" }, "cocoa should ripen")

        world.state.setBlockState(7, 70, 1, state("sweet_berry_bush", mapOf("age" to "0")))
        assertTrue(run(7, 70, 1) { propertyAt(7, 70, 1, "age") == "3" }, "berries should ripen")
    }

    /** parity: `StemBlock.randomTick`, which grows to seven and then puts a fruit on one side. */
    @Test
    fun aFullStemPutsOutAPumpkin() {
        world.state.setBlockState(9, 70, 5, state("farmland", mapOf("moisture" to "7")))
        world.state.setBlockState(9, 71, 5, state("pumpkin_stem", mapOf("age" to "7")))
        for (offset in listOf(-1, 1)) {
            world.state.setBlockState(9 + offset, 70, 5, Blocks.DIRT.stateId)
            world.state.setBlockState(9, 70, 5 + offset, Blocks.DIRT.stateId)
        }

        assertTrue(
            run(9, 71, 5) { pathAt(9, 71, 5) == "attached_pumpkin_stem" },
            "the stem should have attached itself to a pumpkin",
        )
        val fruit =
            listOf(
                pathAt(8, 71, 5),
                pathAt(10, 71, 5),
                pathAt(9, 71, 4),
                pathAt(9, 71, 6),
            )
        assertTrue(fruit.any { it == "pumpkin" }, "and a pumpkin should be beside it, got $fruit")
    }

    /** parity: `BambooStalkBlock.growBamboo`, which climbs and carries its leaves down with it. */
    @Test
    fun bambooClimbs() {
        world.state.setBlockState(12, 70, 5, Blocks.DIRT.stateId)
        world.state.setBlockState(12, 71, 5, state("bamboo", mapOf("age" to "0", "leaves" to "none", "stage" to "0")))

        assertTrue(
            run(12, 71, 5, rounds = 200) { pathAt(12, 72, 5) == "bamboo" },
            "it should have grown a second block",
        )
        assertEquals("small", propertyAt(12, 72, 5, "leaves"), "the new top carries small leaves")
    }

    @Test
    fun aBambooSaplingBecomesAStalk() {
        world.state.setBlockState(14, 70, 5, Blocks.DIRT.stateId)
        world.state.setBlockState(14, 71, 5, state("bamboo_sapling"))

        assertTrue(run(14, 71, 5, rounds = 200) { pathAt(14, 72, 5) == "bamboo" })
    }

    /** parity: `ChangeOverTimeBlock`, which walks copper through its four stages. */
    @Test
    fun copperWeathersOneStageAtATime() {
        world.state.setBlockState(1, 80, 1, state("copper_block"))

        assertTrue(run(1, 80, 1, rounds = 20000) { pathAt(1, 80, 1) == "exposed_copper" }, "it should have exposed")
        assertTrue(
            run(1, 80, 1, rounds = 20000) { pathAt(1, 80, 1) == "weathered_copper" },
            "and then weathered",
        )
    }

    /** parity: `GrowingPlantHeadBlock.randomTick`: the head climbs and leaves a body behind it. */
    @Test
    fun twistingVinesClimbAndLeaveAStem() {
        world.state.setBlockState(4, 80, 1, Blocks.STONE.stateId)
        world.state.setBlockState(4, 81, 1, state("twisting_vines", mapOf("age" to "0")))

        assertTrue(run(4, 81, 1, rounds = 500) { pathAt(4, 82, 1) == "twisting_vines" }, "it should have climbed")
        assertEquals("twisting_vines_plant", pathAt(4, 81, 1), "and left its stem behind")
    }

    /** parity: `BuddingAmethystBlock.randomTick`, one bud at a time on one of its six faces. */
    @Test
    fun buddingAmethystGrowsBuds() {
        world.state.setBlockState(7, 80, 1, state("budding_amethyst"))

        assertTrue(
            run(7, 80, 1, rounds = 2000) {
                listOf(
                    pathAt(6, 80, 1),
                    pathAt(8, 80, 1),
                    pathAt(7, 79, 1),
                    pathAt(7, 81, 1),
                    pathAt(7, 80, 0),
                    pathAt(7, 80, 2),
                ).any { it == "small_amethyst_bud" }
            },
            "a bud should have appeared on one of its faces",
        )
    }

    /** parity: `TurtleEggBlock.randomTick`, whose two cracks come before any turtle. */
    @Test
    fun aTurtleEggCracksTwiceAndThenGoes() {
        world.state.setBlockState(10, 80, 1, state("sand"))
        world.state.setBlockState(10, 81, 1, state("turtle_egg", mapOf("eggs" to "1", "hatch" to "0")))

        // One turn in five hundred does anything at all, so this needs a long run rather than a
        // few turns: that odds figure is the attribute default and is what makes eggs take a night.
        assertTrue(
            run(10, 81, 1, rounds = 20000) { propertyAt(10, 81, 1, "hatch") == "2" },
            "it should have cracked twice",
        )
        assertTrue(
            run(10, 81, 1, rounds = 20000) { world.state.getBlockState(10, 81, 1) == Blocks.AIR.stateId },
            "and then hatched away",
        )
    }

    /** parity: `ChorusFlowerBlock.randomTick`, which climbs off end stone and dies where it cannot. */
    @Test
    fun aChorusFlowerClimbsOffEndStone() {
        world.state.setBlockState(13, 80, 5, state("end_stone"))
        world.state.setBlockState(13, 81, 5, state("chorus_flower", mapOf("age" to "0")))

        assertTrue(
            run(13, 81, 5, rounds = 200) { pathAt(13, 82, 5) == "chorus_flower" },
            "it should have grown upwards",
        )
        assertEquals("chorus_plant", pathAt(13, 81, 5), "leaving a stem where it was")
    }

    @Test
    fun aChorusFlowerHemmedInDies() {
        world.state.setBlockState(1, 84, 5, state("end_stone"))
        world.state.setBlockState(1, 85, 5, state("chorus_flower", mapOf("age" to "4")))
        // Walled in above, so it can neither climb nor branch.
        world.state.setBlockState(1, 87, 5, Blocks.STONE.stateId)
        for (offset in listOf(-1, 1)) {
            world.state.setBlockState(1 + offset, 85, 5, Blocks.STONE.stateId)
            world.state.setBlockState(1, 85, 5 + offset, Blocks.STONE.stateId)
        }

        assertTrue(
            run(1, 85, 5, rounds = 200) { propertyAt(1, 85, 5, "age") == "5" },
            "a flower with nowhere to go should have died",
        )
    }

    /** parity: `VineBlock.randomTick`, which takes a face it can hold on to. */
    @Test
    fun aVineTakesTheWallBesideIt() {
        world.state.setBlockState(4, 89, 5, Blocks.STONE.stateId)
        // The ceiling it should grab: `canSupportAtFace(UP)` asks what is directly above the vine.
        world.state.setBlockState(4, 90, 6, Blocks.STONE.stateId)
        world.state.setBlockState(4, 89, 6, state("vine", mapOf("north" to "true", "up" to "false")))

        assertTrue(
            run(4, 89, 6, rounds = 500) { propertyAt(4, 89, 6, "up") == "true" },
            "it should have taken the stone above it",
        )
    }

    /** parity: `SpeleothemBlock`, which lengthens a stalactite or drips a stalagmite up to meet it. */
    @Test
    fun aStalactiteUnderDripstoneGrows() {
        world.state.setBlockState(7, 92, 5, state("dripstone_block"))
        world.state.setBlockState(
            7,
            91,
            5,
            state(
                "pointed_dripstone",
                mapOf("vertical_direction" to "down", "thickness" to "tip", "waterlogged" to "false"),
            ),
        )
        world.state.setBlockState(7, 85, 5, Blocks.STONE.stateId)

        assertTrue(
            run(7, 91, 5, rounds = 20000) {
                pathAt(7, 90, 5) == "pointed_dripstone" || pathAt(7, 86, 5) == "pointed_dripstone"
            },
            "it should have grown down or dripped a stalagmite up from the floor",
        )
    }

    /** parity: `SulfurSpikeBlock`, the same speleothem on sulfur and reaching two instead of seven. */
    @Test
    fun aSulfurSpikeGrowsTheSameWay() {
        world.state.setBlockState(9, 92, 5, state("sulfur"))
        world.state.setBlockState(
            9,
            91,
            5,
            state(
                "sulfur_spike",
                mapOf("vertical_direction" to "down", "thickness" to "tip", "waterlogged" to "false"),
            ),
        )
        world.state.setBlockState(9, 85, 5, Blocks.STONE.stateId)

        assertTrue(
            run(9, 91, 5, rounds = 20000) {
                pathAt(9, 90, 5) == "sulfur_spike" || pathAt(9, 86, 5) == "sulfur_spike"
            },
            "it should have grown down or dripped a spike up from the floor",
        )
    }

    /**
     * parity: the `day` timeline's two keyframes for `gameplay/eyeblossom_open`: open from 12600,
     * closed from 23401. The attribute has exactly one source in vanilla's data, and it is the
     * clock this build already keeps.
     */
    @Test
    fun anEyeblossomOpensAtDuskAndClosesAtDawn() {
        world.state.setBlockState(1, 92, 8, state("closed_eyeblossom"))
        world.clock.totalTicks = 13000

        assertTrue(run(1, 92, 8, rounds = 10) { pathAt(1, 92, 8) == "open_eyeblossom" }, "it should open at night")

        world.clock.totalTicks = 1000
        assertTrue(
            run(1, 92, 8, rounds = 10) { pathAt(1, 92, 8) == "closed_eyeblossom" },
            "and close again by day",
        )
    }

    /**
     * parity: `NetherPortalBlock.randomTick`. The piglin stands still because nothing has a brain
     * yet; what is under test is the block's own rule.
     */
    @Test
    fun aPortalSpawnsAPiglinInTheOverworld() {
        world.state.setBlockState(4, 92, 8, Blocks.STONE.stateId)
        world.state.setBlockState(4, 93, 8, state("nether_portal", mapOf("axis" to "x")))

        var spawned: org.basaltmc.server.runtime.entity.ServerEntity? = null
        val random = LegacyRandom(4L)
        repeat(20000) {
            if (spawned == null) {
                spawned = BlockGrowth.netherPortal(world, 4, 93, 8, random) { 1 }
            }
        }

        val piglin = checkNotNull(spawned) { "the portal should have spawned one" }
        assertEquals(Identifier.vanilla("zombified_piglin"), piglin.type)
        assertEquals(93.0, piglin.y, "on top of the floor under the portal")
    }

    /** parity: `MushroomBlock.randomTick`, which will not crowd more than five into its box. */
    @Test
    fun mushroomsSpreadButNotIntoACrowd() {
        for (dx in 0..4) {
            world.state.setBlockState(1 + dx, 74, 8, Blocks.DIRT.stateId)
            world.state.setBlockState(1 + dx, 75, 8, state("brown_mushroom"))
        }
        val before = (0..4).count { pathAt(1 + it, 75, 8) == "brown_mushroom" }

        run(1, 75, 8, rounds = 500) { false }

        // Five of them already, so the sixth never appears however long it ticks.
        assertEquals(before, (0..4).count { pathAt(1 + it, 75, 8) == "brown_mushroom" })
    }
}
