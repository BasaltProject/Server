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
import kotlin.test.assertFalse
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlin.test.assertTrue

class BlockInteractionsTest {
    private val world =
        LoadedWorld(
            declaration =
                WorldDeclaration(
                    "doors",
                    WorldDimension.OVERWORLD,
                    WorldType.VOID,
                    seed = 0L,
                    isDefault = true,
                ),
            settings = WorldSettings.VANILLA,
            metadata = WorldMetadata(0L, 0, 64, 0),
            worldsRoot = "build/test-data/interactions",
            logger = Logger("InteractionsTest").apply { minLevel = LogLevel.ERROR },
            generationWorkers = 1,
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
        properties: Map<String, String>,
    ): Int = checkNotNull(BlockStateDefinitions.stateIdOf(Identifier.vanilla(name), properties))

    private fun propertiesAt(
        x: Int,
        y: Int,
        z: Int,
    ) = BlockStateDefinitions.propertiesOf(checkNotNull(world.state.getBlockState(x, y, z)))

    @Test
    fun aDoorOpensBothHalvesAtOnce() {
        val lower = state("oak_door", mapOf("half" to "lower", "open" to "false", "facing" to "north"))
        val upper = state("oak_door", mapOf("half" to "upper", "open" to "false", "facing" to "north"))
        world.state.setBlockState(1, 64, 1, lower)
        world.state.setBlockState(1, 65, 1, upper)

        val toggled = assertNotNull(BlockInteractions.toggle(world, 1, 64, 1))

        assertEquals(2, toggled.changes.size, "both halves should move")
        assertEquals("true", propertiesAt(1, 64, 1)["open"])
        assertEquals("true", propertiesAt(1, 65, 1)["open"])
        assertEquals("upper", propertiesAt(1, 65, 1)["half"], "the halves should not swap")
        assertEquals("minecraft:block.wooden_door.open", toggled.sound)

        // And closing it again says so.
        val closed = assertNotNull(BlockInteractions.toggle(world, 1, 65, 1))
        assertEquals("false", propertiesAt(1, 64, 1)["open"])
        assertEquals("minecraft:block.wooden_door.close", closed.sound)
    }

    /** parity: `BlockSetType.canOpenByHand`, false for iron and nothing else. */
    @Test
    fun anIronDoorDoesNotMoveForAHand() {
        val iron = state("iron_door", mapOf("half" to "lower", "open" to "false", "facing" to "north"))
        world.state.setBlockState(2, 64, 1, iron)

        assertFalse(BlockInteractions.isToggleable(iron))
        assertNull(BlockInteractions.toggle(world, 2, 64, 1))
    }

    @Test
    fun aLeverFlipsItsOwnPower() {
        val lever = state("lever", mapOf("powered" to "false", "face" to "floor", "facing" to "north"))
        world.state.setBlockState(3, 64, 1, lever)

        val toggled = assertNotNull(BlockInteractions.toggle(world, 3, 64, 1))

        assertEquals("true", propertiesAt(3, 64, 1)["powered"])
        assertEquals("minecraft:block.lever.click", toggled.sound)
    }

    @Test
    fun aFenceGateHasItsOwnSoundName() {
        val gate = state("oak_fence_gate", mapOf("open" to "false", "facing" to "north"))
        world.state.setBlockState(4, 64, 1, gate)

        assertEquals("minecraft:block.fence_gate.open", assertNotNull(BlockInteractions.toggle(world, 4, 64, 1)).sound)
    }

    @Test
    fun bambooHasItsOwnSoundSet() {
        val trapdoor = state("bamboo_trapdoor", mapOf("open" to "false", "facing" to "north", "half" to "bottom"))
        world.state.setBlockState(5, 64, 1, trapdoor)

        assertEquals(
            "minecraft:block.bamboo_wood_trapdoor.open",
            assertNotNull(BlockInteractions.toggle(world, 5, 64, 1)).sound,
        )
    }

    @Test
    fun anOrdinaryBlockDoesNothing() {
        assertFalse(BlockInteractions.isToggleable(Blocks.STONE.stateId))
    }

    @Test
    fun everyWoodenDoorCanBeOpened() {
        for (wood in listOf("oak", "spruce", "birch", "jungle", "acacia", "dark_oak", "mangrove", "cherry", "bamboo")) {
            val door = state("${wood}_door", mapOf("half" to "lower", "open" to "false", "facing" to "north"))
            assertTrue(BlockInteractions.isToggleable(door), "$wood doors should open")
        }
    }
}
