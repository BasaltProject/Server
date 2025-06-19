package org.basaltmc.server.runtime.world

import org.basaltmc.server.world.loot.ItemStack
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtByte
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtInt
import org.basaltmc.wire.nbt.NbtList
import org.basaltmc.wire.nbt.NbtString
import org.basaltmc.wire.nbt.NbtType

/**
 * The slots a block keeps, in the shape vanilla writes them.
 *
 * parity: `ContainerHelper.saveAllItems`/`loadAllItems`: a list of compounds, each with the slot it
 * sits in, the item and the count. A furnace and a chest differ in how many slots they have and in
 * what they do with them, not in how they are written down.
 */
public object BlockInventory {
    private const val ITEMS = "Items"

    /** Reads [size] slots out of [data], leaving the ones it does not mention empty. */
    public fun read(
        data: NbtCompound,
        size: Int,
    ): Array<ItemStack?> {
        val slots = arrayOfNulls<ItemStack>(size)
        val items = data[ITEMS] as? NbtList ?: return slots
        for (index in 0 until items.size) {
            val entry = items[index] as? NbtCompound ?: continue
            val slot = (entry["Slot"] as? NbtByte)?.value?.toInt() ?: continue
            if (slot !in slots.indices) continue
            val id = (entry["id"] as? NbtString)?.value ?: continue
            val count = (entry["count"] as? NbtInt)?.value ?: 1
            slots[slot] = ItemStack(Identifier.parse(id), count)
        }
        return slots
    }

    /** Writes them back, skipping the empty ones exactly as vanilla does. */
    public fun write(
        data: NbtCompound,
        slots: Array<ItemStack?>,
    ) {
        val entries = mutableListOf<NbtCompound>()
        for (slot in slots.indices) {
            val stack = slots[slot] ?: continue
            val entry = NbtCompound()
            entry.putByte("Slot", slot.toByte())
            entry.putString("id", stack.item.toString())
            entry.putInt("count", stack.count)
            entries += entry
        }
        data[ITEMS] = NbtList(NbtType.COMPOUND, entries)
    }
}
