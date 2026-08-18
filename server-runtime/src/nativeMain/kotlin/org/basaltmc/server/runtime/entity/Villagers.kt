package org.basaltmc.server.runtime.entity

import org.basaltmc.server.runtime.world.RealVillagerTrades
import org.basaltmc.server.runtime.world.Trades
import org.basaltmc.server.runtime.world.VillagerTrade
import org.basaltmc.server.world.loot.ItemStack
import org.basaltmc.wire.common.Identifier

/**
 * The villager as a merchant: what it offers, and what taking an offer costs it.
 *
 * parity: `Villager` over `VillagerData`, with the trades themselves read out of the jar's own data
 * (see [RealVillagerTrades]). A villager offers the trades of its profession up to its level, each
 * one priced by [Trades]: demand raises what it asks for, and running out of stock takes it off the
 * list until the villager restocks.
 */
public object Villagers {
    public val TYPE: Identifier = Identifier.vanilla("villager")

    /** parity: `VillagerData.MIN_VILLAGER_LEVEL`, which a villager starts at. */
    public const val FIRST_LEVEL: Int = 1

    /** parity: the five levels a villager can reach. */
    public const val MAX_LEVEL: Int = 5

    /** parity: `VillagerProfession`, whose names are what the trade files are filed under. */
    public val PROFESSIONS: List<String> =
        listOf(
            "armorer",
            "butcher",
            "cartographer",
            "cleric",
            "farmer",
            "fisherman",
            "fletcher",
            "leatherworker",
            "librarian",
            "mason",
            "shepherd",
            "toolsmith",
            "weaponsmith",
        )

    /** Turns the jar's trades into the offers a merchant shows, at this level and every one below. */
    public fun offersFor(
        trades: RealVillagerTrades,
        profession: String,
        level: Int,
    ): List<Trades.Offer> =
        (FIRST_LEVEL..level.coerceIn(FIRST_LEVEL, MAX_LEVEL))
            .flatMap { trades.forProfession(profession, it) }
            .map(::asOffer)

    /** parity: one `MerchantOffer` per trade file, with the file's own counts and limits. */
    public fun asOffer(trade: VillagerTrade): Trades.Offer =
        Trades.Offer(
            costA = ItemStack(trade.wants, trade.wantsCount),
            costB = trade.alsoWants?.let { ItemStack(it, trade.alsoWantsCount) },
            result = ItemStack(trade.gives, trade.givesCount),
            maxUses = trade.maxUses,
            experience = trade.experience,
            priceMultiplier = trade.priceMultiplier,
        )
}
