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
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlin.test.assertTrue

class EndPodiumTest {
    private val end =
        LoadedWorld(
            declaration = WorldDeclaration("podium", WorldDimension.END, WorldType.VOID, seed = 0L, isDefault = true),
            settings = WorldSettings.VANILLA,
            metadata = WorldMetadata(0L, 0, 64, 0),
            worldsRoot = "build/test-data/podium",
            logger = Logger("PodiumTest").apply { minLevel = LogLevel.ERROR },
            generationWorkers = 1,
        )

    @AfterTest
    fun stopGenerating() {
        end.stopGenerating(drainTimeoutMillis = 100)
    }

    private fun stateAt(
        x: Int,
        y: Int,
        z: Int,
    ): Int = checkNotNull(end.state.getBlockState(x, y, z))

    /** parity: `EndPodiumFeature` with `active = false`, which is what a live dragon leaves. */
    @Test
    fun theClosedPodiumIsBedrockWithNoPortalInIt() {
        val origin = assertNotNull(EndPodium.ensure(end, active = false))
        val (x, y, z) = Triple(origin[0], origin[1], origin[2])

        assertEquals(Blocks.BEDROCK.stateId, stateAt(x, y, z), "the pillar should stand on bedrock")
        assertEquals(Blocks.BEDROCK.stateId, stateAt(x + 3, y, z), "the rim should be bedrock")
        assertEquals(Blocks.AIR.stateId, stateAt(x + 1, y, z), "there should be no portal yet")
        assertEquals(Blocks.BEDROCK.stateId, stateAt(x, y + 3, z), "the pillar is four blocks tall")

        val torch = checkNotNull(BlockStateDefinitions.propertiesOf(stateAt(x, y + 2, z - 1)))
        assertEquals("north", torch["facing"])
    }

    @Test
    fun theOpenPodiumHasTheExitPortalInIt() {
        val origin = assertNotNull(EndPodium.ensure(end, active = true))
        val portal = checkNotNull(Blocks.REGISTRY.idOf(Identifier.vanilla("end_portal")))

        assertEquals(portal, stateAt(origin[0] + 1, origin[1], origin[2]))
        assertEquals(Blocks.BEDROCK.stateId, stateAt(origin[0], origin[1], origin[2]), "the pillar is not a portal")
    }

    @Test
    fun itIsOnlyBuiltOnce() {
        assertNotNull(EndPodium.ensure(end, active = false))
        assertTrue(EndPodium.exists(end))
        assertNull(EndPodium.ensure(end, active = true), "it should not be rebuilt over itself")
    }
}
