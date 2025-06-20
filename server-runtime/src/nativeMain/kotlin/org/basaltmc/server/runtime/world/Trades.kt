package org.basaltmc.server.runtime.world

import org.basaltmc.server.world.loot.ItemStack
import kotlin.math.floor

/**
 * What a merchant offers and what it charges for it.
 *
 * parity: `MerchantOffer`. The price is not fixed: it is the base cost plus what demand has done to
 * it plus whatever discount the buyer has earned, clamped to at least one and at most a stack. An
 * offer runs out after its uses and comes back when the merchant restocks.
 *
 * Who does the offering is a villager, which does not exist yet. This is the half of trading that
 * is items: when a merchant exists it has an offer list and this prices it.
 */
public object Trades {
    /** One offer: what it takes, what it gives, and how its price moves. */
    public class Offer(
        public val costA: ItemStack,
        public val costB: ItemStack? = null,
        public val result: ItemStack,
        public val maxUses: Int = 12,
        public val experience: Int = 0,
        public val priceMultiplier: Float = 0.05f,
        public var uses: Int = 0,
        public var demand: Int = 0,
        public var specialPrice: Int = 0,
    ) {
        /** parity: `MerchantOffer.isOutOfStock`. */
        public val outOfStock: Boolean get() = uses >= maxUses
    }

    /**
     * parity: `MerchantOffer.getModifiedCostCount`: the base, plus the demand times the multiplier
     * floored, plus the special price, never under one and never over a stack.
     */
    public fun priceOf(
        offer: Offer,
        cost: ItemStack,
        maxStackSize: Int = MAX_STACK,
    ): Int {
        val base = cost.count
        val fromDemand = maxOf(0, floor(base * offer.demand * offer.priceMultiplier.toDouble()).toInt())
        return (base + fromDemand + offer.specialPrice).coerceIn(1, maxStackSize)
    }

    /** What this offer asks for right now, both slots. */
    public fun currentCost(offer: Offer): Pair<ItemStack, ItemStack?> =
        offer.costA.copy(count = priceOf(offer, offer.costA)) to
            offer.costB?.let { it.copy(count = priceOf(offer, it)) }

    /**
     * Takes the trade: one use, and the demand moves with it.
     *
     * parity: `MerchantOffer.increaseUses` and `updateDemand`, which is what makes a trade a
     * villager is asked for often cost more and one nobody wants cost less.
     */
    public fun take(offer: Offer) {
        offer.uses++
    }

    /** parity: `updateDemand`, which a merchant runs when it restocks rather than on each trade. */
    public fun restock(offer: Offer) {
        offer.demand = offer.demand + offer.uses - (offer.maxUses - offer.uses)
        offer.uses = 0
    }

    /** Whether [stack] pays for [cost], which is what the menu checks before it hands anything over. */
    public fun satisfies(
        stack: ItemStack?,
        cost: ItemStack?,
    ): Boolean {
        if (cost == null) return true
        if (stack == null) return false
        return stack.item == cost.item && stack.count >= cost.count
    }

    private const val MAX_STACK = 64
}
