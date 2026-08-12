package org.basaltmc.server.runtime.placeholder

import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNull

class DecimalsTest {
    @Test
    fun aSuffixOfDigitsIsReadAsAPrecision() {
        assertEquals("x" to 2, Decimals.split("x_2"))
        assertEquals("health" to 0, Decimals.split("health_0"))
    }

    @Test
    fun aNameThatIsNotAPrecisionIsLeftAlone() {
        // `health_rounded` is a placeholder of its own, and `x` has no suffix at all.
        assertNull(Decimals.split("health_rounded"))
        assertNull(Decimals.split("x"))
        assertNull(Decimals.split("_2"))
        assertNull(Decimals.split("x_"))
    }

    @Test
    fun aPositionReadsTheWayAScoreboardWantsIt() {
        // The number from the report: 21.70704236703265 is true and unreadable.
        assertEquals("21.71", Decimals.format(21.70704236703265, 2))
        assertEquals("21.7", Decimals.format(21.70704236703265, 1))
        assertEquals("22", Decimals.format(21.70704236703265, 0))
    }

    @Test
    fun theFractionKeepsItsLeadingZeros() {
        // Without padding this would read "69.5" for 69.05, which is a different number.
        assertEquals("69.05", Decimals.format(69.05, 2))
        assertEquals("69.00", Decimals.format(69.0, 2))
    }

    @Test
    fun negativeCoordinatesKeepTheirSign() {
        assertEquals("-13.62", Decimals.format(-13.61992083667093, 2))
        assertEquals("-14", Decimals.format(-13.61992083667093, 0))
    }

    @Test
    fun somethingRoundingToZeroIsNotWrittenAsMinusZero() {
        assertEquals("0.00", Decimals.format(-0.0001, 2))
    }
}
