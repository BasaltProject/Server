package org.basaltmc.common.logging

import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertTrue

class LoggerTest {
    @Test
    fun suppressesMessagesBelowMinLevel() {
        val logger = Logger("test")
        logger.minLevel = LogLevel.WARN
        var evaluated = false

        logger.info {
            evaluated = true
            "should not run"
        }

        assertFalse(evaluated)
    }

    @Test
    fun evaluatesMessagesAtOrAboveMinLevel() {
        val logger = Logger("test")
        logger.minLevel = LogLevel.DEBUG
        var evaluated = false

        logger.warn {
            evaluated = true
            "should run"
        }

        assertTrue(evaluated)
    }

    @Test
    fun defaultMinLevelIsInfo() {
        assertEquals(LogLevel.INFO, Logger("test").minLevel)
    }
}
