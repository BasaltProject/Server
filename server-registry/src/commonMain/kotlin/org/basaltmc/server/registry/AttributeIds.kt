package org.basaltmc.server.registry

/**
 * The `minecraft:attribute` registry: every attribute and the number it travels as.
 *
 * A protocol fact, taken from the jar's own registry report, like the block and item tables beside
 * it. It is here rather than hard-coded where it is used because it was hard-coded once already and
 * one of the six numbers was wrong: `attack_speed` was being sent as `attack_knockback`, which is a
 * mistake nobody spots by reading the code.
 */
public object AttributeIds {
    private val byName: Map<String, Int> =
        mapOf(
            "minecraft:air_drag_modifier" to 0,
            "minecraft:armor" to 1,
            "minecraft:armor_toughness" to 2,
            "minecraft:attack_damage" to 3,
            "minecraft:attack_knockback" to 4,
            "minecraft:attack_speed" to 5,
            "minecraft:below_name_distance" to 6,
            "minecraft:block_break_speed" to 7,
            "minecraft:block_interaction_range" to 8,
            "minecraft:bounciness" to 9,
            "minecraft:burning_time" to 10,
            "minecraft:camera_distance" to 11,
            "minecraft:explosion_knockback_resistance" to 12,
            "minecraft:entity_interaction_range" to 13,
            "minecraft:fall_damage_multiplier" to 14,
            "minecraft:flying_speed" to 15,
            "minecraft:follow_range" to 16,
            "minecraft:friction_modifier" to 17,
            "minecraft:gravity" to 18,
            "minecraft:jump_strength" to 19,
            "minecraft:knockback_resistance" to 20,
            "minecraft:luck" to 21,
            "minecraft:max_absorption" to 22,
            "minecraft:max_health" to 23,
            "minecraft:mining_efficiency" to 24,
            "minecraft:movement_efficiency" to 25,
            "minecraft:movement_speed" to 26,
            "minecraft:name_tag_distance" to 27,
            "minecraft:oxygen_bonus" to 28,
            "minecraft:safe_fall_distance" to 29,
            "minecraft:scale" to 30,
            "minecraft:sneaking_speed" to 31,
            "minecraft:spawn_reinforcements" to 32,
            "minecraft:step_height" to 33,
            "minecraft:submerged_mining_speed" to 34,
            "minecraft:sweeping_damage_ratio" to 35,
            "minecraft:tempt_range" to 36,
            "minecraft:water_movement_efficiency" to 37,
            "minecraft:waypoint_transmit_range" to 38,
            "minecraft:waypoint_receive_range" to 39,
        )

    /** Accepts `max_health` and `minecraft:max_health` alike, since a plugin will write both. */
    public fun protocolId(name: String): Int? = byName[qualified(name)]

    public fun exists(name: String): Boolean = byName.containsKey(qualified(name))

    public fun all(): List<String> = byName.keys.sorted()

    private fun qualified(name: String): String = if (':' in name) name else "minecraft:$name"
}
