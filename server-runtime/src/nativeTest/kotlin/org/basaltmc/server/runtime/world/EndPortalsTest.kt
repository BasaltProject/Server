package org.basaltmc.server.runtime.world

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
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlin.test.assertTrue

private val FRAME = Identifier.vanilla("end_portal_frame")
private val OBSIDIAN_STATE = checkNotNull(Blocks.REGISTRY.idOf(Identifier.vanilla("obsidian")))

private fun endWorld(
    name: String,
    dimension: WorldDimension,
): LoadedWorld =
    LoadedWorld(
        declaration =
            WorldDeclaration(
                name,
                dimension,
                WorldType.VOID,
                seed = 0L,
                isDefault = dimension == WorldDimension.OVERWORLD,
            ),
        settings = WorldSettings.VANILLA,
        metadata = WorldMetadata(0L, 0, 64, 0),
        worldsRoot = "build/test-data/end-$name",
        logger = Logger("EndPortalTest").apply { minLevel = LogLevel.ERROR },
        generationWorkers = 1,
    )

/** The twelve frames around (0, 64, 0), all facing the middle, with eyes wherever asked. */
private fun buildRing(
    world: LoadedWorld,
    withEyes: Boolean,
) {
    for (chunkX in -1..1) {
        for (chunkZ in -1..1) world.state.getOrGenerateChunk(chunkX, chunkZ)
    }
    val eye = if (withEyes) "true" else "false"

    fun place(
        x: Int,
        z: Int,
        facing: String,
    ) {
        val state = checkNotNull(Blocks.stateIdOf(FRAME, mapOf("eye" to eye, "facing" to facing)))
        world.state.setBlockState(x, 64, z, state)
    }
    for (offset in -1..1) {
        place(offset, -2, "south")
        place(offset, 2, "north")
        place(-2, offset, "east")
        place(2, offset, "west")
    }
}

class EndPortalsTest {
    private val overworld = endWorld("overworld", WorldDimension.OVERWORLD)
    private val end = endWorld("end", WorldDimension.END)

    @AfterTest
    fun stopGenerating() {
        overworld.stopGenerating(drainTimeoutMillis = 100)
        end.stopGenerating(drainTimeoutMillis = 100)
    }

    @Test
    fun theTwelfthEyeOpensThePortal() {
        buildRing(overworld, withEyes = true)
        // Take one eye back out, so placing it is what completes the ring.
        val empty = checkNotNull(Blocks.stateIdOf(FRAME, mapOf("eye" to "false", "facing" to "south")))
        overworld.state.setBlockState(0, 64, -2, empty)

        val changed = assertNotNull(EndPortals.placeEye(overworld, 0, 64, -2))

        // The frame that was armed, plus the nine blocks of portal.
        assertEquals(10, changed.size)
        for (x in -1..1) {
            for (z in -1..1) {
                assertTrue(
                    EndPortals.isEndPortal(checkNotNull(overworld.state.getBlockState(x, 64, z))),
                    "no portal at $x, $z",
                )
            }
        }
    }

    @Test
    fun anIncompleteRingOpensNothing() {
        buildRing(overworld, withEyes = false)

        val changed = assertNotNull(EndPortals.placeEye(overworld, 0, 64, -2))

        assertEquals(1, changed.size)
        assertNull(EndPortals.completeRing(overworld, 0, 64, -2))
        assertTrue(Blocks.isAir(checkNotNull(overworld.state.getBlockState(0, 64, 0))))
    }

    /** A ring whose frames look outward is not a portal. parity: the pattern checks every facing. */
    @Test
    fun aRingFacingTheWrongWayOpensNothing() {
        for (chunkX in -1..1) {
            for (chunkZ in -1..1) overworld.state.getOrGenerateChunk(chunkX, chunkZ)
        }
        val north = checkNotNull(Blocks.stateIdOf(FRAME, mapOf("eye" to "true", "facing" to "north")))
        for (offset in -1..1) {
            overworld.state.setBlockState(offset, 64, -2, north)
            overworld.state.setBlockState(offset, 64, 2, north)
            overworld.state.setBlockState(-2, 64, offset, north)
            overworld.state.setBlockState(2, 64, offset, north)
        }

        assertNull(EndPortals.completeRing(overworld, 0, 64, -2))
    }

    /** parity: `ServerLevel.END_SPAWN_POINT` and `EndPlatformFeature`: (100, 50, 0) on obsidian. */
    @Test
    fun goingToTheEndBuildsThePlatformItLandsOn() {
        val exit = EndPortals.exitFor(overworld, end)

        assertEquals(end.name, exit.world.name)
        assertEquals(100.5, exit.x)
        assertEquals(50.0, exit.y)
        assertEquals(0.5, exit.z)
        assertEquals(OBSIDIAN_STATE, end.state.getBlockState(100, 49, 0))
        assertEquals(OBSIDIAN_STATE, end.state.getBlockState(102, 49, 2))
        assertTrue(Blocks.isAir(checkNotNull(end.state.getBlockState(100, 50, 0))))
    }

    @Test
    fun comingBackLandsOnTheWorldSpawn() {
        val exit = EndPortals.exitFor(end, overworld)

        assertEquals(overworld.name, exit.world.name)
        assertEquals(overworld.spawn.first, exit.x)
        assertEquals(overworld.spawn.third, exit.z)
    }
}
