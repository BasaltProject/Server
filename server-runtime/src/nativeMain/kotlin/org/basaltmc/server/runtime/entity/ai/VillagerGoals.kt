package org.basaltmc.server.runtime.entity.ai

import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.runtime.world.WorldState
import kotlin.math.floor

/**
 * What a villager is doing at this hour.
 *
 * parity: the `villager_activity` track of the `minecraft:overworld` timeline, keyframe for
 * keyframe. A villager here runs on goals rather than on a brain, so the schedule is read by the
 * goals that care about it instead of swapping whole behaviour packages in and out.
 */
public object VillagerSchedule {
    public enum class Activity {
        IDLE,
        WORK,
        MEET,
        REST,
    }

    public fun activityAt(dayTime: Long): Activity {
        val time = (dayTime % DAY).toInt()
        return when {
            time < WORK_START -> Activity.IDLE
            time < MEET_START -> Activity.WORK
            time < EVENING_START -> Activity.MEET
            time < REST_START -> Activity.IDLE
            else -> Activity.REST
        }
    }

    private const val DAY = 24000L
    private const val WORK_START = 2000
    private const val MEET_START = 9000
    private const val EVENING_START = 11000
    private const val REST_START = 12000
}

/** The blocks a village is made of, as far as the goals that walk to one are concerned. */
public object VillageBlocks {
    /** parity: `PoiTypes`, minus the professions this build does not hand out. */
    public val WORKSTATIONS: Set<String> =
        setOf(
            "composter",
            "smithing_table",
            "barrel",
            "blast_furnace",
            "brewing_stand",
            "cauldron",
            "cartography_table",
            "fletching_table",
            "grindstone",
            "lectern",
            "loom",
            "smoker",
            "stonecutter",
        )

    public val BELLS: Set<String> = setOf("bell")

    public val BEDS: Set<String> =
        setOf(
            "white_bed",
            "orange_bed",
            "magenta_bed",
            "light_blue_bed",
            "yellow_bed",
            "lime_bed",
            "pink_bed",
            "gray_bed",
            "light_gray_bed",
            "cyan_bed",
            "purple_bed",
            "blue_bed",
            "brown_bed",
            "green_bed",
            "red_bed",
            "black_bed",
        )

    /** The nearest block of any of these names, or null. Answers block coordinates, not a middle. */
    public fun findNearest(
        world: WorldState,
        fromX: Double,
        fromY: Double,
        fromZ: Double,
        names: Set<String>,
        range: Int,
        height: Int,
    ): IntArray? {
        val originX = floor(fromX).toInt()
        val originY = floor(fromY).toInt()
        val originZ = floor(fromZ).toInt()
        var best: IntArray? = null
        var bestDistance = Int.MAX_VALUE
        for (dy in -height..height) {
            for (dx in -range..range) {
                for (dz in -range..range) {
                    val distance = dx * dx + dy * dy + dz * dz
                    if (distance >= bestDistance) continue
                    val x = originX + dx
                    val y = originY + dy
                    val z = originZ + dz
                    if (!matches(world, x, y, z, names)) continue
                    best = intArrayOf(x, y, z)
                    bestDistance = distance
                }
            }
        }
        return best
    }

    public fun matches(
        world: WorldState,
        x: Int,
        y: Int,
        z: Int,
        names: Set<String>,
    ): Boolean {
        val state = world.getBlockState(x, y, z) ?: return false
        val path = Blocks.byStateId(state)?.block?.path ?: return false
        return path in names
    }
}

/**
 * Walks to a block of a given kind and stays by it.
 *
 * parity: the `walk_to_target` half of the villager's work, meet and rest packages, which is all
 * those packages do that anybody can see from outside.
 */
