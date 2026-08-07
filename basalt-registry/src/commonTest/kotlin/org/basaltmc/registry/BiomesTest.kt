package org.basaltmc.registry

import kotlin.test.Test
import kotlin.test.assertEquals

class BiomesTest {
    @Test
    fun plainsIsRegisteredAtItsVanillaId() {
        assertEquals(Identifier.vanilla("plains"), Biomes.PLAINS.id)
        assertEquals(Biomes.PLAINS, Biomes.REGISTRY.byId(40))
    }
}
