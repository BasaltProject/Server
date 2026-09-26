#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/registry/World.hpp>
#include <basalt/server/runtime/ClientSession.hpp>
#include <basalt/server/runtime/UiStateSync.hpp>
#include <basalt/server/runtime/command/CommandDispatcher.hpp>
#include <basalt/server/runtime/command/CommandEnvironment.hpp>
#include <basalt/server/runtime/command/CommandSource.hpp>
#include <basalt/server/runtime/placeholder/PlaceholderSlots.hpp>
#include <basalt/server/runtime/player/PlayerAttributes.hpp>
#include <basalt/server/runtime/player/PlayerEating.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/MobEffects.hpp>
#include <basalt/server/runtime/world/RealEnchantments.hpp>
#include <basalt/wire/protocol/play/CommandPackets.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>
#include <basalt/wire/protocol/play/HudPackets.hpp>
#include <basalt/wire/protocol/play/PlayerStatePackets.hpp>
#include <basalt/wire/protocol/play/SimplePackets.hpp>
#include <basalt/wire/protocol/play/UiPackets.hpp>
#include <basalt/wire/protocol/play/WorldPackets.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <utility>

namespace basalt::server::runtime {
namespace {

using player::PlayerSession;
using wire::common::Identifier;

/// parity: `AbsorptionMobEffect`, four hearts a level.
constexpr float kAbsorptionPerLevel = 4.0F;

} // namespace

UiStateSync::UiStateSync(ClientSession& owner, Hooks hooks) : owner_(&owner), hooks_(std::move(hooks)) {}

void UiStateSync::syncHealth() {
    PlayerSession* session = owner_->playerSession();
    if (session == nullptr) {
        return;
    }
    owner_->sendCompressed(wire::protocol::play::SetHealthPacket::kId, [session](wire::io::ByteBuffer& buffer) {
        wire::protocol::play::SetHealthPacket{.health = session->health(),
                                              .food = session->food.foodLevel(),
                                              .saturation = session->food.saturationLevel()}
            .write(buffer);
    });
}

void UiStateSync::addEffect(const Identifier& effect, std::int32_t durationTicks, std::int32_t amplifier) {
    PlayerSession* session = owner_->playerSession();
    if (session == nullptr) {
        return;
    }
    if (hooks_.effectAllowed && !hooks_.effectAllowed(*session, effect, amplifier, durationTicks)) {
        return;
    }
    const world::ActiveEffect active{.effect = effect, .amplifier = amplifier, .remainingTicks = durationTicks};
    session->effects.insert_or_assign(effect, active);
    sendEffect(active);
}

void UiStateSync::sendEffect(const world::ActiveEffect& effect) {
    PlayerSession* session = owner_->playerSession();
    if (session == nullptr) {
        return;
    }
    const std::optional<std::int32_t> id = world::mobEffects::protocolIdOf(effect.effect);
    if (!id.has_value()) {
        return;
    }
    // parity: `AbsorptionMobEffect.onEffectStarted` adds its hearts on top of whatever is left.
    if (effect.effect == world::mobEffects::absorption()) {
        session->absorption += kAbsorptionPerLevel * static_cast<float>(effect.amplifier + 1);
    }
    owner_->sendPlayPacket(wire::protocol::play::UpdateMobEffectPacket::kId, [&](wire::io::ByteBuffer& buffer) {
        wire::protocol::play::UpdateMobEffectPacket{.entityId = session->entityId(),
                                                    .effect = *id,
                                                    .amplifier = effect.amplifier,
                                                    .durationTicks = effect.remainingTicks,
                                                    .ambient = effect.ambient,
                                                    .visible = effect.visible,
                                                    .showIcon = effect.showIcon}
            .write(buffer);
    });
}

void UiStateSync::sendEffectRemoved(const Identifier& effect) {
    PlayerSession* session = owner_->playerSession();
    if (session == nullptr) {
        return;
    }
    const std::optional<std::int32_t> id = world::mobEffects::protocolIdOf(effect);
    if (!id.has_value()) {
        return;
    }
    // parity: `AbsorptionMobEffect.onEffectRemoved` takes back exactly what it gave, and here
    // nothing else gives any, so the pool empties.
    if (effect == world::mobEffects::absorption()) {
        session->absorption = 0.0F;
    }
    owner_->sendPlayPacket(wire::protocol::play::RemoveMobEffectPacket::kId, [&](wire::io::ByteBuffer& buffer) {
        wire::protocol::play::RemoveMobEffectPacket{.entityId = session->entityId(), .effect = *id}.write(buffer);
    });
}

void UiStateSync::syncHeldAttributes() {
    PlayerSession* session = owner_->playerSession();
    if (session == nullptr) {
        return;
    }
    using Snapshot = wire::protocol::play::UpdateAttributesPacket::AttributeSnapshot;
    world::RealEnchantments* enchantments = owner_->options().enchantments;
    const player::playerAttributes::Modifiers modifiers =
        player::playerAttributes::modifiersOf(*session, [enchantments](const Identifier& id) {
            return enchantments == nullptr ? nullptr : enchantments->definitionOf(id);
        });
    std::map<std::int32_t, Snapshot> now;
    const auto record = [&now, session](std::string_view name, std::vector<player::playerAttributes::Modifier> list) {
        const std::optional<std::int32_t> id = server::registry::attributeIds::protocolId(name);
        if (id.has_value()) {
            now.insert_or_assign(*id, Snapshot{.attribute = *id,
                                               .base = player::playerAttributes::baseOf(*session, name),
                                               .modifiers = std::move(list)});
        }
    };
    for (const auto& [name, base] : session->attributeBases) {
        record(name, {});
    }
    for (const auto& [name, list] : modifiers) {
        record(name, list);
    }
    std::vector<Snapshot> changed;
    for (const auto& [id, snapshot] : now) {
        const auto sent = sentAttributes_.find(id);
        if (sent == sentAttributes_.end() || !(sent->second == snapshot)) {
            changed.push_back(snapshot);
        }
    }
    // One that lost every modifier goes back to its bare base, which the client has to be told too.
    for (const auto& [id, snapshot] : sentAttributes_) {
        if (!now.contains(id)) {
            changed.push_back(Snapshot{.attribute = id, .base = snapshot.base, .modifiers = {}});
            now.emplace(id, changed.back());
        }
    }
    sentAttributes_ = std::move(now);
    if (changed.empty()) {
        return;
    }
    owner_->sendPlayPacket(wire::protocol::play::UpdateAttributesPacket::kId, [&](wire::io::ByteBuffer& buffer) {
        wire::protocol::play::UpdateAttributesPacket{.entityId = session->entityId(), .attributes = changed}.write(
            buffer);
    });
}

void UiStateSync::tickUi() {
    PlayerSession* session = owner_->playerSession();
    if (session == nullptr) {
        return;
    }
    const ClientSession::PluginHooks& plugins = owner_->options().plugins;
    // A plugin holding a surface is reason enough to draw, even with all the files switched off.
    const bool anyEnabled = owner_->options().ui && owner_->options().ui().anyEnabled();
    const bool pluginDraws = (plugins.surfaces.holdsSidebar && plugins.surfaces.holdsSidebar()) ||
                             (plugins.surfaces.holdsBossBar && plugins.surfaces.holdsBossBar());
    if (!anyEnabled && !pluginDraws) {
        return;
    }
    const wire::common::placeholder::PlaceholderContext context =
        wire::common::placeholder::PlaceholderContext{}.with(placeholder::playerSlot(), session);
    owner_->playerUi().tick(owner_->uiTick++, context);
}

void UiStateSync::sendTime(const world::LoadedWorld& world) {
    const wire::protocol::play::SetTimePacket packet = command::timePacketOf(world);
    owner_->sendPlayPacket(wire::protocol::play::SetTimePacket::kId,
                           [packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
}

void UiStateSync::sendWeather(const world::WorldWeather& weather) {
    using wire::protocol::play::GameEventPacket;
    // parity: `PlayerList.sendLevelInfo`, which announces rain as a start or stop event *and* the
    // two levels: the events swap the client's weather renderer, the levels set how heavy it is,
    // and a client sent only one of the two shows the wrong thing.
    owner_->sendPlayPacket(GameEventPacket::kId, [&](wire::io::ByteBuffer& buffer) {
        GameEventPacket{.event = weather.raining ? GameEventPacket::kStartRaining : GameEventPacket::kStopRaining,
                        .param = 0.0F}
            .write(buffer);
    });
    owner_->sendPlayPacket(GameEventPacket::kId, [&](wire::io::ByteBuffer& buffer) {
        GameEventPacket{.event = GameEventPacket::kRainLevelChange, .param = weather.rainLevel()}.write(buffer);
    });
    owner_->sendPlayPacket(GameEventPacket::kId, [&](wire::io::ByteBuffer& buffer) {
        GameEventPacket{.event = GameEventPacket::kThunderLevelChange, .param = weather.thunderLevel()}.write(buffer);
    });
}

void UiStateSync::syncExperience() {
    PlayerSession* session = owner_->playerSession();
    if (session == nullptr) {
        return;
    }
    if (session->experience.level() != owner_->lastSyncedLevel) {
        const std::int32_t was = owner_->lastSyncedLevel;
        owner_->lastSyncedLevel = session->experience.level();
        if (hooks_.levelChanged) {
            hooks_.levelChanged(*session, session->experience.level(), was);
        }
    }
    owner_->sendCompressed(wire::protocol::play::SetExperiencePacket::kId, [session](wire::io::ByteBuffer& buffer) {
        wire::protocol::play::SetExperiencePacket{.progress = session->experience.progress(),
                                                  .level = session->experience.level(),
                                                  .total = session->experience.total()}
            .write(buffer);
    });
}

void UiStateSync::sendAbilities() {
    PlayerSession* session = owner_->playerSession();
    if (session == nullptr) {
        return;
    }
    owner_->sendCompressed(
        wire::protocol::play::ClientboundPlayerAbilitiesPacket::kId, [session](wire::io::ByteBuffer& buffer) {
            wire::protocol::play::ClientboundPlayerAbilitiesPacket{.invulnerable = session->invulnerable(),
                                                                   .flying = session->flying,
                                                                   .mayFly = session->mayFly(),
                                                                   .instabuild = session->instabuild()}
                .write(buffer);
        });
}

void UiStateSync::sendCommandTree() {
    PlayerSession* session = owner_->playerSession();
    command::CommandDispatcher* dispatcher = owner_->options().commands;
    if (session == nullptr || dispatcher == nullptr) {
        return;
    }
    sendPermissionLevel();
    const wire::protocol::play::CommandsPacket tree =
        dispatcher->treeFor(command::PlayerCommandSource{*session, dispatcher->environment().groups()});
    owner_->sendCompressed(wire::protocol::play::CommandsPacket::kId,
                           [&tree](wire::io::ByteBuffer& buffer) { tree.write(buffer); });
}

void UiStateSync::sendPermissionLevel() {
    PlayerSession* session = owner_->playerSession();
    command::CommandDispatcher* dispatcher = owner_->options().commands;
    if (session == nullptr || dispatcher == nullptr) {
        return;
    }
    // The client parses commands locally against this: below gamemaster it refuses selectors, so
    // without it `/tellraw @a ...` is underlined in red even though the server runs it fine.
    const std::int32_t level =
        command::PlayerCommandSource{*session, dispatcher->environment().groups()}.permissionLevel();
    const wire::protocol::play::EntityEventPacket packet =
        wire::protocol::play::EntityEventPacket::permissionLevel(session->entityId(), level);
    owner_->sendPlayPacket(wire::protocol::play::EntityEventPacket::kId,
                           [packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
}

void UiStateSync::sendStats() {
    PlayerSession* session = owner_->playerSession();
    if (session == nullptr) {
        return;
    }
    using wire::protocol::play::AwardStatsPacket;
    using wire::protocol::play::StatCounter;
    std::vector<StatCounter> stats;
    // parity: `ClientboundAwardStatsPacket`, each counter as its type and the id of what it counts.
    const auto itemId = [](std::string_view name) -> std::optional<std::int32_t> {
        const std::expected<Identifier, wire::common::IdentifierError> item = Identifier::parse(name);
        const server::registry::Item* entry =
            item.has_value() ? server::registry::items::registry().byName(*item) : nullptr;
        return entry == nullptr ? std::nullopt : std::optional{entry->protocolId};
    };
    const auto entityId = [](std::string_view name) -> std::optional<std::int32_t> {
        const std::expected<Identifier, wire::common::IdentifierError> type = Identifier::parse(name);
        const server::registry::EntityTypeEntry* entry =
            type.has_value() ? server::registry::entityTypes::of(*type) : nullptr;
        return entry == nullptr ? std::nullopt : std::optional{entry->protocolId};
    };
    const auto blockId = [](std::string_view name) -> std::optional<std::int32_t> {
        const std::expected<Identifier, wire::common::IdentifierError> block = Identifier::parse(name);
        return block.has_value() ? server::registry::blocks::registry().idOf(*block) : std::nullopt;
    };
    const auto customId = [](std::string_view name) -> std::optional<std::int32_t> {
        const std::expected<Identifier, wire::common::IdentifierError> custom = Identifier::parse(name);
        return custom.has_value() && custom->namespaceName() == "minecraft"
                   ? server::registry::customStats::idOf(custom->path())
                   : std::nullopt;
    };

    struct Kind {
        std::string_view prefix;
        std::int32_t type;
        std::optional<std::int32_t> (*id)(std::string_view);
    };

    const std::array<Kind, 9> kinds{Kind{storage::stats::kMined, AwardStatsPacket::kTypeMined, +blockId},
                                    Kind{storage::stats::kCrafted, AwardStatsPacket::kTypeCrafted, +itemId},
                                    Kind{storage::stats::kUsed, AwardStatsPacket::kTypeUsed, +itemId},
                                    Kind{storage::stats::kBroken, AwardStatsPacket::kTypeBroken, +itemId},
                                    Kind{storage::stats::kPickedUp, AwardStatsPacket::kTypePickedUp, +itemId},
                                    Kind{storage::stats::kDropped, AwardStatsPacket::kTypeDropped, +itemId},
                                    Kind{storage::stats::kKilledBy, AwardStatsPacket::kTypeKilledBy, +entityId},
                                    Kind{storage::stats::kKilled, AwardStatsPacket::kTypeKilled, +entityId},
                                    Kind{storage::stats::kCustom, AwardStatsPacket::kTypeCustom, +customId}};
    for (const auto& [key, value] : session->stats.snapshot(owner_->playTimeTicks).counters) {
        const auto count =
            static_cast<std::int32_t>(std::min<std::int64_t>(value, std::numeric_limits<std::int32_t>::max()));
        for (const Kind& kind : kinds) {
            if (!key.starts_with(kind.prefix) || key.size() <= kind.prefix.size() || key[kind.prefix.size()] != '/') {
                continue;
            }
            if (const std::optional<std::int32_t> id = kind.id(std::string_view{key}.substr(kind.prefix.size() + 1));
                id.has_value()) {
                stats.push_back(StatCounter{.statType = kind.type, .value = *id, .count = count});
            }
            break;
        }
    }
    owner_->sendCompressed(AwardStatsPacket::kId,
                           [&stats](wire::io::ByteBuffer& buffer) { AwardStatsPacket{.stats = stats}.write(buffer); });
}

void UiStateSync::applyConsumeEffects(PlayerSession& session,
                                      const std::vector<server::registry::ConsumeEffect>& effects,
                                      const std::function<float()>& nextFloat) {
    player::applyConsumeEffectsFor(session, effects, nextFloat);
}

} // namespace basalt::server::runtime
