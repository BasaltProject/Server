package org.basaltmc.server.player

import org.basaltmc.protocol.play.WireItemStack
import org.basaltmc.registry.Identifier
import org.basaltmc.registry.Items
import org.basaltmc.storage.StoredInventory
import org.basaltmc.storage.StoredItemStack

/**
 * The 41 slots of one player: 36 main (hotbar included), 4 armour, 1 offhand.
 *
 * Held in the vanilla *network* order, because that is the order the container packets index and
 * the order a client clicks in; the stored order is the same, which is why persistence is a
 * straight copy either way.
 *
 * This is deliberately only the container: it holds stacks, hands them out and puts them back. What
 * a player can *do* with them (clicking, dropping, placing, crafting) is a separate system that
 * does not exist yet, and is why [org.basaltmc.server.ClientSession] answers a click by re-sending
 * the whole window rather than by half-applying it.
 */
public class PlayerInventory {
    private val slots = arrayOfNulls<StoredItemStack>(StoredInventory.PLAYER_SLOTS)

    /** Which hotbar slot is in hand, 0 to 8. */
    public var heldSlot: Int = 0
        set(value) {
            field = value.coerceIn(0, HOTBAR_SIZE - 1)
        }

    public operator fun get(slot: Int): StoredItemStack? = slots.getOrNull(slot)

    public operator fun set(
        slot: Int,
        stack: StoredItemStack?,
    ) {
        if (slot in slots.indices) slots[slot] = stack
    }

    public val heldItem: StoredItemStack? get() = slots[HOTBAR_START + heldSlot]

    /**
     * Adds [stack] the way picking something up does: into a matching stack first, then the first
     * empty slot. Returns how many items would not fit.
     *
     * Stack limits are not modelled per item yet, so everything stacks to 64 except where the stored
     * count says otherwise. That is wrong for buckets and swords, and it is wrong in the direction
     * that loses nothing: the leftover is reported rather than vanishing.
     */
    public fun add(stack: StoredItemStack): Int {
        var remaining = stack.count
        for (slot in FILL_ORDER) {
            if (remaining <= 0) break
            val existing = slots[slot] ?: continue
            if (existing.id != stack.id || existing.components != stack.components) continue
            val room = MAX_STACK - existing.count
            if (room <= 0) continue
            val moved = minOf(room, remaining)
            slots[slot] = existing.copy(count = existing.count + moved)
            remaining -= moved
        }
        for (slot in FILL_ORDER) {
            if (remaining <= 0) break
            if (slots[slot] != null) continue
            val moved = minOf(MAX_STACK, remaining)
            slots[slot] = stack.copy(count = moved)
            remaining -= moved
        }
        return remaining
    }

    public fun clear() {
        slots.fill(null)
    }

    /** Every slot as the container packet wants them, empty slots included. */
    public fun toWire(): List<WireItemStack?> = slots.map(::toWire)

    public fun toWire(slot: Int): WireItemStack? = toWire(slots.getOrNull(slot))

    public fun snapshot(): StoredInventory = StoredInventory(slots.toList())

    public fun restore(stored: StoredInventory) {
        slots.fill(null)
        for ((index, stack) in stored.slots.withIndex()) {
            if (index in slots.indices) slots[index] = stack
        }
    }

    private fun toWire(stack: StoredItemStack?): WireItemStack? {
        if (stack == null) return null
        // An item this build's registry does not know is left out rather than sent as some other
        // item: a wrong id is a wrong item in someone's hand.
        val id = Items.REGISTRY.byIdentifier(Identifier.parse(stack.id))?.protocolId ?: return null
        return WireItemStack(id, stack.count)
    }

    public companion object {
        public const val HOTBAR_SIZE: Int = 9

        /** Where the hotbar starts in the network order: after the 27 storage rows. */
        public const val HOTBAR_START: Int = 36 - HOTBAR_SIZE

        private const val MAX_STACK = 64

        /** Hotbar first, then the rest of the main inventory: where a picked-up item goes. */
        private val FILL_ORDER: List<Int> = (HOTBAR_START until 36) + (9 until HOTBAR_START) + (0 until 9)
    }
}
