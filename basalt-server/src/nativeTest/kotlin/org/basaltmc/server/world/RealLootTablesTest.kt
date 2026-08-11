package org.basaltmc.server.world

import org.basaltmc.common.Identifier
import kotlin.test.Test
import kotlin.test.assertNull

class RealLootTablesTest {
    @Test
    fun aNullServerJarMeansEveryBlockBreaksEmptyHanded() {
        val lootTables = RealLootTables(serverJar = null)

        assertNull(lootTables.forBlock(Identifier.vanilla("stone")))
        // Asked twice, so the cache-vs-"never looked up" branch is exercised too.
        assertNull(lootTables.forBlock(Identifier.vanilla("stone")))
    }
}
