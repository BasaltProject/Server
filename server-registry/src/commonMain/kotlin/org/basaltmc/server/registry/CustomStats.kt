package org.basaltmc.server.registry

/**
 * The `minecraft:custom_stat` registry, in its own order.
 *
 * parity: `Stats`, whose `makeCustomStat` calls are what fill that registry: the statistics screen
 * reads each counter by id, so the order is the protocol and not a detail.
 */
public object CustomStats {
    private val NAMES: List<String> =
        listOf(
            "leave_game",
            "play_time",
            "total_world_time",
            "time_since_death",
            "time_since_rest",
            "sneak_time",
            "walk_one_cm",
            "crouch_one_cm",
            "sprint_one_cm",
            "walk_on_water_one_cm",
            "fall_one_cm",
            "climb_one_cm",
            "fly_one_cm",
            "walk_under_water_one_cm",
            "minecart_one_cm",
            "boat_one_cm",
            "pig_one_cm",
            "happy_ghast_one_cm",
            "horse_one_cm",
            "aviate_one_cm",
            "swim_one_cm",
            "strider_one_cm",
            "nautilus_one_cm",
            "jump",
            "drop",
            "damage_dealt",
            "damage_dealt_absorbed",
            "damage_dealt_resisted",
            "damage_taken",
            "damage_blocked_by_shield",
            "damage_absorbed",
            "damage_resisted",
            "deaths",
            "mob_kills",
            "animals_bred",
            "player_kills",
            "fish_caught",
            "talked_to_villager",
            "traded_with_villager",
            "eat_cake_slice",
            "fill_cauldron",
            "use_cauldron",
            "clean_armor",
            "clean_banner",
            "clean_shulker_box",
            "interact_with_brewingstand",
            "interact_with_beacon",
            "inspect_dropper",
            "inspect_hopper",
            "inspect_dispenser",
            "play_noteblock",
            "tune_noteblock",
            "pot_flower",
            "trigger_trapped_chest",
            "open_enderchest",
            "enchant_item",
            "play_record",
            "interact_with_furnace",
            "interact_with_crafting_table",
            "open_chest",
            "sleep_in_bed",
            "open_shulker_box",
            "open_barrel",
            "interact_with_blast_furnace",
            "interact_with_smoker",
            "interact_with_lectern",
            "interact_with_campfire",
            "interact_with_cartography_table",
            "interact_with_loom",
            "interact_with_stonecutter",
            "bell_ring",
            "raid_trigger",
            "raid_win",
            "interact_with_anvil",
            "interact_with_grindstone",
            "target_hit",
            "interact_with_smithing_table",
        )

    /** The registry id of a custom statistic, or null when this build does not know the name. */
    public fun idOf(name: String): Int? = NAMES.indexOf(name).takeIf { it >= 0 }

    public fun nameOf(id: Int): String? = NAMES.getOrNull(id)

    public val count: Int get() = NAMES.size
}
