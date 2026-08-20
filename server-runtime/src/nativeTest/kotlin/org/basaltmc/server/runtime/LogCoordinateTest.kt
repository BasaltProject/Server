package org.basaltmc.server.runtime

import kotlin.math.abs
import kotlin.math.roundToLong
import kotlin.test.Test
import kotlin.test.assertEquals

/** The login line rounds to a tenth: nobody reads seventeen decimals off a log. */
class LogCoordinateTest {
    private fun oneDecimal(value: Double): String {
        // The sign is kept apart from the digits: dividing a negative tenth by ten is zero, so
        // anything between -1 and 0 came out looking positive.
        val tenths = (abs(value) * 10.0).roundToLong()
        val sign = if (value < 0.0 && tenths != 0L) "-" else ""
        return "$sign${tenths / 10}.${tenths % 10}"
    }

    @Test
    fun aCoordinateIsWrittenToOneDecimal() {
        assertEquals("25.7", oneDecimal(25.655916544114223))
        assertEquals("67.0", oneDecimal(66.95287145626008))
        assertEquals("268.9", oneDecimal(268.8903158457127))
        assertEquals("8.5", oneDecimal(8.5))
        assertEquals("-60.0", oneDecimal(-60.0))
    }

    @Test
    fun aNegativeCoordinateKeepsItsSignAndItsTenth() {
        assertEquals("-12.3", oneDecimal(-12.34))
        assertEquals("-0.5", oneDecimal(-0.45))
    }
}
