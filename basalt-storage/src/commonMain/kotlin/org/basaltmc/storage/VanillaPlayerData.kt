package org.basaltmc.storage

import org.basaltmc.nbt.NbtCompound
import org.basaltmc.nbt.NbtIO

/**
 * Reads a vanilla `playerdata/<uuid>.dat` and maps it onto [StoredPlayer].
 *
 * The file is gzipped binary NBT, so the caller passes the decompressor: this module has no
 * business linking one, and `basalt-io` already has gzip. What comes out is vanilla's own field
 * names, which are not the ones used here (`playerGameType`, `foodLevel`, `XpP`), so the mapping is
 * explicit rather than reflective.
 *
 * A word on what this is *not*: it is a one-way import, not a compatibility layer. Basalt does not
 * write this format, and nothing here tries to preserve what it cannot model.
 */
public object VanillaPlayerData {
    /**
     * Vanilla stores the UUID in the file name, not in the file. The dimension is a resource
     * location (`minecraft:overworld`) and becomes a plain world name here, since a Basalt world is
     * a directory under `worlds/`, not a dimension of one save.
     */
    public fun read(
        uuid: String,
        name: String,
        gzippedBytes: ByteArray,
        decompress: (ByteArray) -> ByteArray,
        nowEpochSeconds: Long,
        worldName: String,
    ): StoredPlayer {
        val root =
            NbtIO.readCompressed(gzippedBytes, decompress).tag as? NbtCompound
                ?: throw StorageFormatException("$uuid.dat is not an NBT compound")

        val position = root.getList("Pos")?.toList().orEmpty()
        val rotation = root.getList("Rotation")?.toList().orEmpty()

        return StoredPlayer(
            uuid = uuid,
            name = name,
            lastSeenEpochSeconds = nowEpochSeconds,
            firstSeenEpochSeconds = nowEpochSeconds,
            // Vanilla keeps play time in the statistics file, not here, so an import starts the
            // counter rather than inventing one.
            playTimeTicks = 0L,
            world = worldName,
            x = position.getOrNull(0).asDouble(),
            y = position.getOrNull(1).asDouble(),
            z = position.getOrNull(2).asDouble(),
            yaw = rotation.getOrNull(0).asDouble().toFloat(),
            pitch = rotation.getOrNull(1).asDouble().toFloat(),
            health = root.getFloat("Health"),
            food = root.getInt("foodLevel"),
            saturation = root.getFloat("foodSaturationLevel"),
            exhaustion = root.getFloat("foodExhaustionLevel"),
            xpLevel = root.getInt("XpLevel"),
            xpProgress = root.getFloat("XpP"),
            xpTotal = root.getInt("XpTotal"),
            gamemode = StoredGamemode.byId(root.getInt("playerGameType")),
            inventory = readInventory(root.getList("Inventory")?.toList().orEmpty(), StoredInventory.PLAYER_SLOTS),
            enderChest = readInventory(root.getList("EnderItems")?.toList().orEmpty(), StoredPlayer.ENDER_CHEST_SLOTS),
        )
    }

    /**
     * Vanilla's inventory is a list of item compounds each carrying its own `Slot` byte, with
     * armour and the offhand at slots 100-103 and -106. Those are folded into the flat layout used
     * here, because a slot number that is negative or above the inventory size is vanilla's own
     * encoding trick, not a real position.
     */
    private fun readInventory(
        items: List<org.basaltmc.nbt.NbtTag>,
        size: Int,
    ): StoredInventory {
        val slots = arrayOfNulls<StoredItemStack>(size)
        for (element in items) {
            val item = element as? NbtCompound ?: continue
            val vanillaSlot = item.getByte("Slot").toInt()
            val slot =
                when {
                    vanillaSlot in 0 until MAIN_SLOTS -> vanillaSlot
                    vanillaSlot in ARMOUR_BASE until ARMOUR_BASE + 4 -> MAIN_SLOTS + (vanillaSlot - ARMOUR_BASE)
                    vanillaSlot == OFFHAND -> MAIN_SLOTS + 4
                    else -> continue
                }
            if (slot !in slots.indices) continue
            val count = item.getInt("count").takeIf { it > 0 } ?: continue
            slots[slot] = StoredItemStack(item.getString("id"), count, item.getCompound("components"))
        }
        return StoredInventory(slots.toList())
    }

    private fun org.basaltmc.nbt.NbtTag?.asDouble(): Double =
        when (this) {
            is org.basaltmc.nbt.NbtDouble -> value
            is org.basaltmc.nbt.NbtFloat -> value.toDouble()
            else -> 0.0
        }

    private const val MAIN_SLOTS = 36
    private const val ARMOUR_BASE = 100
    private const val OFFHAND = -106
}
