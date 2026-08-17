package org.basaltmc.server.runtime.scoreboard

import org.basaltmc.wire.protocol.play.DisplaySlot
import org.basaltmc.wire.protocol.play.NumberFormat
import org.basaltmc.wire.protocol.play.ObjectiveRenderType
import org.basaltmc.wire.text.Component

/**
 * One objective: a name commands refer to, a criterion, and how the client should draw it.
 *
 * The criterion is stored, not honoured: nothing here increments a score on its own yet, so every
 * objective behaves like `dummy` regardless of what it was created with. That is a documented gap,
 * and it is why `/scoreboard objectives add x deathCount` is accepted rather than refused.
 */
public class Objective(
    public val name: String,
    public val criterion: String,
    public var displayName: Component,
    public var renderType: ObjectiveRenderType = ObjectiveRenderType.INTEGER,
) {
    /** parity: `numberformat`, which decides how the number on the right is drawn, or not drawn. */
    public var numberFormat: NumberFormat? = null

    /** parity: `displayautoupdate`, off when a sidebar should only change when told to. */
    public var displayAutoUpdate: Boolean = true

    /** parity: `ObjectiveCriteria.isReadOnly`: a `trigger` objective is the one `/trigger` may touch. */
    public val isTrigger: Boolean get() = criterion == "trigger"

    public companion object {
        /** parity: `ObjectiveCriteria`'s own registrations, minus the per-statistic ones. */
        public val CRITERIA: List<String> =
            listOf(
                "dummy",
                "trigger",
                "deathCount",
                "playerKillCount",
                "totalKillCount",
                "health",
                "food",
                "air",
                "armor",
                "xp",
                "level",
            )
    }
}

/**
 * Objectives, scores and display slots for the whole server.
 *
 * Score holders are names, not players: vanilla scores anything with a name, including entities
 * that are not online and "fake players" a datapack invents, and `/scoreboard players set #total x
 * 5` is the ordinary way to keep a counter.
 */
public class Scoreboard {
    private val objectives = linkedMapOf<String, Objective>()
    private val scores = mutableMapOf<String, MutableMap<String, Int>>()
    private val displaySlots = mutableMapOf<DisplaySlot, String>()

    /** Which holders may use `/trigger` on which objective, and whether it is currently enabled. */
    private val enabledTriggers = mutableSetOf<Pair<String, String>>()

    public val allObjectives: List<Objective> get() = objectives.values.toList()

    public operator fun get(name: String): Objective? = objectives[name]

    public fun add(objective: Objective): Boolean {
        if (objective.name in objectives) return false
        objectives[objective.name] = objective
        return true
    }

    public fun remove(name: String): Objective? {
        val removed = objectives.remove(name) ?: return null
        for (holder in scores.values) holder.remove(name)
        for (slot in displaySlots.filterValues { it == name }.keys.toList()) displaySlots.remove(slot)
        enabledTriggers.removeAll { it.second == name }
        return removed
    }

    public fun displayed(slot: DisplaySlot): String? = displaySlots[slot]

    public fun display(
        slot: DisplaySlot,
        objectiveName: String?,
    ) {
        if (objectiveName == null) displaySlots.remove(slot) else displaySlots[slot] = objectiveName
    }

    public fun score(
        holder: String,
        objectiveName: String,
    ): Int? = scores[holder]?.get(objectiveName)

    /** Per-holder overrides `/scoreboard players display` sets, keyed by holder and objective. */
    private val displayNames = mutableMapOf<Pair<String, String>, Component?>()
    private val scoreFormats = mutableMapOf<Pair<String, String>, NumberFormat?>()

    public fun displayNameOf(
        holder: String,
        objectiveName: String,
    ): Component? = displayNames[holder to objectiveName]

    public fun setDisplayName(
        holder: String,
        objectiveName: String,
        name: Component?,
    ) {
        displayNames[holder to objectiveName] = name
    }

    public fun numberFormatOf(
        holder: String,
        objectiveName: String,
    ): NumberFormat? = scoreFormats[holder to objectiveName]

    public fun setNumberFormat(
        holder: String,
        objectiveName: String,
        format: NumberFormat?,
    ) {
        scoreFormats[holder to objectiveName] = format
    }

    public fun setScore(
        holder: String,
        objectiveName: String,
        value: Int,
    ) {
        scores.getOrPut(holder) { mutableMapOf() }[objectiveName] = value
    }

    public fun addScore(
        holder: String,
        objectiveName: String,
        delta: Int,
    ): Int {
        val next = (score(holder, objectiveName) ?: 0).toLong() + delta
        val clamped = next.coerceIn(Int.MIN_VALUE.toLong(), Int.MAX_VALUE.toLong()).toInt()
        setScore(holder, objectiveName, clamped)
        return clamped
    }

    public fun resetScore(
        holder: String,
        objectiveName: String?,
    ) {
        if (objectiveName == null) scores.remove(holder) else scores[holder]?.remove(objectiveName)
    }

    /** Every holder with a score in [objectiveName], in insertion order, which is what `/scoreboard players list` prints. */
    public fun holdersIn(objectiveName: String): List<String> =
        scores.filterValues { objectiveName in it }.keys.toList()

    public fun holders(): List<String> = scores.keys.toList()

    public fun scoresOf(holder: String): Map<String, Int> = scores[holder].orEmpty()

    public fun enableTrigger(
        holder: String,
        objectiveName: String,
    ) {
        enabledTriggers += holder to objectiveName
    }

    public fun isTriggerEnabled(
        holder: String,
        objectiveName: String,
    ): Boolean = (holder to objectiveName) in enabledTriggers

    public fun disableTrigger(
        holder: String,
        objectiveName: String,
    ) {
        enabledTriggers -= holder to objectiveName
    }
}
