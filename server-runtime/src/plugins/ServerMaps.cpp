#include <basalt/server/runtime/MapSystem.hpp>
#include <basalt/server/runtime/plugins/ServerMaps.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/wire/common/json/Json.hpp>
#include <basalt/wire/text/ComponentJson.hpp>

namespace basalt::server::runtime::plugins {
namespace {

using server::plugins::PluginMap;
using server::plugins::PluginMapCursor;

constexpr std::string_view kFilledMap = "minecraft:filled_map";

[[nodiscard]] wire::text::Component fromJson(const std::string& text) {
    const auto parsed = wire::common::parseJson(text);
    return parsed.has_value() ? wire::text::componentJson::deserialize(*parsed) : wire::text::Component::text(text);
}

} // namespace

std::optional<PluginMap> ServerMaps::map(std::int32_t id) {
    if (hooks_.storage == nullptr) {
        return std::nullopt;
    }
    const std::unique_lock<std::mutex> lock = hooks_.storage->lock();
    const world::maps::MapData* data = hooks_.storage->get(id);
    if (data == nullptr) {
        return std::nullopt;
    }
    return PluginMap{.id = id,
                     .dimension = data->dimension(),
                     .scale = data->scale(),
                     .centerX = data->centerX(),
                     .centerZ = data->centerZ(),
                     .tracking = data->trackingPosition(),
                     .unlimited = data->unlimitedTracking(),
                     .locked = data->isLocked()};
}

std::int32_t ServerMaps::create(std::string_view world, std::int32_t centerX, std::int32_t centerZ,
                                std::int32_t scale) {
    world::LoadedWorld* found = hooks_.worldFor ? hooks_.worldFor(std::string{world}) : nullptr;
    if (found == nullptr || hooks_.storage == nullptr) {
        return -1;
    }
    const std::unique_lock<std::mutex> lock = hooks_.storage->lock();
    const std::int32_t id = hooks_.storage->nextId();
    hooks_.storage->set(id, world::maps::MapData::createFresh(centerX, centerZ, static_cast<std::int8_t>(scale), true,
                                                              false, found->levelKey()));
    return id;
}

bool ServerMaps::update(const PluginMap& changed) {
    if (hooks_.storage == nullptr) {
        return false;
    }
    const std::unique_lock<std::mutex> lock = hooks_.storage->lock();
    world::maps::MapData* data = hooks_.storage->get(changed.id);
    if (data == nullptr) {
        return false;
    }
    data->reframe(changed.centerX, changed.centerZ, static_cast<std::int8_t>(changed.scale), changed.tracking,
                  changed.unlimited, changed.locked, changed.dimension);
    return true;
}

std::vector<std::uint8_t> ServerMaps::colors(std::int32_t id) {
    if (hooks_.storage == nullptr) {
        return {};
    }
    const std::unique_lock<std::mutex> lock = hooks_.storage->lock();
    const world::maps::MapData* data = hooks_.storage->get(id);
    if (data == nullptr) {
        return {};
    }
    const std::span<const std::uint8_t> held = data->colors();
    return {held.begin(), held.end()};
}

std::vector<std::string> ServerMaps::carriers(std::int32_t id) {
    std::vector<std::string> found;
    for (const player::PlayerSession* session :
         hooks_.sessions ? hooks_.sessions() : std::vector<player::PlayerSession*>{}) {
        for (std::int32_t slot = 0; slot < storage::StoredInventory::kPlayerSlots; ++slot) {
            const std::optional<storage::StoredItemStack>& stack = session->inventory.at(slot);
            if (stack.has_value() && stack->id == kFilledMap && MapSystem::mapIdOf(stack->components) == id) {
                found.push_back(session->uuid().toString());
                break;
            }
        }
    }
    return found;
}

void ServerMaps::show(std::string_view uuid, std::int32_t id, std::span<const std::uint8_t> colors,
                      const std::vector<PluginMapCursor>& cursors) {
    const std::optional<PluginMap> known = map(id);
    if (!known.has_value() || colors.size() < server::plugins::PluginMaps::kPixels) {
        return;
    }
    for (const player::PlayerSession* session :
         hooks_.sessions ? hooks_.sessions() : std::vector<player::PlayerSession*>{}) {
        if (session->uuid().toString() != uuid) {
            continue;
        }
        wire::protocol::play::MapItemDataPacket packet;
        packet.mapId = id;
        packet.scale = static_cast<std::int8_t>(known->scale);
        packet.locked = known->locked;
        std::vector<wire::protocol::play::MapDecorationEntry> decorations;
        for (const PluginMapCursor& cursor : cursors) {
            wire::protocol::play::MapDecorationEntry entry{
                .type = cursor.type, .x = cursor.x, .y = cursor.y, .rotation = cursor.direction};
            if (cursor.caption.has_value()) {
                entry.name = fromJson(*cursor.caption);
            }
            decorations.push_back(std::move(entry));
        }
        packet.decorations = std::move(decorations);
        packet.patch = wire::protocol::play::MapColorPatch{
            .startX = 0,
            .startY = 0,
            .width = 128,
            .height = 128,
            .colors = std::vector<std::uint8_t>(colors.begin(), colors.begin() + server::plugins::PluginMaps::kPixels)};
        sendMapUpdate(*session, packet);
    }
}

} // namespace basalt::server::runtime::plugins
