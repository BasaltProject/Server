package org.basaltmc.server.runtime.world

import org.basaltmc.wire.common.Identifier

/** One status effect on a player: what it is, how strong, and how long it has left. */
public data class ActiveEffect(
    val effect: Identifier,
    val amplifier: Int,
    var remainingTicks: Int,
    val ambient: Boolean = false,
    val visible: Boolean = true,
    val showIcon: Boolean = true,
) {
    /** parity: an infinite effect is `-1` on the wire and never counts down. */
    public val infinite: Boolean get() = remainingTicks < 0
}

/**
 * The `minecraft:mob_effect` registry, which is what the effect packets carry.
 *
 * Ids rather than names: the packets send the registry id, and the client resolves it against its
 * own built-in registry, so this table is protocol fact and not content.
 */
public object MobEffects {
    private val IDS: Map<String, Int> =
        mapOf(
            "speed" to 0,
            "slowness" to 1,
            "haste" to 2,
            "mining_fatigue" to 3,
            "strength" to 4,
            "instant_health" to 5,
            "instant_damage" to 6,
            "jump_boost" to 7,
            "nausea" to 8,
            "regeneration" to 9,
            "resistance" to 10,
            "fire_resistance" to 11,
            "water_breathing" to 12,
            "invisibility" to 13,
            "blindness" to 14,
            "night_vision" to 15,
            "hunger" to 16,
            "weakness" to 17,
            "poison" to 18,
            "wither" to 19,
            "health_boost" to 20,
            "absorption" to 21,
            "saturation" to 22,
            "glowing" to 23,
            "levitation" to 24,
            "luck" to 25,
            "unluck" to 26,
            "slow_falling" to 27,
            "conduit_power" to 28,
            "dolphins_grace" to 29,
            "bad_omen" to 30,
            "hero_of_the_village" to 31,
            "darkness" to 32,
            "trial_omen" to 33,
            "raid_omen" to 34,
            "wind_charged" to 35,
            "weaving" to 36,
            "oozing" to 37,
            "infested" to 38,
            "breath_of_the_nautilus" to 39,
        )

    private val BY_ID: Map<Int, String> = IDS.entries.associate { (name, id) -> id to name }

    public fun protocolIdOf(effect: Identifier): Int? = IDS[effect.path]?.takeIf { effect.namespace == "minecraft" }

    public fun byProtocolId(id: Int): Identifier? = BY_ID[id]?.let(Identifier::vanilla)

    public val ids: List<Identifier> get() = IDS.keys.map(Identifier::vanilla)

    /** parity: the four that act on a tick rather than by being present. */
    public val REGENERATION: Identifier = Identifier.vanilla("regeneration")
    public val POISON: Identifier = Identifier.vanilla("poison")
    public val WITHER: Identifier = Identifier.vanilla("wither")
    public val HUNGER: Identifier = Identifier.vanilla("hunger")

    /** parity: the two that apply once, the moment they are given. */
    public val INSTANT_HEALTH: Identifier = Identifier.vanilla("instant_health")
    public val INSTANT_DAMAGE: Identifier = Identifier.vanilla("instant_damage")

    public val HASTE: Identifier = Identifier.vanilla("haste")
    public val MINING_FATIGUE: Identifier = Identifier.vanilla("mining_fatigue")

    /** parity: the one that changes what a hit is worth rather than what it hits. */
    public val RESISTANCE: Identifier = Identifier.vanilla("resistance")
}
