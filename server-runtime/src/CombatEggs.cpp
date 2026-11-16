#include <basalt/server/plugins/PluginEventTypes.hpp>
#include <basalt/server/plugins/PluginHookEventTypes.hpp>
#include <basalt/server/runtime/ClientSession.hpp>
#include <basalt/server/runtime/CombatSystem.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/plugins/EventSink.hpp>
#include <basalt/server/runtime/plugins/HookEvents.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/wire/protocol/play/PlayPackets.hpp>

#include <algorithm>
#include <string>
#include <string_view>

namespace basalt::server::runtime {

using player::PlayerSession;

namespace {

constexpr std::int32_t kHatchOdds = 8;
constexpr std::int32_t kQuadrupleOdds = 32;
constexpr std::int32_t kMaxHatches = 127;

[[nodiscard]] std::string_view variantOf(std::string_view egg) {
    if (egg == "blue_egg") {
        return "minecraft:cold";
    }
    return egg == "brown_egg" ? "minecraft:warm" : "minecraft:temperate";
}

} // namespace

void CombatSystem::hatchEgg(world::LoadedWorld& world, const std::vector<PlayerSession*>& watchers,
                            entity::ServerEntity& egg) {
    if (!options_.nextInt || !options_.spawnHatched) {
        return;
    }
    // parity: `ThrownEgg.onHit` rolls the hatch first and the quadruple second, whatever the first said.
    bool hatching = options_.nextInt(kHatchOdds) == 0;
    std::int32_t count = options_.nextInt(kQuadrupleOdds) == 0 ? 4 : 1;
    if (!hatching) {
        count = 0;
    }
    std::string type = "minecraft:chicken";
    const auto shooter = std::ranges::find_if(watchers, [&egg](const PlayerSession* who) {
        return egg.ownerId.has_value() && who->entityId() == *egg.ownerId;
    });
    if (shooter != watchers.end()) {
        const PlayerSession& thrower = **shooter;
        if (const server::plugins::PluginEvent* answered =
                world.events().ask(&server::plugins::pluginEventTypes::playerEggThrow,
                                   [&](server::plugins::PluginEvent& event) {
                                       plugins::EventSink::player(event, thrower);
                                       event.set("entity_id", static_cast<std::int64_t>(egg.entityId()));
                                       event.set("hatching", hatching);
                                       event.set("count", static_cast<std::int64_t>(count));
                                       event.set("hatch_type", type);
                                   });
            answered != nullptr) {
            if (const auto* edited = std::get_if<bool>(&answered->get("hatching")); edited != nullptr) {
                hatching = *edited;
            }
            if (const auto* edited = std::get_if<std::int64_t>(&answered->get("count")); edited != nullptr) {
                count = static_cast<std::int32_t>(std::clamp<std::int64_t>(*edited, 0, kMaxHatches));
            }
            if (const auto* edited = std::get_if<std::string>(&answered->get("hatch_type")); edited != nullptr) {
                type = *edited;
            }
            if (!hatching) {
                count = 0;
            }
        }
    }
    plugins::hookEvents::eggHatch(world.events(), world.name(), egg, hatching, count, type);
    if (const auto hatched = wire::common::Identifier::parse(type); hatched.has_value()) {
        for (std::int32_t index = 0; index < count; ++index) {
            options_.spawnHatched(world, *hatched, egg.x, egg.y, egg.z, egg.yaw,
                                  variantOf(egg.heldItem.value_or(std::string{})));
        }
    }
    for (PlayerSession* watcher : watchers) {
        watcher->connection().sendPlayPacket(
            wire::protocol::play::EntityEventPacket::kId, [&egg](wire::io::ByteBuffer& buffer) {
                wire::protocol::play::EntityEventPacket{.entityId = egg.entityId(), .eventId = 3}.write(buffer);
            });
    }
}

} // namespace basalt::server::runtime
