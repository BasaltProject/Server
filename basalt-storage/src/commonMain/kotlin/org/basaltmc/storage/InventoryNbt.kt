package org.basaltmc.storage

import org.basaltmc.nbt.NbtCompound
import org.basaltmc.nbt.NbtIO
import org.basaltmc.nbt.NbtList
import org.basaltmc.nbt.NbtType

/**
 * Turns an inventory into the blob that goes in the database, and back.
 *
 * NBT rather than JSON, for the same reason vanilla uses it: item data is arbitrarily nested and
 * typed, and JSON would mean either losing the type of every number or reinventing NBT inside JSON
 * without gaining any readability worth the size. Nothing here is compressed: the blob is small,
 * SQLite already stores it as one value, and a compressed blob cannot be inspected with `sqlite3`
 * when something goes wrong.
 *
 * [FORMAT_VERSION] is written into every blob. It exists so a future change to the item model can
 * be detected rather than misread: a blob from a version this build does not understand is
 * refused, which is preferable to loading someone's inventory as garbage.
 */
public object InventoryNbt {
    public const val FORMAT_VERSION: Int = 1

    private const val VERSION_KEY = "Version"
    private const val SIZE_KEY = "Size"
    private const val SLOTS_KEY = "Slots"
    private const val SLOT_KEY = "Slot"
    private const val ID_KEY = "id"
    private const val COUNT_KEY = "count"
    private const val COMPONENTS_KEY = "components"

    public fun encode(inventory: StoredInventory): ByteArray {
        val root = NbtCompound()
        root.putInt(VERSION_KEY, FORMAT_VERSION)
        root.putInt(SIZE_KEY, inventory.slots.size)
        // Only occupied slots are written, each carrying its own index: an inventory is mostly
        // empty most of the time, and a list of 41 empty compounds is all overhead.
        val occupied =
            inventory.slots.mapIndexedNotNull { index, stack ->
                if (stack == null) {
                    null
                } else {
                    NbtCompound().apply {
                        putInt(SLOT_KEY, index)
                        putString(ID_KEY, stack.id)
                        putInt(COUNT_KEY, stack.count)
                        if (stack.components.keys.isNotEmpty()) putCompound(COMPONENTS_KEY, stack.components)
                    }
                }
            }
        root[SLOTS_KEY] = NbtList(NbtType.COMPOUND, occupied)
        return NbtIO.write(root)
    }

    public fun decode(bytes: ByteArray): StoredInventory {
        val root =
            runCatching { NbtIO.read(bytes).tag }
                .getOrElse { throw StorageFormatException("inventory blob is not readable NBT: ${it.message}") }
                as? NbtCompound ?: throw StorageFormatException("inventory blob is not a compound")
        val version = root.getInt(VERSION_KEY)
        if (version != FORMAT_VERSION) {
            throw StorageFormatException("inventory blob is format version $version, this build reads $FORMAT_VERSION")
        }
        val size = root.getInt(SIZE_KEY)
        val slots = arrayOfNulls<StoredItemStack>(size)
        for (element in root.getList(SLOTS_KEY)?.toList().orEmpty()) {
            val entry = element as? NbtCompound ?: continue
            val slot = entry.getInt(SLOT_KEY)
            if (slot !in
                slots.indices
            ) {
                throw StorageFormatException("inventory blob has an item in slot $slot of $size")
            }
            val count = entry.getInt(COUNT_KEY)
            if (count <= 0) throw StorageFormatException("inventory blob has $count items in slot $slot")
            slots[slot] = StoredItemStack(entry.getString(ID_KEY), count, entry.getCompound(COMPONENTS_KEY))
        }
        return StoredInventory(slots.toList())
    }
}

/** A stored blob that cannot be read back: a format from another version, or a corrupted one. */
public class StorageFormatException(
    message: String,
) : Exception(message)
