package org.basaltmc.server.runtime.plugins

import org.basaltmc.wire.protocol.ProtocolState

/**
 * What each packet is called, so a plugin can name one without knowing its number.
 *
 * The protocol's own names with the direction in front, not a dictionary invented for plugins. The
 * direction comes from what this server does with each one: dispatched on the way in is serverbound,
 * sent is clientbound. A test walks the sources so a packet added later without a name here fails
 * the build rather than showing up as a number.
 */
internal object PacketNames {
    private class Entry(
        val state: ProtocolState,
        val serverbound: Boolean,
        val id: Int,
        val name: String,
    )

    private val entries: List<Entry> =
        listOf(
            Entry(ProtocolState.STATUS, false, 0x00, "status_response"),
            Entry(ProtocolState.STATUS, false, 0x01, "pong_response"),
            Entry(ProtocolState.STATUS, true, 0x00, "status_request"),
            Entry(ProtocolState.STATUS, true, 0x01, "ping_request"),
            Entry(ProtocolState.LOGIN, false, 0x00, "login_disconnect"),
            Entry(ProtocolState.LOGIN, false, 0x01, "hello"),
            Entry(ProtocolState.LOGIN, false, 0x02, "login_finished"),
            Entry(ProtocolState.LOGIN, false, 0x03, "login_compression"),
            Entry(ProtocolState.LOGIN, true, 0x00, "login_start"),
            Entry(ProtocolState.LOGIN, true, 0x01, "key"),
            Entry(ProtocolState.LOGIN, true, 0x03, "login_acknowledged"),
            Entry(ProtocolState.CONFIGURATION, false, 0x01, "brand"),
            Entry(ProtocolState.CONFIGURATION, false, 0x03, "finish_configuration"),
            Entry(ProtocolState.CONFIGURATION, false, 0x07, "registry_data"),
            Entry(ProtocolState.CONFIGURATION, false, 0x0d, "tags"),
            Entry(ProtocolState.CONFIGURATION, false, 0x0e, "select_known_packs"),
            Entry(ProtocolState.CONFIGURATION, true, 0x00, "client_information"),
            Entry(ProtocolState.CONFIGURATION, true, 0x03, "finish_configuration"),
            Entry(ProtocolState.CONFIGURATION, true, 0x07, "select_known_packs"),
            Entry(ProtocolState.PLAY, false, 0x01, "spawn_entity"),
            Entry(ProtocolState.PLAY, false, 0x02, "animate"),
            Entry(ProtocolState.PLAY, false, 0x03, "award_stats"),
            Entry(ProtocolState.PLAY, false, 0x04, "block_changed_ack"),
            Entry(ProtocolState.PLAY, false, 0x05, "block_destruction"),
            Entry(ProtocolState.PLAY, false, 0x06, "block_entity_data"),
            Entry(ProtocolState.PLAY, false, 0x07, "block_event"),
            Entry(ProtocolState.PLAY, false, 0x08, "block_update"),
            Entry(ProtocolState.PLAY, false, 0x09, "boss_event"),
            Entry(ProtocolState.PLAY, false, 0x0a, "change_difficulty"),
            Entry(ProtocolState.PLAY, false, 0x0b, "chunk_batch_finished"),
            Entry(ProtocolState.PLAY, false, 0x0c, "chunk_batch_start"),
            Entry(ProtocolState.PLAY, false, 0x0d, "chunks_biomes"),
            Entry(ProtocolState.PLAY, false, 0x0e, "clear_titles"),
            Entry(ProtocolState.PLAY, false, 0x0f, "command_suggestions"),
            Entry(ProtocolState.PLAY, false, 0x10, "commands"),
            Entry(ProtocolState.PLAY, false, 0x11, "container_close"),
            Entry(ProtocolState.PLAY, false, 0x12, "container_set_content"),
            Entry(ProtocolState.PLAY, false, 0x13, "container_set_data"),
            Entry(ProtocolState.PLAY, false, 0x14, "container_set_slot"),
            Entry(ProtocolState.PLAY, false, 0x16, "cooldown"),
            Entry(ProtocolState.PLAY, false, 0x19, "damage_event"),
            Entry(ProtocolState.PLAY, false, 0x20, "play_disconnect"),
            Entry(ProtocolState.PLAY, false, 0x22, "entity_event"),
            Entry(ProtocolState.PLAY, false, 0x23, "entity_position_sync"),
            Entry(ProtocolState.PLAY, false, 0x24, "explode"),
            Entry(ProtocolState.PLAY, false, 0x25, "forget_level_chunk"),
            Entry(ProtocolState.PLAY, false, 0x26, "game_event"),
            Entry(ProtocolState.PLAY, false, 0x2a, "hurt_animation"),
            Entry(ProtocolState.PLAY, false, 0x2c, "play_keep_alive"),
            Entry(ProtocolState.PLAY, false, 0x2d, "map_chunk"),
            Entry(ProtocolState.PLAY, false, 0x2e, "level_event"),
            Entry(ProtocolState.PLAY, false, 0x2f, "level_particles"),
            Entry(ProtocolState.PLAY, false, 0x30, "update_light"),
            Entry(ProtocolState.PLAY, false, 0x31, "login_play"),
            Entry(ProtocolState.PLAY, false, 0x33, "merchant_offers"),
            Entry(ProtocolState.PLAY, false, 0x3b, "open_screen"),
            Entry(ProtocolState.PLAY, false, 0x3c, "open_sign_editor"),
            Entry(ProtocolState.PLAY, false, 0x40, "player_abilities"),
            Entry(ProtocolState.PLAY, false, 0x44, "player_combat_kill"),
            Entry(ProtocolState.PLAY, false, 0x46, "player_info_update"),
            Entry(ProtocolState.PLAY, false, 0x48, "player_position"),
            Entry(ProtocolState.PLAY, false, 0x4a, "recipe_book_add"),
            Entry(ProtocolState.PLAY, false, 0x4b, "recipe_book_remove"),
            Entry(ProtocolState.PLAY, false, 0x4d, "remove_entities"),
            Entry(ProtocolState.PLAY, false, 0x4e, "remove_mob_effect"),
            Entry(ProtocolState.PLAY, false, 0x4f, "reset_score"),
            Entry(ProtocolState.PLAY, false, 0x52, "respawn"),
            Entry(ProtocolState.PLAY, false, 0x53, "rotate_head"),
            Entry(ProtocolState.PLAY, false, 0x54, "section_blocks_update"),
            Entry(ProtocolState.PLAY, false, 0x57, "set_action_bar_text"),
            Entry(ProtocolState.PLAY, false, 0x58, "set_border_center"),
            Entry(ProtocolState.PLAY, false, 0x59, "set_border_lerp_size"),
            Entry(ProtocolState.PLAY, false, 0x5a, "set_border_size"),
            Entry(ProtocolState.PLAY, false, 0x5b, "set_border_warning_delay"),
            Entry(ProtocolState.PLAY, false, 0x5c, "set_border_warning_distance"),
            Entry(ProtocolState.PLAY, false, 0x5d, "set_camera"),
            Entry(ProtocolState.PLAY, false, 0x5e, "set_chunk_cache_center"),
            Entry(ProtocolState.PLAY, false, 0x5f, "set_chunk_cache_radius"),
            Entry(ProtocolState.PLAY, false, 0x61, "spawn_position"),
            Entry(ProtocolState.PLAY, false, 0x62, "set_display_objective"),
            Entry(ProtocolState.PLAY, false, 0x63, "set_entity_data"),
            Entry(ProtocolState.PLAY, false, 0x64, "set_entity_link"),
            Entry(ProtocolState.PLAY, false, 0x65, "set_entity_motion"),
            Entry(ProtocolState.PLAY, false, 0x66, "set_equipment"),
            Entry(ProtocolState.PLAY, false, 0x67, "set_experience"),
            Entry(ProtocolState.PLAY, false, 0x68, "set_health"),
            Entry(ProtocolState.PLAY, false, 0x69, "set_held_slot"),
            Entry(ProtocolState.PLAY, false, 0x6a, "set_objective"),
            Entry(ProtocolState.PLAY, false, 0x6b, "set_passengers"),
            Entry(ProtocolState.PLAY, false, 0x6d, "set_player_team"),
            Entry(ProtocolState.PLAY, false, 0x6e, "set_score"),
            Entry(ProtocolState.PLAY, false, 0x6f, "set_simulation_distance"),
            Entry(ProtocolState.PLAY, false, 0x70, "set_subtitle_text"),
            Entry(ProtocolState.PLAY, false, 0x71, "set_time"),
            Entry(ProtocolState.PLAY, false, 0x72, "set_title_text"),
            Entry(ProtocolState.PLAY, false, 0x73, "set_titles_animation"),
            Entry(ProtocolState.PLAY, false, 0x75, "sound"),
            Entry(ProtocolState.PLAY, false, 0x77, "stop_sound"),
            Entry(ProtocolState.PLAY, false, 0x18, "custom_payload"),
            Entry(ProtocolState.PLAY, false, 0x79, "system_chat"),
            Entry(ProtocolState.PLAY, false, 0x7a, "tab_list"),
            Entry(ProtocolState.PLAY, false, 0x7c, "take_item_entity"),
            Entry(ProtocolState.PLAY, false, 0x7d, "entity_teleport"),
            Entry(ProtocolState.PLAY, false, 0x7f, "ticking_state"),
            Entry(ProtocolState.PLAY, false, 0x80, "ticking_step"),
            Entry(ProtocolState.PLAY, false, 0x81, "transfer"),
            Entry(ProtocolState.PLAY, false, 0x83, "update_attributes"),
            Entry(ProtocolState.PLAY, false, 0x84, "update_mob_effect"),
            Entry(ProtocolState.PLAY, false, 0x85, "update_recipes"),
            Entry(ProtocolState.PLAY, false, 0x8a, "tracked_waypoint"),
            Entry(ProtocolState.PLAY, false, 0x8b, "clear_dialog"),
            Entry(ProtocolState.PLAY, false, 0x8c, "show_dialog"),
            Entry(ProtocolState.PLAY, true, 0x01, "attack"),
            Entry(ProtocolState.PLAY, true, 0x07, "chat_command"),
            Entry(ProtocolState.PLAY, true, 0x16, "custom_payload"),
            Entry(ProtocolState.PLAY, true, 0x08, "signed_chat_command"),
            Entry(ProtocolState.PLAY, true, 0x09, "chat_message"),
            Entry(ProtocolState.PLAY, true, 0x0b, "chunk_batch_received"),
            Entry(ProtocolState.PLAY, true, 0x0c, "client_command"),
            Entry(ProtocolState.PLAY, true, 0x0f, "command_suggestion_request"),
            Entry(ProtocolState.PLAY, true, 0x11, "container_button_click"),
            Entry(ProtocolState.PLAY, true, 0x12, "container_click"),
            Entry(ProtocolState.PLAY, true, 0x13, "container_close"),
            Entry(ProtocolState.PLAY, true, 0x1a, "interact"),
            Entry(ProtocolState.PLAY, true, 0x1c, "play_keep_alive"),
            Entry(ProtocolState.PLAY, true, 0x1e, "position"),
            Entry(ProtocolState.PLAY, true, 0x1f, "position_look"),
            Entry(ProtocolState.PLAY, true, 0x24, "pick_item_from_block"),
            Entry(ProtocolState.PLAY, true, 0x25, "pick_item_from_entity"),
            Entry(ProtocolState.PLAY, true, 0x28, "player_abilities"),
            Entry(ProtocolState.PLAY, true, 0x29, "player_action"),
            Entry(ProtocolState.PLAY, true, 0x2a, "player_command"),
            Entry(ProtocolState.PLAY, true, 0x33, "select_trade"),
            Entry(ProtocolState.PLAY, true, 0x35, "set_carried_item"),
            Entry(ProtocolState.PLAY, true, 0x38, "set_creative_mode_slot"),
            Entry(ProtocolState.PLAY, true, 0x3d, "sign_update"),
            Entry(ProtocolState.PLAY, true, 0x42, "use_item_on"),
            Entry(ProtocolState.PLAY, true, 0x43, "use_item"),
        )

    private val byId: Map<Int, String> = entries.associate { key(it.state, it.serverbound, it.id) to it.name }

    private val byName: Map<String, Entry> =
        entries.associateBy { "${if (it.serverbound) "serverbound" else "clientbound"}.${it.name}" }

    private fun key(
        state: ProtocolState,
        serverbound: Boolean,
        id: Int,
    ): Int = (state.ordinal shl 17) or (if (serverbound) 1 shl 16 else 0) or id

    /** `clientbound.set_title_text`, or the number in hex when this build has no name for it. */
    fun of(
        state: ProtocolState,
        serverbound: Boolean,
        id: Int,
    ): String {
        val side = if (serverbound) "serverbound" else "clientbound"
        val name = byId[key(state, serverbound, id)] ?: return "$side.0x${hex(id)}"
        return "$side.$name"
    }

    /** The state and number behind a name, for sending one. */
    fun idOf(name: String): Pair<ProtocolState, Int>? = byName[name]?.let { it.state to it.id }

    fun isKnown(name: String): Boolean = byName.containsKey(name)

    /** Every name, for the error message when a plugin misspells one. */
    fun all(): List<String> = byName.keys.sorted()

    private fun hex(id: Int): String = id.toString(16).padStart(2, '0')
}
