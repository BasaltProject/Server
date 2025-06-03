package org.basaltmc.server.world.noise

import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith

class NoiseParametersJsonTest {
    @Test
    fun parsesFirstOctaveAndAmplitudes() {
        val parameters = NoiseParametersJson.parse("""{"firstOctave": -7, "amplitudes": [1.0, 0.0, 1.0]}""")
        assertEquals(-7, parameters.firstOctave)
        assertContentEquals(doubleArrayOf(1.0, 0.0, 1.0), parameters.amplitudes)
    }

    @Test
    fun missingFirstOctaveThrows() {
        assertFailsWith<NoiseParametersParseException> {
            NoiseParametersJson.parse("""{"amplitudes": [1.0]}""")
        }
    }

    @Test
    fun missingAmplitudesThrows() {
        assertFailsWith<NoiseParametersParseException> {
            NoiseParametersJson.parse("""{"firstOctave": -7}""")
        }
    }
}
