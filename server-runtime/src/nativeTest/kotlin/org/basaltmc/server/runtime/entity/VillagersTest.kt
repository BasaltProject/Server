package org.basaltmc.server.runtime.entity

import org.basaltmc.server.runtime.world.Trades
import org.basaltmc.server.runtime.world.VillagerTrade
import org.basaltmc.server.world.loot.ItemStack
import org.basaltmc.wire.common.Identifier
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNull
import kotlin.test.assertTrue

/** parity: `Villager` as a merchant, over the trades the jar ships as data. */
class VillagersTest {
    private fun trade(
        wants: String = "wheat",
        wantsCount: Int = 20,
        gives: String = "emerald",
        givesCount: Int = 1,
        maxUses: Int = 16,
        xp: Int = 2,
    ) = VillagerTrade(
        id = Identifier.vanilla("farmer/1/wheat_emerald"),
        wants = Identifier.vanilla(wants),
        wantsCount = wantsCount,
        alsoWants = null,
        alsoWantsCount = 0,
        gives = Identifier.vanilla(gives),
        givesCount = givesCount,
        maxUses = maxUses,
        experience = xp,
        priceMultiplier = 0.05f,
    )

    /** A trade file becomes an offer with the same counts and the same limit. */
    @Test
    fun aTradeBecomesAnOffer() {
        val offer = Villagers.asOffer(trade())

        assertEquals(Identifier.vanilla("wheat"), offer.costA.item)
        assertEquals(20, offer.costA.count)
        assertEquals(Identifier.vanilla("emerald"), offer.result.item)
        assertEquals(16, offer.maxUses)
        assertEquals(2, offer.experience)
        assertNull(offer.costB)
    }

    /** parity: `MerchantOffer.isOutOfStock`, which is what greys a trade out. */
    @Test
    fun anOfferRunsOutAfterItsUses() {
        val offer = Villagers.asOffer(trade(maxUses = 2))

        Trades.take(offer)
        assertTrue(!offer.outOfStock)
        Trades.take(offer)
        assertTrue(offer.outOfStock)
    }

    /** parity: `getModifiedCostCount`: what it asks for climbs with demand. */
    @Test
    fun demandRaisesThePrice() {
        val offer = Villagers.asOffer(trade(wantsCount = 20, maxUses = 10))

        assertEquals(20, Trades.priceOf(offer, offer.costA))
        repeat(10) { Trades.take(offer) }
        Trades.restock(offer)
        assertTrue(Trades.priceOf(offer, offer.costA) > 20, "ten sales should have raised it")
    }

    /** parity: `VillagerProfession`, whose names are the folders the trade files live in. */
    @Test
    fun theProfessionsAreTheOnesWithTrades() {
        assertTrue("farmer" in Villagers.PROFESSIONS)
        assertTrue("librarian" in Villagers.PROFESSIONS)
        assertEquals(13, Villagers.PROFESSIONS.size, "thirteen that trade, nitwit and unemployed aside")
    }

    /** A cost of two items is carried through, which is how a librarian asks for a book as well. */
    @Test
    fun aSecondCostSurvives() {
        val withBook =
            VillagerTrade(
                id = Identifier.vanilla("librarian/1/emerald_book_enchanted_book"),
                wants = Identifier.vanilla("emerald"),
                wantsCount = 8,
                alsoWants = Identifier.vanilla("book"),
                alsoWantsCount = 1,
                gives = Identifier.vanilla("enchanted_book"),
                givesCount = 1,
                maxUses = 12,
                experience = 5,
                priceMultiplier = 0.2f,
            )

        val offer = Villagers.asOffer(withBook)

        assertEquals(ItemStack(Identifier.vanilla("book"), 1).item, offer.costB?.item)
        assertEquals(1, offer.costB?.count)
    }
}
