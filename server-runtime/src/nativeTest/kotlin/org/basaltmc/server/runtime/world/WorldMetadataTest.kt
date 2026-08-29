package org.basaltmc.server.runtime.world

import org.basaltmc.wire.io.FileIO
import kotlin.test.AfterTest
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNotNull

/** Durable round trips for level-wide state that must survive a server restart. */
class WorldMetadataTest {
    private val root = "build/test-worlds/world_metadata_test"

    @AfterTest
    fun cleanUp() {
        FileIO.delete(WorldMetadata.pathFor(root))
        FileIO.delete(root)
    }

    @Test
    fun preservesWanderingTraderSavedData() {
        WorldMetadata(
            seed = 42L,
            spawnX = 8,
            spawnY = 64,
            spawnZ = 8,
            wanderingTraderSpawnDelay = 7_200,
            wanderingTraderSpawnChance = 75,
        ).writeTo(root)

        val restored = assertNotNull(WorldMetadata.read(root))
        assertEquals(7_200, restored.wanderingTraderSpawnDelay)
        assertEquals(75, restored.wanderingTraderSpawnChance)
    }
}
