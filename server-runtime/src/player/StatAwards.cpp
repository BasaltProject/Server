#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/player/StatAwards.hpp>
#include <basalt/server/storage/StoredPlayer.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>

#include <cmath>
#include <utility>

namespace basalt::server::runtime::player::statAwards {

std::optional<std::string_view> openStatOf(std::string_view path) {
    if (path == "trapped_chest") {
        return "trigger_trapped_chest";
    }
    // parity: `CopperChestBlock` extends `ChestBlock`.
    if (path == "chest" || path.ends_with("copper_chest")) {
        return "open_chest";
    }
    if (path.ends_with("shulker_box")) {
        return "open_shulker_box";
    }
    if (path.ends_with("anvil")) {
        return "interact_with_anvil";
    }
    static constexpr std::pair<std::string_view, std::string_view> kScreens[]{
        {"crafting_table", "interact_with_crafting_table"},
        {"furnace", "interact_with_furnace"},
        {"blast_furnace", "interact_with_blast_furnace"},
        {"smoker", "interact_with_smoker"},
        {"brewing_stand", "interact_with_brewingstand"},
        {"beacon", "interact_with_beacon"},
        {"barrel", "open_barrel"},
        {"dispenser", "inspect_dispenser"},
        {"dropper", "inspect_dropper"},
        {"hopper", "inspect_hopper"},
        {"ender_chest", "open_enderchest"},
        {"lectern", "interact_with_lectern"},
        {"smithing_table", "interact_with_smithing_table"},
        {"stonecutter", "interact_with_stonecutter"},
        {"cartography_table", "interact_with_cartography_table"},
        {"loom", "interact_with_loom"},
        {"grindstone", "interact_with_grindstone"},
    };
    for (const auto& [block, stat] : kScreens) {
        if (block == path) {
            return stat;
        }
    }
    return std::nullopt;
}

std::optional<std::string_view> ridingStatOf(const wire::common::Identifier& vehicle) {
    const std::string& path = vehicle.path();
    if (path == "minecart" || path.ends_with("_minecart")) {
        return "minecart_one_cm";
    }
    if (path.ends_with("_boat") || path.ends_with("_raft")) {
        return "boat_one_cm";
    }
    if (path == "pig") {
        return "pig_one_cm";
    }
    // parity: `AbstractHorse`.
    if (path == "horse" || path == "donkey" || path == "mule" || path == "llama" || path == "trader_llama" ||
        path == "camel" || path == "camel_husk" || path == "skeleton_horse" || path == "zombie_horse") {
        return "horse_one_cm";
    }
    if (path == "strider") {
        return "strider_one_cm";
    }
    if (path == "happy_ghast") {
        return "happy_ghast_one_cm";
    }
    if (path == "nautilus" || path == "zombie_nautilus") {
        return "nautilus_one_cm";
    }
    return std::nullopt;
}

void award(PlayerSession& player, std::string_view customName, std::int64_t amount) {
    player.stats.increment(storage::stats::custom(customName), amount);
}

void used(PlayerSession& player, std::string_view item) { player.stats.increment(storage::stats::used(item)); }

void awardThrown(PlayerSession& player, std::string_view item, std::int32_t count) {
    if (count <= 0) {
        return;
    }
    player.stats.increment(storage::stats::dropped(item), count);
    award(player, "drop");
}

void itemBroken(PlayerSession& player, std::string_view item, std::int8_t breakEvent, const PlayerRegistry* registry) {
    player.stats.increment(storage::stats::broken(item));
    if (registry == nullptr) {
        return;
    }
    const wire::protocol::play::EntityEventPacket packet{.entityId = player.entityId(), .eventId = breakEvent};
    for (const PlayerSession* watcher : registry->snapshot()) {
        if (watcher->worldName == player.worldName && watcher->connection().sendPlayPacket) {
            watcher->connection().sendPlayPacket(wire::protocol::play::EntityEventPacket::kId,
                                                 [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
        }
    }
}

void awardRiding(PlayerSession& player, const wire::common::Identifier& vehicle, double dx, double dy, double dz) {
    if (dx == 0.0 && dy == 0.0 && dz == 0.0) {
        return;
    }
    const std::optional<std::string_view> stat = ridingStatOf(vehicle);
    if (!stat.has_value()) {
        return;
    }
    // parity: `Math.round((float)Math.sqrt(...) * 100.0F)`, the float rounding half up.
    const float distance = static_cast<float>(std::sqrt(dx * dx + dy * dy + dz * dz)) * 100.0F;
    award(player, *stat, static_cast<std::int64_t>(std::floor(distance + 0.5F)));
}

} // namespace basalt::server::runtime::player::statAwards
