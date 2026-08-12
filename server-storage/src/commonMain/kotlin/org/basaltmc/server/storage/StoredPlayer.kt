package org.basaltmc.server.storage

import org.basaltmc.wire.nbt.NbtCompound

/**
 * One item in a slot: what it is, how many, and whatever else the game hung off it.
 *
 * [components] stays an NBT compound rather than field-by-field: what an item can carry changes
 * every version, and a persistence layer that enumerates them silently drops what it has not been
 * taught. Round-tripping the compound whole cannot lose anything.
 *
 * It lives here only until an inventory system exists to own it.
 */
public data class StoredItemStack(
    val id: String,
    val count: Int,
    val components: NbtCompound = NbtCompound(),
) {
    init {
        require(count > 0) { "an item stack with $count items is an empty slot, not a stack" }
    }
}

/**
 * A player's slots. Empty slots are `null`, and the list keeps its length so slot numbers survive:
 * an inventory is addressed by index, so compacting it would silently move everything.
 */
public data class StoredInventory(
    val slots: List<StoredItemStack?>,
) {
    public val isEmpty: Boolean get() = slots.all { it == null }

    public companion object {
        /** A player's main inventory: 36 slots (hotbar included), 4 armour, 1 offhand, as vanilla lays it out. */
        public const val PLAYER_SLOTS: Int = 41

        public fun empty(size: Int = PLAYER_SLOTS): StoredInventory = StoredInventory(List(size) { null })
    }
}

/** The four game modes, by the id the protocol and the database both use. */
public enum class StoredGamemode {
    SURVIVAL,
    CREATIVE,
    ADVENTURE,
    SPECTATOR,
    ;

    public companion object {
        public fun byId(id: Int): StoredGamemode = entries.getOrElse(id) { SURVIVAL }
    }
}

/**
 * Everything the server keeps about a player between sessions.
 *
 * The split is deliberate and mirrors the table: scalars are indexable columns, [inventory] and
 * [enderChest] become blobs. Nothing in here refers to a world directory, which is what lets a
 * world be deleted without touching a single row.
 */
public data class StoredPlayer(
    val uuid: String,
    val name: String,
    val lastSeenEpochSeconds: Long,
    val firstSeenEpochSeconds: Long,
    val playTimeTicks: Long,
    val world: String,
    val x: Double,
    val y: Double,
    val z: Double,
    val yaw: Float,
    val pitch: Float,
    val health: Float,
    val food: Int,
    val saturation: Float,
    val exhaustion: Float,
    val xpLevel: Int,
    val xpProgress: Float,
    val xpTotal: Int,
    val gamemode: StoredGamemode,
    /** Which permission group they are in, or null for whichever group is currently the default. */
    val group: String? = null,
    val inventory: StoredInventory = StoredInventory.empty(),
    val enderChest: StoredInventory = StoredInventory.empty(ENDER_CHEST_SLOTS),
    val stats: StoredStats = StoredStats.EMPTY,
) {
    public companion object {
        public const val ENDER_CHEST_SLOTS: Int = 27
    }
}
