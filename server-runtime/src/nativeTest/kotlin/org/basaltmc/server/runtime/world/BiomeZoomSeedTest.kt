package org.basaltmc.server.runtime.world

import kotlin.test.Test
import kotlin.test.assertEquals

class BiomeZoomSeedTest {
    @Test
    fun matchesOfficialSeedHashVectors() {
        assertEquals(8794265229978523055L, obfuscateBiomeSeed(0L))
        assertEquals(-6467378160175308932L, obfuscateBiomeSeed(1L))
        assertEquals(7890956078138091352L, obfuscateBiomeSeed(1234567890123456789L))
    }
}
