package org.basaltmc.server.runtime.entity

import kotlin.uuid.ExperimentalUuidApi
import kotlin.uuid.Uuid

/** How a raid ended, which is what decides whether anybody gets the hero effect. */
public enum class RaidStatus {
    ONGOING,
    VICTORY,
    LOSS,
    STOPPED,
}

/**
 * One raid on one village.
 *
 * parity: `Raid`, minus what needs a POI map: the centre never moves, and the raid is over when its
 * waves are, rather than when the village stops being one.
 */
@OptIn(ExperimentalUuidApi::class)
public class Raid(
    public val centerX: Int,
    public val centerY: Int,
    public val centerZ: Int,
    /** parity: `getNumGroups`, three waves on easy, five on normal, seven on hard. */
    public val totalGroups: Int,
) {
    public val barId: Uuid = Uuid.random()

    public var omenLevel: Int = 0
    public var groupsSpawned: Int = 0
    public var ticksActive: Int = 0

    /** parity: `raidCooldownTicks`, the fifteen seconds between one wave dying and the next. */
    public var cooldown: Int = PRE_RAID_TICKS
    public var status: RaidStatus = RaidStatus.ONGOING
    public var celebrationTicks: Int = 0

    /** Entity ids, which is all this build needs to count what is left of a wave. */
    public val raiders: MutableSet<Int> = linkedSetOf()
    public var totalHealth: Double = 0.0

    /** Whoever was near while it was on, and so gets the hero effect when it is won. */
    public val heroes: MutableSet<Int> = mutableSetOf()

    /** Which players are currently being sent the bar. */
    public val watchers: MutableSet<Int> = mutableSetOf()

    public val active: Boolean get() = status == RaidStatus.ONGOING

    public val hasMoreWaves: Boolean get() = groupsSpawned < totalGroups

    public fun distanceSquaredTo(
        x: Double,
        y: Double,
        z: Double,
    ): Double {
        val dx = centerX + 0.5 - x
        val dy = centerY + 0.5 - y
        val dz = centerZ + 0.5 - z
        return dx * dx + dy * dy + dz * dz
    }

    public companion object {
        /** parity: `DEFAULT_PRE_RAID_TICKS`, `RAID_TIMEOUT_TICKS` and `MAX_CELEBRATION_TICKS`. */
        public const val PRE_RAID_TICKS: Int = 300
        public const val TIMEOUT_TICKS: Int = 48000
        public const val CELEBRATION_TICKS: Int = 600

        /** parity: `VALID_RAID_RADIUS_SQR`, the distance a player counts as being in the raid from. */
        public const val VALID_RADIUS_SQUARED: Double = 9216.0

        /** parity: `MAX_RAID_OMEN_LEVEL` and the hero effect's own duration. */
        public const val MAX_OMEN_LEVEL: Int = 5
        public const val HERO_DURATION_TICKS: Int = 48000
    }
}

/** parity: `Raid.RaiderType`, whose arrays are how many of each kind each wave brings. */
public object RaidWaves {
    private val COUNTS: Map<String, IntArray> =
        mapOf(
            "vindicator" to intArrayOf(0, 0, 2, 0, 1, 4, 2, 5),
            "evoker" to intArrayOf(0, 0, 0, 0, 0, 1, 1, 2),
            "pillager" to intArrayOf(0, 4, 3, 3, 4, 4, 4, 2),
            "witch" to intArrayOf(0, 0, 0, 0, 3, 0, 0, 1),
            "ravager" to intArrayOf(0, 0, 0, 1, 0, 1, 0, 2),
        )

    public val types: Set<String> get() = COUNTS.keys

    /** How many of [type] wave [wave] brings, counting from one. */
    public fun countFor(
        type: String,
        wave: Int,
    ): Int {
        val counts = COUNTS[type] ?: return 0
        return counts[wave.coerceIn(0, counts.size - 1)]
    }

    /** parity: `getNumGroups`, which is the difficulty and nothing else. */
    public fun groupsFor(difficulty: String): Int =
        when (difficulty.lowercase()) {
            "peaceful" -> 0
            "easy" -> 3
            "hard" -> 7
            else -> 5
        }
}

/** Every raid running in one world. parity: `Raids`, the saved data behind `ServerLevel.getRaids`. */
public class RaidManager {
    private val raids = mutableListOf<Raid>()

    public val all: List<Raid> get() = raids

    /** parity: `getNearbyRaid`, which is what tells a village it is under attack. */
    public fun nearby(
        x: Double,
        y: Double,
        z: Double,
        radiusSquared: Double = Raid.VALID_RADIUS_SQUARED,
    ): Raid? = raids.firstOrNull { it.active && it.distanceSquaredTo(x, y, z) < radiusSquared }

    /**
     * parity: `createOrExtendRaid`: a second omen carried into a raid that is already running makes
     * that raid harder rather than starting another one.
     */
    public fun createOrExtend(
        x: Int,
        y: Int,
        z: Int,
        amplifier: Int,
        difficulty: String,
    ): Raid? {
        val groups = RaidWaves.groupsFor(difficulty)
        if (groups == 0) return null
        val raid =
            nearby(x + 0.5, y + 0.5, z + 0.5)
                ?: Raid(x, y, z, groups).also { raids.add(it) }
        raid.omenLevel = (raid.omenLevel + amplifier + 1).coerceIn(0, Raid.MAX_OMEN_LEVEL)
        return raid
    }

    public fun remove(raid: Raid) {
        raids.remove(raid)
    }

    public fun forget(raids: List<Raid>) {
        this.raids.removeAll(raids)
    }
}
