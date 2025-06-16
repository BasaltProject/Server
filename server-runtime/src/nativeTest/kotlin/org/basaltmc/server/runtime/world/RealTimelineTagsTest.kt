package org.basaltmc.server.runtime.world

import org.basaltmc.wire.common.Identifier
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

class RealTimelineTagsTest {
    /**
     * Without the jar there are no timelines, and the caller has to be able to tell: an empty
     * `in_overworld` is exactly what leaves the sky frozen at midday.
     */
    @Test
    fun withoutTheJarNothingIsTagged() {
        val tags = RealTimelineTags(serverJar = null)

        assertEquals(0, tags.size)
        assertTrue(tags.membersOf(Identifier.vanilla("in_overworld")).isEmpty())
    }
}
