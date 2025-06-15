package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.runtime.config.WorldDeclaration
import org.basaltmc.server.runtime.config.WorldDimension
import org.basaltmc.server.runtime.config.WorldSettings
import org.basaltmc.server.runtime.config.WorldType
import org.basaltmc.server.world.portal.PortalAxis
import org.basaltmc.server.world.portal.PortalShape
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.common.logging.LogLevel
import org.basaltmc.wire.common.logging.Logger
import org.basaltmc.wire.io.FileIO
import kotlin.math.abs
import kotlin.test.AfterTest
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlin.test.assertTrue

private val OBSIDIAN = checkNotNull(Blocks.REGISTRY.idOf(Identifier.vanilla("obsidian")))
private val FIRE = checkNotNull(Blocks.REGISTRY.idOf(Identifier.vanilla("fire")))

private fun world(
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
                isDefault =
                    dimension == WorldDimension.OVERWORLD,
            ),
        settings = WorldSettings.VANILLA,
        metadata = WorldMetadata(0L, 0, 64, 0),
        worldsRoot = "build/test-data/portals-$name",
        logger = Logger("PortalTest").apply { minLevel = LogLevel.ERROR },
        generationWorkers = 1,
    )

private fun deleteRecursively(path: String) {
    if (!FileIO.directoryExists(path)) return
    for (entry in FileIO.list(path)) {
        val child = "$path/$entry"
        if (FileIO.directoryExists(child)) deleteRecursively(child) else FileIO.delete(child)
    }
    FileIO.delete(path)
}

/** Builds the usual 2x3 frame with its bottom-left interior block at the given position. */
private fun buildFrame(
    world: LoadedWorld,
    axis: PortalAxis,
    x: Int,
    y: Int,
    z: Int,
) {
    for (chunkX in ((x - 2) shr 4)..((x + 2) shr 4)) {
        for (chunkZ in ((z - 2) shr 4)..((z + 2) shr 4)) world.state.getOrGenerateChunk(chunkX, chunkZ)
    }

    fun at(
        column: Int,
        row: Int,
    ): Triple<Int, Int, Int> = Triple(x + axis.rightX * column, y + row, z + axis.rightZ * column)
    for (column in -1..2) {
        val (bx, by, bz) = at(column, -1)
        world.state.setBlockState(bx, by, bz, OBSIDIAN)
        val (tx, ty, tz) = at(column, 3)
        world.state.setBlockState(tx, ty, tz, OBSIDIAN)
    }
    for (row in 0 until 3) {
        val (lx, ly, lz) = at(-1, row)
        world.state.setBlockState(lx, ly, lz, OBSIDIAN)
        val (rx, ry, rz) = at(2, row)
        world.state.setBlockState(rx, ry, rz, OBSIDIAN)
    }
}

/** Every portal block in the chunks this world has in memory, which is what "one portal" means here. */
private fun portalBlocksIn(world: LoadedWorld): Int {
    var count = 0
    for (chunk in world.state.chunks) {
        for (index in 0 until chunk.sectionCount) {
            val section = chunk.section(index)
            if (!section.mayContain { PortalShape.isPortal(it) }) continue
            for (y in 0 until 16) {
                for (z in 0 until 16) {
                    for (x in 0 until 16) {
                        if (PortalShape.isPortal(section.getBlockStateId(x, y, z))) count++
                    }
                }
            }
        }
    }
    return count
}

class NetherPortalsTest {
    private val overworld = world("overworld", WorldDimension.OVERWORLD)
    private val nether = world("nether", WorldDimension.NETHER)

    @AfterTest
    fun stopGenerating() {
        overworld.stopGenerating(drainTimeoutMillis = 100)
        nether.stopGenerating(drainTimeoutMillis = 100)
    }

    @Test
    fun lightingAFrameFillsItWithPortalBlocks() {
        buildFrame(overworld, PortalAxis.X, x = 0, y = 64, z = 0)

        val shape = assertNotNull(NetherPortals.light(overworld, 0, 64, 0))

        assertEquals(PortalAxis.X, shape.axis)
        assertEquals(6, shape.interior().size)
        for (block in shape.interior()) {
            assertTrue(PortalShape.isPortal(checkNotNull(overworld.state.getBlockState(block[0], block[1], block[2]))))
        }
    }

    @Test
    fun lightingSomethingThatIsNotAFrameOnlyLeavesFire() {
        overworld.state.getOrGenerateChunk(0, 0)

        assertNull(NetherPortals.light(overworld, 8, 64, 8))
        assertTrue(NetherPortals.placeFire(overworld, 8, 64, 8))
        assertEquals(FIRE, overworld.state.getBlockState(8, 64, 8))
    }

    /** parity: `DimensionType.getTeleportationScale`, the 1:8 between the Overworld and the Nether. */
    @Test
    fun goingToTheNetherDividesTheHorizontalPositionByEight() {
        val exit = assertNotNull(NetherPortals.exitFor(overworld, nether, 800.0, 64.0, -400.0, PortalAxis.X))

        assertEquals(nether.name, exit.world.name)
        assertTrue(abs(exit.x - 100.0) <= 17.0, "x was ${exit.x}")
        assertTrue(abs(exit.z + 50.0) <= 17.0, "z was ${exit.z}")
    }

