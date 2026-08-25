package org.basaltmc.server.runtime

import org.basaltmc.server.plugins.PluginEventTypes
import org.basaltmc.server.registry.Items
import org.basaltmc.server.runtime.entity.ServerEntity
import org.basaltmc.server.runtime.entity.Villagers
import org.basaltmc.server.runtime.player.Menu
import org.basaltmc.server.runtime.world.Trades
import org.basaltmc.server.storage.StoredItemStack
import org.basaltmc.wire.protocol.play.MerchantCost
import org.basaltmc.wire.protocol.play.MerchantOffer
import org.basaltmc.wire.protocol.play.MerchantOffersPacket
import org.basaltmc.wire.protocol.play.OpenScreenPacket
import org.basaltmc.wire.protocol.play.WireItemStack
import org.basaltmc.wire.text.Component

/**
 * Trading with a villager: the offers sent when the window opens, and the trade the player picks.
 *
 * A back-reference to the owning session rather than captured values, same as [PacketInterceptor]
 * and [ItemUseTimers]: opening a trade reaches into [ClientSession]'s menu and inventory state, and
 * every packet still goes out through the same connection.
 */
internal class VillagerTradeController(
    private val owner: ClientSession,
) {
    /** The villager this client is trading with, and what it offered. */
    private var openTradingWith: Int? = null
    private var openOffers: List<Trades.Offer>? = null

    /**
     * Opens a villager's trades.
     *
     * parity: `Villager.mobInteract` over `MerchantMenu`: the offers go out as they are, and what
     * the player then picks is a `select_trade`. The offers themselves come out of the jar's own
     * data, so a farmer offers what a farmer offers.
     */
    internal fun openTrades(villager: ServerEntity) {
        val session = owner.playerSession ?: return
        val trades = owner.villagerTrades ?: return
        val offers = Villagers.offersFor(trades, villager.profession, villager.villagerLevel)
        if (offers.isEmpty()) return
        owner.returnCraftingGrid(session)
        owner.nextContainerId =
            if (owner.nextContainerId >= ClientSession.MAX_CONTAINER_ID) {
                ClientSession.FIRST_BLOCK_CONTAINER_ID
            } else {
                owner.nextContainerId + 1
            }
        owner.openMenu = Menu.merchant(owner.nextContainerId)
        owner.openBlock = null
        owner.openBlockSecond = null
        openTradingWith = villager.entityId
        openOffers = offers
        owner.sendCompressed(OpenScreenPacket.ID) {
            OpenScreenPacket(
                containerId = owner.openMenu.containerId,
                menuType = OpenScreenPacket.MERCHANT_MENU,
                title = Component.translatable("entity.minecraft.villager"),
            ).write(it)
        }
        sendOffers()
        owner.sendInventory()
    }

    /** parity: `ClientboundMerchantOffersPacket`, sent when the window opens and after every trade. */
    internal fun sendOffers() {
        val offers = openOffers ?: return
        val villager = openTradingWith?.let { owner.currentWorld?.state?.entity(it) } ?: return
        val wire =
            offers.mapNotNull { offer ->
                val costA = Items.REGISTRY.byIdentifier(offer.costA.item)?.protocolId ?: return@mapNotNull null
                val result = Items.REGISTRY.byIdentifier(offer.result.item)?.protocolId ?: return@mapNotNull null
                val costB =
                    offer.costB?.let { second ->
                        Items.REGISTRY.byIdentifier(second.item)?.protocolId?.let { id ->
                            MerchantCost(id, Trades.priceOf(offer, second))
                        }
                    }
                MerchantOffer(
                    costA = MerchantCost(costA, Trades.priceOf(offer, offer.costA)),
                    result = WireItemStack(result, offer.result.count),
                    costB = costB,
                    outOfStock = offer.outOfStock,
                    uses = offer.uses,
                    maxUses = offer.maxUses,
                    experience = offer.experience,
                    specialPrice = offer.specialPrice,
                    priceMultiplier = offer.priceMultiplier,
                    demand = offer.demand,
                )
            }
        owner.sendPlayPacket(MerchantOffersPacket.ID) {
            MerchantOffersPacket(
                containerId = owner.openMenu.containerId,
                offers = wire,
                level = villager.villagerLevel,
                experience = villager.villagerExperience,
                showProgress = true,
                canRestock = true,
            ).write(it)
        }
    }

    /**
     * Takes the trade the player picked.
     *
     * parity: `MerchantMenu.clickMenuButton`: the offer is checked against what they are carrying,
     * the cost comes out of the inventory, the result goes in, and the offer's own use is counted.
     */
    internal fun selectTrade(index: Int) {
        val trading =
            owner.playerEvent(PluginEventTypes.INVENTORY_TRADE) { event ->
                event.set(3, owner.openMenu.containerId)
                event.set(4, index)
                event.set(
                    5,
                    openOffers
                        ?.getOrNull(index)
                        ?.result
                        ?.item
                        ?.toString(),
                )
            }
        if (trading?.cancelled == true) {
            trading.cancelMessage?.let { owner.sendPluginNotice(it) }
            return
        }
        val session = owner.playerSession ?: return
        val offers = openOffers ?: return
        val offer = offers.getOrNull(index) ?: return
        if (offer.outOfStock) return
        val (costA, costB) = Trades.currentCost(offer)
        if (session.inventory.countOf(costA.item.toString()) < costA.count) return
        if (costB != null && session.inventory.countOf(costB.item.toString()) < costB.count) return

        session.inventory.take(costA.item.toString(), costA.count)
        costB?.let { session.inventory.take(it.item.toString(), it.count) }
        val given =
            session.inventory.add(StoredItemStack(offer.result.item.toString(), offer.result.count))
        if (given > 0) {
            owner.spawnDroppedItem(offer.result.item, given, session.x, session.y, session.z)
        }
        Trades.take(offer)
        val villager = openTradingWith?.let { owner.currentWorld?.state?.entity(it) }
        villager?.let { it.villagerExperience += offer.experience }
        owner.sendInventory()
        sendOffers()
    }
}
