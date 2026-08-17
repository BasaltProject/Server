package org.basaltmc.server.storage

import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtIO

/**
 * A player's counters, keyed the way vanilla keys them: `<category>/<what>`, both namespaced, e.g.
 * `minecraft:custom/minecraft:deaths` or `minecraft:mined/minecraft:stone`.
 *
 * Vanilla's own keys rather than names of our own, because these are the same facts and there is a
 * `stats/<uuid>.json` full of them in every existing world: an import that has to translate names
 * is an import that quietly loses the ones it has no name for.
 *
 * One flat map rather than a map of categories, since that is what every question asked of it looks
 * like ("how many of X") and the category is only ever the first half of the key.
 */
public data class StoredStats(
    public val counters: Map<String, Long> = emptyMap(),
) {
    public operator fun get(key: String): Long = counters[key] ?: 0L

    public val isEmpty: Boolean get() = counters.isEmpty()

    public fun with(
        key: String,
        amount: Long,
    ): StoredStats = StoredStats(counters + (key to (this[key] + amount)))

    public companion object {
        public val EMPTY: StoredStats = StoredStats()

        /** The category every counter that is not about a block or an item goes in. */
        public const val CUSTOM: String = "minecraft:custom"
        public const val MINED: String = "minecraft:mined"

        public fun custom(name: String): String = "$CUSTOM/minecraft:$name"

        public fun mined(block: String): String = "$MINED/$block"

        // The handful this server can honestly count today. Vanilla has around a hundred more, and
        // each one is a place in the code that has to notice something happening: they arrive with
        // the systems that make them observable, not before.
        public val DEATHS: String = custom("deaths")
        public val PLAY_TIME: String = custom("play_time")
        public val WALK_ONE_CM: String = custom("walk_one_cm")
        public val DAMAGE_TAKEN: String = custom("damage_taken")
        public val JUMP: String = custom("jump")
        public val FALL_ONE_CM: String = custom("fall_one_cm")
    }
}

/**
 * The stats blob, in the same shape and for the same reasons as [InventoryNbt].
 *
 * Counters are `long` even though vanilla's are `int`, because `walk_one_cm` and `damage_taken` are
 * counted in hundredths and a long-lived player overflows an int in a few hundred hours of walking.
 * Nothing on the wire carries these yet, so nothing is constrained by vanilla's width.
 */
public object StatsNbt {
    public const val FORMAT_VERSION: Int = 1

    private const val VERSION_KEY = "Version"
    private const val COUNTERS_KEY = "Counters"

    public fun encode(stats: StoredStats): ByteArray? {
        if (stats.isEmpty) return null
        val root = NbtCompound()
        root.putInt(VERSION_KEY, FORMAT_VERSION)
        val counters = NbtCompound()
        // A counter back at zero is dropped rather than written: it says the same thing as its
        // absence, and every stat a player has never touched would otherwise be in every blob.
        for ((key, value) in stats.counters) {
            if (value != 0L) counters.putLong(key, value)
        }
        root.putCompound(COUNTERS_KEY, counters)
        return NbtIO.write(root)
    }

    public fun decode(bytes: ByteArray): StoredStats {
        // Any malformed blob surfaces as one exception type, not two: a caller catching "the blob is
        // bad" should not have to know whether it broke in the NBT reader or after it.
        val tag =
            runCatching { NbtIO.read(bytes).tag }
                .getOrElse { throw StorageFormatException("stats blob is not readable NBT: ${it.message}") }
        val root = tag as? NbtCompound ?: throw StorageFormatException("stats blob is not a compound")
        val version = root.getInt(VERSION_KEY)
        if (version != FORMAT_VERSION) {
            throw StorageFormatException("stats blob is format version $version, this build reads $FORMAT_VERSION")
        }
        val counters = root.getCompound(COUNTERS_KEY)
        return StoredStats(counters.keys.associateWith { counters.getLong(it) }.filterValues { it != 0L })
    }
}
