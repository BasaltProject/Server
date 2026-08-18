package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.BlockFace
import org.basaltmc.server.registry.BlockStateDefinitions
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
import kotlin.test.assertTrue

/** parity: `TripWireBlock.checkPressed` and `TripWireHookBlock.calculateState`. */
class TripwiresTest {
    private val world =
        LoadedWorld(
            WorldDeclaration("tripwire", WorldDimension.OVERWORLD, WorldType.VOID, seed = 0L, isDefault = true),
            WorldSettings.VANILLA,
            WorldMetadata(0L, 0, 64, 0),
            "build/test-data/tripwire",
            Logger("TripwiresTest").apply { minLevel = LogLevel.ERROR },
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

    private fun propertiesAt(
        x: Int,
        y: Int,
        z: Int,
    ): Map<String, String> = BlockStateDefinitions.propertiesOf(world.state.getBlockState(x, y, z) ?: 0)

    /** A line east along z=5: hooks at x=2 and x=6, string between them. */
    private fun lineOfString(y: Int = 70) {
        world.state.setBlockState(
            2,
            y,
            5,
            state("tripwire_hook", mapOf("facing" to "east", "attached" to "false", "powered" to "false")),
        )
        world.state.setBlockState(
            6,
            y,
            5,
            state("tripwire_hook", mapOf("facing" to "west", "attached" to "false", "powered" to "false")),
        )
        for (x in 3..5) {
            world.state.setBlockState(
                x,
                y,
                5,
                state(
                    "tripwire",
                    mapOf(
                        "attached" to "false",
                        "disarmed" to "false",
                        "powered" to "false",
                        "north" to "false",
                        "south" to "false",
                        "east" to "false",
                        "west" to "false",
                    ),
                ),
            )
        }
    }

    /** parity: `calculateState`: a hook with string reaching another hook is attached. */
    @Test
    fun twoHooksWithStringBetweenThemAttach() {
        lineOfString()
        val changes = mutableListOf<BlockChange>()

        Tripwires.calculateHook(world, 2, 70, 5, changes)

        assertEquals("true", propertiesAt(2, 70, 5)["attached"])
        assertEquals("true", propertiesAt(6, 70, 5)["attached"], "and so is the one at the other end")
        assertEquals("true", propertiesAt(4, 70, 5)["attached"], "the string is drawn taut")
    }

    /** parity: a broken line never attaches, so nothing it carries can power anything. */
    @Test
    fun aBrokenLineDoesNotAttach() {
        lineOfString()
        world.state.setBlockState(4, 70, 5, org.basaltmc.server.registry.Blocks.STONE.stateId)
        val changes = mutableListOf<BlockChange>()

        Tripwires.calculateHook(world, 2, 70, 5, changes)

        assertEquals("false", propertiesAt(2, 70, 5)["attached"])
    }

    /** parity: `checkPressed`, and the hook that reads it. */
    @Test
    fun steppingOnTheStringPowersTheHooks() {
        lineOfString()
        val changes = mutableListOf<BlockChange>()
        Tripwires.calculateHook(world, 2, 70, 5, changes)
        changes.clear()

        Tripwires.press(world, 4, 70, 5, occupied = true, changes = changes)

        assertEquals("true", propertiesAt(4, 70, 5)["powered"], "the string itself")
        assertEquals("true", propertiesAt(2, 70, 5)["powered"], "and both hooks")
        assertEquals("true", propertiesAt(6, 70, 5)["powered"])
        assertTrue(changes.isNotEmpty())
    }

    /** parity: a pressed wire books its own ten ticks, which is how it notices you left. */
    @Test
    fun aPressedWireBooksItsRecheck() {
        lineOfString()
        Tripwires.press(world, 4, 70, 5, occupied = true, changes = mutableListOf())

        val pending =
            world.state
                .chunkIfLoaded(0, 0)
                ?.scheduledBlockTicks()
                .orEmpty()
        assertTrue(
            pending.any { it.localX == 4 && it.y == 70 && it.type.path == "tripwire" && it.delay == 10 },
            "ten ticks: $pending",
        )
    }

    @Test
    fun steppingOffLetsItGo() {
        lineOfString()
        val changes = mutableListOf<BlockChange>()
        Tripwires.calculateHook(world, 2, 70, 5, changes)
        Tripwires.press(world, 4, 70, 5, occupied = true, changes = changes)

        Tripwires.press(world, 4, 70, 5, occupied = false, changes = changes)

        assertEquals("false", propertiesAt(4, 70, 5)["powered"])
        assertEquals("false", propertiesAt(2, 70, 5)["powered"])
    }

    /** parity: `DISARMED`, which is what shears leave behind: string that never fires again. */
    @Test
    fun aDisarmedWirePowersNothing() {
        lineOfString()
        world.state.setBlockState(
            4,
            70,
            5,
            state(
                "tripwire",
                mapOf(
                    "attached" to "true",
                    "disarmed" to "true",
                    "powered" to "true",
                    "north" to "false",
                    "south" to "false",
                    "east" to "false",
                    "west" to "false",
                ),
            ),
        )
        val changes = mutableListOf<BlockChange>()

        Tripwires.calculateHook(world, 2, 70, 5, changes)

        assertEquals("false", propertiesAt(2, 70, 5)["powered"])
    }

    /** parity: `TripWireHookBlock.getSignal`, which is what the redstone around it reads. */
    @Test
    fun aPoweredHookFeedsRedstone() {
        lineOfString()
        val changes = mutableListOf<BlockChange>()
        Tripwires.calculateHook(world, 2, 70, 5, changes)
        Tripwires.press(world, 4, 70, 5, occupied = true, changes = changes)

        val hook = checkNotNull(world.state.getBlockState(2, 70, 5))
        assertEquals(15, Redstone.signalOf(hook, BlockFace.NORTH))
        assertEquals(15, Redstone.directSignalOf(hook, BlockFace.EAST), "strongly into the block it faces")
    }

    @Test
    fun onlyWhatIsInsideTheBlockCounts() {
        assertTrue(Tripwires.isStandingOn(4, 70, 5, 4.5, 70.0, 5.5))
        assertFalse(Tripwires.isStandingOn(4, 70, 5, 4.5, 72.0, 5.5), "well above it")
        assertFalse(Tripwires.isStandingOn(4, 70, 5, 6.5, 70.0, 5.5), "and two blocks along")
    }
}
