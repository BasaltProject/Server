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

    /**
     * The first slot carrying [id], or `null` if this player has none: what "pick block" searches
     * for. parity: `Inventory.findSlotMatchingItem`, which scans only the main inventory, so
     * armour and the offhand deliberately do not answer here either.
     */
    public fun indexOf(id: String): Int? = (0 until MAIN_SLOTS).firstOrNull { slots[it]?.id == id }

    /**
     * The hotbar slot a picked item should go to, as a 0-8 index.
     *
     * parity: `Inventory.getSuitableHotbarSlot`. Scans from the currently held slot and wraps, so
     * the first *empty* slot at or after the one in hand wins; only when every hotbar slot is full
     * does it fall back. Vanilla's own fallback prefers an unenchanted slot before giving up on the
     * held one, which collapses to the held one here because enchantments are not modelled yet.
     */
    public fun suitableHotbarSlot(): Int {
        for (offset in 0 until HOTBAR_SIZE) {
            val index = (heldSlot + offset) % HOTBAR_SIZE
            if (slots[HOTBAR_START + index] == null) return index
        }
        return heldSlot
    }

    /** The first empty main-inventory slot, or `null` if there is none. parity: `Inventory.getFreeSlot`. */
    public fun firstFreeSlot(): Int? = (0 until MAIN_SLOTS).firstOrNull { slots[it] == null }

    /** Swaps the stacks at [a] and [b], slot indices both in range. */
    public fun swap(
        a: Int,
        b: Int,
    ) {
        val temp = slots[a]
        slots[a] = slots[b]
        slots[b] = temp
    }

    /**
     * Every slot as container 0 wants them: [MENU_SLOTS] entries in `InventoryMenu`'s own order,
     * not this class's storage order. See [menuSlotOf] for why those differ.
     */
    public fun toWire(): List<WireItemStack?> =
        List(MENU_SLOTS) { menuSlot -> storageSlotOf(menuSlot)?.let { toWire(slots.getOrNull(it)) } }

    public fun toWire(slot: Int): WireItemStack? = toWire(slots.getOrNull(slot))

    public fun snapshot(): StoredInventory = StoredInventory(slots.toList())

    public fun restore(stored: StoredInventory) {
        slots.fill(null)
        for ((index, stack) in stored.slots.withIndex()) {
            if (index in slots.indices) slots[index] = stack
        }
    }

    /** One arbitrary stack in wire form, for things held outside the slots (the cursor during a click). */
    public fun toWire(stack: StoredItemStack?): WireItemStack? {
        if (stack == null) return null
        // An item this build's registry does not know is left out rather than sent as some other
        // item: a wrong id is a wrong item in someone's hand.
        val id = Items.REGISTRY.byIdentifier(Identifier.parse(stack.id))?.protocolId ?: return null
        return WireItemStack(id, stack.count)
    }

    public companion object {
        /**
         * How many slots container 0 has on the wire, and the whole reason this class needs a
         * mapping at all: the player's own window is `InventoryMenu`, which is **46** slots
         * (result, 2x2 crafting grid, armour, storage, hotbar, offhand), not the 41 this class
         * stores. Verified against the real 26.2 jar's own `InventoryMenu` constants
         * (`RESULT_SLOT`/`CRAFT_SLOT_START`/`ARMOR_SLOT_START`/`INV_SLOT_START`/`USE_ROW_SLOT_START`/
         * `SHIELD_SLOT`).
         *
         * Sending the raw 41 instead is not a cosmetic mismatch: a real client drops the whole
         * window packet, so nothing the server changes ever appears, and it addresses its own
         * clicks in menu numbering, so a hotbar edit lands on whatever the server keeps 9 slots
         * earlier (armour, as it happens). Both directions have to go through here.
         */
        public const val MENU_SLOTS: Int = 46

        private const val MENU_ARMOUR_START = 5
        private const val MENU_STORAGE_START = 9
        private const val MENU_OFFHAND = 45

        private const val STORAGE_ARMOUR_START = 36
        private const val STORAGE_OFFHAND = 40

        /**
         * This class's own slot index for a container-0 slot, or `null` for the five slots it does
         * not model (the crafting result and the 2x2 grid, which this build has no crafting for).
         */
        public fun storageSlotOf(menuSlot: Int): Int? =
            when (menuSlot) {
                in MENU_ARMOUR_START until MENU_STORAGE_START -> menuSlot - MENU_ARMOUR_START + STORAGE_ARMOUR_START
                in MENU_STORAGE_START until MENU_OFFHAND -> menuSlot - MENU_STORAGE_START
                MENU_OFFHAND -> STORAGE_OFFHAND
                else -> null
            }

        /** The container-0 slot this class's [storageSlot] appears in. The inverse of [storageSlotOf]. */
        public fun menuSlotOf(storageSlot: Int): Int =
            when (storageSlot) {
                in 0 until STORAGE_ARMOUR_START -> storageSlot + MENU_STORAGE_START
                in STORAGE_ARMOUR_START until STORAGE_OFFHAND -> storageSlot - STORAGE_ARMOUR_START + MENU_ARMOUR_START
                else -> MENU_OFFHAND
            }

        /** Storage rows plus hotbar, the part `Inventory.findSlotMatchingItem`/`getFreeSlot` scan. Armour and offhand come after. */
        public const val MAIN_SLOTS: Int = 36

        public const val HOTBAR_SIZE: Int = 9

        /** Where the hotbar starts in the network order: after the 27 storage rows. */
        public const val HOTBAR_START: Int = 36 - HOTBAR_SIZE

        /** Every item stacks to this until per-item stack limits are modelled; see [add]'s own KDoc. */
        public const val MAX_STACK: Int = 64

        /** Hotbar first, then the rest of the main inventory: where a picked-up item goes. */
        private val FILL_ORDER: List<Int> = (HOTBAR_START until 36) + (9 until HOTBAR_START) + (0 until 9)
    }
}