public open class WalkToBlockGoal(
    private val speed: Double,
    private val names: Set<String>,
    private val range: Int,
    private val height: Int = HEIGHT,
) : Goal() {
    override val flags: Set<GoalFlag> = setOf(GoalFlag.MOVE, GoalFlag.LOOK)

    override val requiresUpdateEveryTick: Boolean = true

    private var targetX = 0
    private var targetY = 0
    private var targetZ = 0
    private var found = false
    private var searchCooldown = 0
    private var arrived = false
    private var walkX = 0.0
    private var walkZ = 0.0

    /** Whether this hour, or this raid, is one where the villager wants to be there at all. */
    protected open fun wanted(mob: MobContext): Boolean = true

    /** Called the tick the mob reaches the block, and again the tick it gives up on it. */
    protected open fun onArrived(
        mob: MobContext,
        arrived: Boolean,
    ) {
    }

    override fun canUse(mob: MobContext): Boolean {
        if (!wanted(mob)) return false
        if (found && VillageBlocks.matches(mob.world, targetX, targetY, targetZ, names)) return true
        found = false
        if (searchCooldown > 0) {
            searchCooldown--
            return false
        }
        searchCooldown = SEARCH_INTERVAL
        val position = VillageBlocks.findNearest(mob.world, mob.mob.x, mob.mob.y, mob.mob.z, names, range, height)
        if (position == null) return false
        targetX = position[0]
        targetY = position[1]
        targetZ = position[2]
        found = true
        return true
    }

    override fun canContinueToUse(mob: MobContext): Boolean =
        found && wanted(mob) && VillageBlocks.matches(mob.world, targetX, targetY, targetZ, names)

    override fun start(mob: MobContext) {
        arrived = false
        // A bed or a workstation is not something a mob can stand in, so it walks to the nearest
        // spot beside it and counts that as being there.
        val spot = standingSpot(mob) ?: return
        walkX = spot[0]
        walkZ = spot[1]
        Navigation.moveTo(mob.world, mob.mob, walkX, targetY.toDouble(), walkZ, speed)
    }

    override fun stop(mob: MobContext) {
        if (arrived) onArrived(mob, false)
        arrived = false
        Navigation.stop(mob.mob)
    }

    override fun tick(mob: MobContext) {
        LookControl.lookAt(mob.mob, targetX + 0.5, targetY + 0.5, targetZ + 0.5)
        val close = mob.distanceSquaredTo(targetX + 0.5, targetY.toDouble(), targetZ + 0.5) <= ARRIVED * ARRIVED
        if (close != arrived) {
            arrived = close
            onArrived(mob, close)
        }
        if (close) {
            Navigation.stop(mob.mob)
            return
        }
        if (!Navigation.isDone(mob.mob)) return
        if (mob.mob.repathCooldown > 0) {
            mob.mob.repathCooldown--
            return
        }
        mob.mob.repathCooldown = REPATH_INTERVAL
        if (!Navigation.moveTo(mob.world, mob.mob, walkX, targetY.toDouble(), walkZ, speed)) {
            found = false
        }
    }

    /** The middle of the block itself when it can be stood in, or of whichever side is free. */
    private fun standingSpot(mob: MobContext): DoubleArray? {
        if (isPassable(mob.world, targetX, targetY, targetZ)) {
            return doubleArrayOf(targetX + 0.5, targetZ + 0.5)
        }
        for (side in SIDES) {
            val x = targetX + side[0]
            val z = targetZ + side[1]
            if (!isPassable(mob.world, x, targetY, z)) continue
            return doubleArrayOf(x + 0.5, z + 0.5)
        }
        return null
    }

    private companion object {
        val SIDES = arrayOf(intArrayOf(1, 0), intArrayOf(-1, 0), intArrayOf(0, 1), intArrayOf(0, -1))
        const val SEARCH_INTERVAL = 100
        const val REPATH_INTERVAL = 20
        const val ARRIVED = 2.0
        const val HEIGHT = 4
    }
}

/** parity: the work package, which sends a villager to its own job site for the working hours. */
public class WorkAtSiteGoal(
    speed: Double,
) : WalkToBlockGoal(speed, VillageBlocks.WORKSTATIONS, RANGE) {
    override fun wanted(mob: MobContext): Boolean =
        !mob.raidNearby && VillagerSchedule.activityAt(mob.dayTime) == VillagerSchedule.Activity.WORK

    private companion object {
        const val RANGE = 16
    }
}

/** parity: the meet package, which gathers the village at the bell in the afternoon. */
public class MeetAtBellGoal(
    speed: Double,
) : WalkToBlockGoal(speed, VillageBlocks.BELLS, RANGE) {
    override fun wanted(mob: MobContext): Boolean =
        !mob.raidNearby && VillagerSchedule.activityAt(mob.dayTime) == VillagerSchedule.Activity.MEET

    private companion object {
        const val RANGE = 32
    }
}

/**
 * parity: the rest package, and the hide package it shares its bed with: a villager sleeps at night
 * and runs for the same bed while a raid is on, whatever the hour.
 */
public class SleepInBedGoal(
    speed: Double,
) : WalkToBlockGoal(speed, VillageBlocks.BEDS, RANGE) {
    override fun wanted(mob: MobContext): Boolean =
        mob.raidNearby || VillagerSchedule.activityAt(mob.dayTime) == VillagerSchedule.Activity.REST

    override fun onArrived(
        mob: MobContext,
        arrived: Boolean,
    ) {
        // parity: a villager hiding from a raid stands in the house rather than lying down.
        mob.actions.setSleeping(mob.mob, arrived && !mob.raidNearby)
    }

    private companion object {
        const val RANGE = 16
    }
}
