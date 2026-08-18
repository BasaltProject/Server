package org.basaltmc.server.runtime.world

import org.basaltmc.wire.common.Identifier
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlin.test.assertTrue

/** parity: the `villager_trade` files 26.2 moved the trades into. */
class RealVillagerTradesTest {
    @Test
    fun withoutTheJarNobodyTrades() {
        val trades = RealVillagerTrades(serverJar = null)

        assertTrue(trades.forProfession("farmer", 1).isEmpty())
    }

    /** The shape of a real trade file, written by hand: the jar itself is the operator's. */
    @Test
    fun itReadsWhatATradeWantsAndGives() {
        val trade =
            assertNotNull(
                RealVillagerTrades.parseForTest(
                    Identifier.vanilla("farmer/1/wheat_emerald"),
                    """
                    {
                      "gives": { "id": "minecraft:emerald" },
                      "max_uses": 16.0,
                      "reputation_discount": 0.05,
                      "wants": { "count": 20.0, "id": "minecraft:wheat" },
                      "xp": 2.0
                    }
                    """.trimIndent(),
                ),
            )

        assertEquals(Identifier.vanilla("wheat"), trade.wants)
        assertEquals(20, trade.wantsCount)
        assertEquals(Identifier.vanilla("emerald"), trade.gives)
        assertEquals(1, trade.givesCount, "a count that is not written is one")
        assertEquals(16, trade.maxUses)
        assertEquals(2, trade.experience)
        assertEquals(0.05f, trade.priceMultiplier)
        assertNull(trade.alsoWants)
    }

    /** parity: `additional_wants`, the second thing some trades ask for. */
    @Test
    fun aTradeCanAskForTwoThings() {
        val trade =
            assertNotNull(
                RealVillagerTrades.parseForTest(
                    Identifier.vanilla("librarian/1/emerald_book_enchanted_book"),
                    """
                    {
                      "additional_wants": { "count": 1.0, "id": "minecraft:book" },
                      "gives": { "id": "minecraft:enchanted_book" },
                      "max_uses": 12.0,
                      "reputation_discount": 0.2,
                      "wants": { "count": 8.0, "id": "minecraft:emerald" },
                      "xp": 5.0
                    }
                    """.trimIndent(),
                ),
            )

        assertEquals(Identifier.vanilla("emerald"), trade.wants)
        assertEquals(8, trade.wantsCount)
        assertEquals(Identifier.vanilla("book"), trade.alsoWants)
        assertEquals(1, trade.alsoWantsCount)
    }
}
