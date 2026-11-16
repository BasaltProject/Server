#include <basalt/server/plugins/PluginEventTypes.hpp>
#include <basalt/server/plugins/PluginHookEventTypes.hpp>
#include <basalt/server/runtime/ClientSession.hpp>
#include <basalt/server/runtime/CombatHandler.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/player/PlayerSounds.hpp>
#include <basalt/server/runtime/plugins/EventSink.hpp>
#include <basalt/server/runtime/world/AdvancementTriggers.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/TriggerFacts.hpp>

#include <array>
#include <optional>
#include <string>
#include <string_view>

namespace basalt::server::runtime {

using player::PlayerSession;

namespace {

constexpr std::array<std::string_view, 16> kDyeColors{"white", "orange", "magenta",    "light_blue", "yellow", "lime",
                                                      "pink",  "gray",   "light_gray", "cyan",       "purple", "blue",
                                                      "brown", "green",  "red",        "black"};

[[nodiscard]] std::optional<std::int32_t> colorOfName(std::string_view name) {
    for (std::size_t index = 0; index < kDyeColors.size(); ++index) {
        if (kDyeColors[index] == name) {
            return static_cast<std::int32_t>(index);
        }
    }
    return std::nullopt;
}

} // namespace

bool CombatHandler::dyeSheep(PlayerSession& session, entity::ServerEntity& sheep, const std::int32_t handSlot) {
    world::LoadedWorld* world = owner_->currentWorld();
    const std::optional<storage::StoredItemStack> held = session.inventory.at(handSlot);
    if (world == nullptr || !held.has_value() || !held->id.ends_with("_dye")) {
        return false;
    }
    const std::string_view itemName = std::string_view{held->id}.substr(held->id.find(':') + 1);
    const std::optional<std::int32_t> dye = colorOfName(itemName.substr(0, itemName.size() - 4));
    const bool sheared = sheep.sourceNbt.has_value() && sheep.sourceNbt->getBoolean("Sheared");
    const std::int32_t current = sheep.sourceNbt.has_value() ? (sheep.sourceNbt->getByte("Color") & 0x0F) : 0;
    // parity: `DyeItem.interactLivingEntity` only dyes a live, unshorn sheep to a colour it does not already wear.
    if (!dye.has_value() || sheep.dying || sheep.removed || sheared || *dye == current) {
        return false;
    }
    std::int32_t color = *dye;
    if (const server::plugins::PluginEvent* answered =
            world->events().ask(&server::plugins::pluginEventTypes::entitySheepDye,
                                [&](server::plugins::PluginEvent& event) {
                                    plugins::EventSink::player(event, session);
                                    event.set("entity_id", static_cast<std::int64_t>(sheep.entityId()));
                                    event.set("entity_type", sheep.type().toString());
                                    event.set("color", std::string{kDyeColors[static_cast<std::size_t>(color)]});
                                });
        answered != nullptr) {
        if (answered->cancelled()) {
            return false;
        }
        if (const auto* edited = std::get_if<std::string>(&answered->get("color")); edited != nullptr) {
            color = colorOfName(*edited).value_or(color);
        }
    }
    const std::int64_t seed = options_.nextSeed ? options_.nextSeed() : 0;
    for (const PlayerSession* other : owner_->playersInSameWorld()) {
        if (other->entityId() != session.entityId()) {
            player::playSoundFor(*other, "item.dye.use", wire::protocol::play::SoundSource::Players, sheep.x, sheep.y,
                                 sheep.z, 1.0F, 1.0F, seed);
        }
    }
    if (!sheep.sourceNbt.has_value()) {
        sheep.sourceNbt = wire::nbt::NbtCompound{};
    }
    sheep.sourceNbt->putByte("Color", static_cast<std::int8_t>(color));
    ++sheep.sourceNbtVersion;
    if (world::AdvancementTriggers* triggers = owner_->options().advancementTriggers; triggers != nullptr) {
        triggers->fire(
            "player_interacted_with_entity", session.name(), session.uuid().toString(),
            world::TriggerEvent{.entities = {{"entity", world::triggerFacts::ofEntity(*world, sheep)}},
                                .items = {{"item", world::triggerFacts::itemOf(held).value_or(world::TriggerItem{
                                                       .item = wire::common::Identifier::vanilla("air")})}}});
    }
    if (session.gamemode != wire::protocol::play::Gamemode::Creative) {
        storage::StoredItemStack left = *held;
        left.count = held->count - 1;
        session.inventory.set(handSlot, left.count <= 0 ? std::nullopt : std::optional{left});
        owner_->sendInventorySlot(handSlot);
    }
    return true;
}

} // namespace basalt::server::runtime
