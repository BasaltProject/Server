package org.basaltmc.server.runtime.world

import org.basaltmc.server.world.loot.ItemStack
import org.basaltmc.wire.common.Identifier
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertTrue

/** parity: `MerchantOffer`, the half of trading that is items rather than villagers. */
class TradesTest {
    private val emerald = Identifier.vanilla("emerald")
    private val bread = Identifier.vanilla("bread")

    private fun offer(
        cost: Int = 1,
        maxUses: Int = 12,
        multiplier: Float = 0.05f,
    ) = Trades.Offer(
        costA = ItemStack(emerald, cost),
        result = ItemStack(bread, 6),
        maxUses = maxUses,
        priceMultiplier = multiplier,
    )

    @Test
    fun anUntouchedOfferCostsWhatItSays() {
        val trade = offer(cost = 3)

        assertEquals(3, Trades.priceOf(trade, trade.costA))
        assertFalse(trade.outOfStock)
    }

    /** parity: `isOutOfStock`, which is what greys a trade out until the villager restocks. */
    @Test
    fun itRunsOutAfterItsUses() {
        val trade = offer(maxUses = 3)

        repeat(3) { Trades.take(trade) }

        assertTrue(trade.outOfStock)
    }

    /**
     * parity: `updateDemand`: demand climbs with the uses since the last restock and falls when
     * nobody buys, which is what makes a popular trade cost more.
     */
    @Test
    fun demandFollowsWhatWasBought() {
        val trade = offer(cost = 20, maxUses = 10, multiplier = 0.05f)

        repeat(10) { Trades.take(trade) }
        Trades.restock(trade)

        assertEquals(10, trade.demand, "ten uses out of ten")
        assertEquals(0, trade.uses, "and the stock is back")
        assertEquals(30, Trades.priceOf(trade, trade.costA), "twenty plus twenty times ten times 0.05")

        // Nobody buys for a whole cycle, and the demand comes back down.
        Trades.restock(trade)
        assertEquals(0, trade.demand)
        assertEquals(20, Trades.priceOf(trade, trade.costA))
    }

    /** parity: the clamp, which is what keeps a discount from making something free. */
    @Test
    fun thePriceNeverFallsUnderOne() {
        val trade = offer(cost = 2)
        trade.specialPrice = -10

        assertEquals(1, Trades.priceOf(trade, trade.costA))
    }

    @Test
    fun payingForItIsAboutCountAndKind() {
        val trade = offer(cost = 5)
        val (cost, second) = Trades.currentCost(trade)

        assertEquals(5, cost.count)
        assertTrue(Trades.satisfies(ItemStack(emerald, 5), cost))
        assertFalse(Trades.satisfies(ItemStack(emerald, 4), cost))
        assertFalse(Trades.satisfies(ItemStack(bread, 64), cost))
        assertTrue(Trades.satisfies(null, second), "an offer with one slot asks for nothing in the other")
    }
}