    @Test
    fun comingBackMultipliesItByEight() {
        val exit = assertNotNull(NetherPortals.exitFor(nether, overworld, 100.0, 64.0, -50.0, PortalAxis.X))

        assertEquals(overworld.name, exit.world.name)
        assertTrue(abs(exit.x - 800.0) <= 17.0, "x was ${exit.x}")
        assertTrue(abs(exit.z + 400.0) <= 17.0, "z was ${exit.z}")
    }

    /** The gap `PortalIndex` closed: a portal whose chunk is gone used to be built all over again. */
    @Test
    fun aPortalIsStillFoundAfterAReload() {
        // Its own world, because this is the one test that writes to disk.
        deleteRecursively("build/test-data/portals-evicted")
        val built = world("evicted", WorldDimension.OVERWORLD)
        try {
            buildFrame(built, PortalAxis.X, x = 0, y = 64, z = 0)
            assertNotNull(NetherPortals.light(built, 0, 64, 0))
            built.saveDirty()
        } finally {
            built.stopGenerating(drainTimeoutMillis = 100)
        }

        // A second world on the same directory, with nothing in memory: only `portals.nbt` knows.
        val reloaded = world("evicted", WorldDimension.OVERWORLD)
        try {
            assertNull(reloaded.state.chunkIfLoaded(0, 0), "nothing should be loaded yet")
            val found = assertNotNull(NetherPortals.findClosestPortal(reloaded, 0, 64, 0, radius = 16))
            assertEquals(listOf(0, 64, 0), found.toList())
            assertNull(
                NetherPortals.findClosestLoadedPortal(reloaded, 0, 64, 0, radius = 16),
                "the chunk search should find nothing, which is the whole point of the index",
            )
        } finally {
            reloaded.stopGenerating(drainTimeoutMillis = 100)
        }
    }

    @Test
    fun breakingAPortalTakesItOutOfTheIndex() {
        buildFrame(overworld, PortalAxis.X, x = 0, y = 64, z = 0)
        assertNotNull(NetherPortals.light(overworld, 0, 64, 0))
        assertNotNull(NetherPortals.findClosestPortal(overworld, 0, 64, 0, radius = 16))

        // One block of the frame away, and the whole portal goes out.
        overworld.state.setBlockState(0, 63, 0, Blocks.AIR.stateId)
        NetherPortals.extinguishAround(overworld, 0, 63, 0)

        assertNull(NetherPortals.findClosestPortal(overworld, 0, 64, 0, radius = 16))
    }

    /** With no portal on the other side, one is built: the arrival block has to be a portal block. */
    @Test
    fun buildsAPortalWhereThereIsNone() {
        val exit = assertNotNull(NetherPortals.exitFor(overworld, nether, 0.0, 64.0, 0.0, PortalAxis.Z))

        val state = checkNotNull(nether.state.getBlockState(exit.x.toInt(), exit.y.toInt(), exit.z.toInt()))
        assertTrue(PortalShape.isPortal(state), "arrived on ${Blocks.byStateId(state)?.block}")
        assertEquals(PortalAxis.Z, PortalShape.axisOf(state))
    }

    /** parity: `NetherPortalBlock.updateShape`: break the frame and the whole portal goes out. */
    @Test
    fun breakingTheFrameputsThePortalOut() {
        buildFrame(overworld, PortalAxis.X, x = 0, y = 64, z = 0)
        assertNotNull(NetherPortals.light(overworld, 0, 64, 0))

        // One corner of the frame taken away, the way a player breaking obsidian does it.
        overworld.state.setBlockState(1, 64, 0, Blocks.AIR.stateId)
        val cleared = NetherPortals.extinguishAround(overworld, 1, 64, 0)

        assertEquals(6, cleared.size)
        assertEquals(0, portalBlocksIn(overworld))
    }

    @Test
    fun keepsAPortalWhoseFrameIsStillWhole() {
        buildFrame(overworld, PortalAxis.X, x = 0, y = 64, z = 0)
        assertNotNull(NetherPortals.light(overworld, 0, 64, 0))

        // A block two away from the frame: nothing to do with it.
        assertTrue(NetherPortals.extinguishAround(overworld, 4, 64, 4).isEmpty())
        assertEquals(6, portalBlocksIn(overworld))
    }

    @Test
    fun reusesAPortalThatIsAlreadyThere() {
        // A lit portal 10 blocks from where the scaled position lands.
        buildFrame(nether, PortalAxis.X, x = 10, y = 64, z = 0)
        assertNotNull(NetherPortals.light(nether, 10, 64, 0))

        val exit = assertNotNull(NetherPortals.exitFor(overworld, nether, 0.0, 64.0, 0.0, PortalAxis.X))

        // The nearest block of that portal, and no second portal built next to it.
        assertTrue(exit.x in 9.0..11.0, "x was ${exit.x}")
        assertEquals(64.0, exit.y)
        assertEquals(0.5, exit.z)
        assertEquals(6, portalBlocksIn(nether))
    }
}
