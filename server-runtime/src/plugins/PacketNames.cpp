#include <basalt/server/runtime/plugins/PacketNames.hpp>

#include <algorithm>
#include <array>
#include <map>

namespace basalt::server::runtime::plugins::packetNames {
namespace {

struct Entry {
    wire::protocol::ProtocolState state = wire::protocol::ProtocolState::Play;
    bool serverbound = false;
    std::int32_t id = 0;
    std::string_view name;
};

constexpr std::array kEntries{
    Entry{wire::protocol::ProtocolState::Status, false, 0x00, "status_response"},
    Entry{wire::protocol::ProtocolState::Status, false, 0x01, "pong_response"},
    Entry{wire::protocol::ProtocolState::Status, true, 0x00, "status_request"},
    Entry{wire::protocol::ProtocolState::Status, true, 0x01, "ping_request"},
    Entry{wire::protocol::ProtocolState::Login, false, 0x00, "login_disconnect"},
    Entry{wire::protocol::ProtocolState::Login, false, 0x01, "hello"},
    Entry{wire::protocol::ProtocolState::Login, false, 0x02, "login_finished"},
    Entry{wire::protocol::ProtocolState::Login, false, 0x03, "login_compression"},
    Entry{wire::protocol::ProtocolState::Login, true, 0x00, "login_start"},
    Entry{wire::protocol::ProtocolState::Login, true, 0x01, "key"},
    Entry{wire::protocol::ProtocolState::Login, true, 0x03, "login_acknowledged"},
    Entry{wire::protocol::ProtocolState::Configuration, false, 0x01, "brand"},
    Entry{wire::protocol::ProtocolState::Configuration, false, 0x03, "finish_configuration"},
    Entry{wire::protocol::ProtocolState::Configuration, false, 0x07, "registry_data"},
    Entry{wire::protocol::ProtocolState::Configuration, false, 0x0d, "tags"},
    Entry{wire::protocol::ProtocolState::Configuration, false, 0x0e, "select_known_packs"},
    Entry{wire::protocol::ProtocolState::Configuration, true, 0x00, "client_information"},
    Entry{wire::protocol::ProtocolState::Configuration, true, 0x03, "finish_configuration"},
    Entry{wire::protocol::ProtocolState::Configuration, true, 0x07, "select_known_packs"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x01, "spawn_entity"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x02, "animate"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x03, "award_stats"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x04, "block_changed_ack"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x05, "block_destruction"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x06, "block_entity_data"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x07, "block_event"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x08, "block_update"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x09, "boss_event"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x0a, "change_difficulty"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x0b, "chunk_batch_finished"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x0c, "chunk_batch_start"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x0d, "chunks_biomes"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x0e, "clear_titles"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x0f, "command_suggestions"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x10, "commands"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x11, "container_close"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x12, "container_set_content"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x13, "container_set_data"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x14, "container_set_slot"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x16, "cooldown"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x19, "damage_event"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x20, "play_disconnect"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x22, "entity_event"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x23, "entity_position_sync"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x24, "explode"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x25, "forget_level_chunk"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x26, "game_event"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x2a, "hurt_animation"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x2c, "play_keep_alive"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x2d, "map_chunk"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x2e, "level_event"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x2f, "level_particles"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x30, "update_light"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x31, "login_play"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x33, "merchant_offers"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x3b, "open_screen"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x3c, "open_sign_editor"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x40, "player_abilities"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x44, "player_combat_kill"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x46, "player_info_update"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x48, "player_position"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x4a, "recipe_book_add"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x4b, "recipe_book_remove"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x4d, "remove_entities"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x4e, "remove_mob_effect"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x4f, "reset_score"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x52, "respawn"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x53, "rotate_head"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x54, "section_blocks_update"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x57, "set_action_bar_text"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x58, "set_border_center"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x59, "set_border_lerp_size"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x5a, "set_border_size"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x5b, "set_border_warning_delay"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x5c, "set_border_warning_distance"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x5d, "set_camera"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x5e, "set_chunk_cache_center"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x5f, "set_chunk_cache_radius"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x61, "spawn_position"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x62, "set_display_objective"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x63, "set_entity_data"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x64, "set_entity_link"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x65, "set_entity_motion"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x66, "set_equipment"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x67, "set_experience"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x68, "set_health"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x69, "set_held_slot"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x6a, "set_objective"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x6b, "set_passengers"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x6d, "set_player_team"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x6e, "set_score"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x6f, "set_simulation_distance"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x70, "set_subtitle_text"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x71, "set_time"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x72, "set_title_text"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x73, "set_titles_animation"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x75, "sound"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x77, "stop_sound"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x18, "custom_payload"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x79, "system_chat"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x7a, "tab_list"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x7c, "take_item_entity"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x7d, "entity_teleport"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x7f, "ticking_state"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x80, "ticking_step"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x81, "transfer"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x83, "update_attributes"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x84, "update_mob_effect"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x85, "update_recipes"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x8a, "tracked_waypoint"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x8b, "clear_dialog"},
    Entry{wire::protocol::ProtocolState::Play, false, 0x8c, "show_dialog"},
    Entry{wire::protocol::ProtocolState::Play, true, 0x01, "attack"},
    Entry{wire::protocol::ProtocolState::Play, true, 0x07, "chat_command"},
    Entry{wire::protocol::ProtocolState::Play, true, 0x16, "custom_payload"},
    Entry{wire::protocol::ProtocolState::Play, true, 0x08, "signed_chat_command"},
    Entry{wire::protocol::ProtocolState::Play, true, 0x09, "chat_message"},
    Entry{wire::protocol::ProtocolState::Play, true, 0x0b, "chunk_batch_received"},
    Entry{wire::protocol::ProtocolState::Play, true, 0x0c, "client_command"},
    Entry{wire::protocol::ProtocolState::Play, true, 0x0f, "command_suggestion_request"},
    Entry{wire::protocol::ProtocolState::Play, true, 0x11, "container_button_click"},
    Entry{wire::protocol::ProtocolState::Play, true, 0x12, "container_click"},
    Entry{wire::protocol::ProtocolState::Play, true, 0x13, "container_close"},
    Entry{wire::protocol::ProtocolState::Play, true, 0x1a, "interact"},
    Entry{wire::protocol::ProtocolState::Play, true, 0x1c, "play_keep_alive"},
    Entry{wire::protocol::ProtocolState::Play, true, 0x1e, "position"},
    Entry{wire::protocol::ProtocolState::Play, true, 0x1f, "position_look"},
    Entry{wire::protocol::ProtocolState::Play, true, 0x24, "pick_item_from_block"},
    Entry{wire::protocol::ProtocolState::Play, true, 0x25, "pick_item_from_entity"},
    Entry{wire::protocol::ProtocolState::Play, true, 0x28, "player_abilities"},
    Entry{wire::protocol::ProtocolState::Play, true, 0x29, "player_action"},
    Entry{wire::protocol::ProtocolState::Play, true, 0x2a, "player_command"},
    Entry{wire::protocol::ProtocolState::Play, true, 0x33, "select_trade"},
    Entry{wire::protocol::ProtocolState::Play, true, 0x35, "set_carried_item"},
    Entry{wire::protocol::ProtocolState::Play, true, 0x38, "set_creative_mode_slot"},
    Entry{wire::protocol::ProtocolState::Play, true, 0x3d, "sign_update"},
    Entry{wire::protocol::ProtocolState::Play, true, 0x42, "use_item_on"},
    Entry{wire::protocol::ProtocolState::Play, true, 0x43, "use_item"},
};

[[nodiscard]] constexpr std::int32_t keyOf(wire::protocol::ProtocolState state, bool serverbound, std::int32_t id) {
    return (static_cast<std::int32_t>(state) << 17) | (serverbound ? 1 << 16 : 0) | id;
}

[[nodiscard]] std::string hex(std::int32_t id) {
    constexpr std::string_view kDigits = "0123456789abcdef";
    std::string written;
    written += kDigits[static_cast<std::size_t>((id >> 4) & 0xF)];
    written += kDigits[static_cast<std::size_t>(id & 0xF)];
    return written;
}

[[nodiscard]] std::string qualified(const Entry& entry) {
    return std::string{entry.serverbound ? "serverbound." : "clientbound."} + std::string{entry.name};
}

[[nodiscard]] const std::map<std::string, const Entry*, std::less<>>& byName() {
    static const std::map<std::string, const Entry*, std::less<>> kByName = [] {
        std::map<std::string, const Entry*, std::less<>> built;
        for (const Entry& entry : kEntries) {
            built.emplace(qualified(entry), &entry);
        }
        return built;
    }();
    return kByName;
}

} // namespace

std::string of(wire::protocol::ProtocolState state, bool serverbound, std::int32_t id) {
    const std::string side = serverbound ? "serverbound" : "clientbound";
    const std::int32_t key = keyOf(state, serverbound, id);
    for (const Entry& entry : kEntries) {
        if (keyOf(entry.state, entry.serverbound, entry.id) == key) {
            return side + "." + std::string{entry.name};
        }
    }
    return side + ".0x" + hex(id);
}

std::optional<std::pair<wire::protocol::ProtocolState, std::int32_t>> idOf(std::string_view name) {
    const auto found = byName().find(name);
    if (found == byName().end()) {
        return std::nullopt;
    }
    return std::pair{found->second->state, found->second->id};
}

bool isKnown(std::string_view name) { return byName().contains(name); }

std::vector<std::string> all() {
    std::vector<std::string> names;
    names.reserve(byName().size());
    for (const auto& [name, entry] : byName()) {
        names.push_back(name);
    }
    return names;
}

} // namespace basalt::server::runtime::plugins::packetNames
