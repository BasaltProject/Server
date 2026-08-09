package org.basaltmc.storage

import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertNull
import kotlin.test.assertTrue

class StatsNbtTest {
    @Test
    fun countersSurviveARoundTrip() {
        val stats =
            StoredStats.EMPTY
                .with(StoredStats.DEATHS, 3)
                .with(StoredStats.mined("minecraft:stone"), 128)
                .with(StoredStats.WALK_ONE_CM, 4_500_000)

        val decoded = StatsNbt.decode(StatsNbt.encode(stats)!!)

        assertEquals(3, decoded[StoredStats.DEATHS])
        assertEquals(128, decoded[StoredStats.mined("minecraft:stone")])
        assertEquals(4_500_000, decoded[StoredStats.WALK_ONE_CM])
    }

    @Test
    fun aPlayerWhoHasDoneNothingHasNoBlobAtAll() {
        // Null rather than an empty compound: every fresh player would otherwise carry one.
        assertNull(StatsNbt.encode(StoredStats.EMPTY))
    }

    @Test
    fun aCounterBackAtZeroIsNotWritten() {
        val stats = StoredStats(mapOf(StoredStats.DEATHS to 0L, StoredStats.JUMP to 2L))

        val decoded = StatsNbt.decode(StatsNbt.encode(stats)!!)

        assertTrue(StoredStats.DEATHS !in decoded.counters)
        assertEquals(2, decoded[StoredStats.JUMP])
    }

    @Test
    fun anUnknownCounterReadsAsZeroRatherThanFailing() {
        assertEquals(0, StoredStats.EMPTY["minecraft:custom/minecraft:never_touched"])
    }

    @Test
    fun addingToACounterKeepsWhatWasAlreadyThere() {
        val stats = StoredStats.EMPTY.with(StoredStats.DEATHS, 2).with(StoredStats.DEATHS, 3)

        assertEquals(5, stats[StoredStats.DEATHS])
    }

    @Test
    fun keysAreSpelledTheWayVanillaSpellsThem() {
        // The same strings vanilla writes into stats/<uuid>.json, so an import never has to
        // translate a name it might not have one for.
        assertEquals("minecraft:custom/minecraft:deaths", StoredStats.DEATHS)
        assertEquals("minecraft:mined/minecraft:stone", StoredStats.mined("minecraft:stone"))
    }

    @Test
    fun aBlobThatIsNotEvenACompoundIsRefused() {
        assertFailsWith<StorageFormatException> { StatsNbt.decode(byteArrayOf(1, 2, 3)) }
    }
}
