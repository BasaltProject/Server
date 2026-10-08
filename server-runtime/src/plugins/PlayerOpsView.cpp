#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/player/PlayerTabList.hpp>
#include <basalt/server/runtime/plugins/PlayerOps.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/Signs.hpp>
#include <basalt/wire/protocol/play/BlockEntityPackets.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>
#include <basalt/wire/text/ComponentNbt.hpp>
#include <basalt/wire/text/MiniMessage.hpp>

#include <algorithm>
#include <cmath>

namespace basalt::server::runtime::plugins::playerOps {
namespace {

namespace play = wire::protocol::play;

[[nodiscard]] player::PlayerSession* playerWithEntity(Context& context, const std::int32_t entityId) {
    for (player::PlayerSession* candidate : context.registry.snapshot()) {
        if (candidate->entityId() == entityId) {
            return candidate;
        }
    }
    return nullptr;
}

template <typename Ids>
void forgetGone(Context& context, Ids& ids) {
    constexpr std::size_t kPruneAbove = 256;
    if (ids.size() <= kPruneAbove) {
        return;
    }
    const world::LoadedWorld* world = worldOf(context);
    std::erase_if(ids, [&](std::int32_t id) {
        return playerWithEntity(context, id) == nullptr && (world == nullptr || world->state().entity(id) == nullptr);
    });
}

[[nodiscard]] std::vector<std::string> linesOf(const std::string& text) {
    std::vector<std::string> lines;
    std::size_t at = 0;
    while (lines.size() < static_cast<std::size_t>(world::signs::kLines)) {
        const std::size_t stop = text.find('\n', at);
        lines.push_back(text.substr(at, stop == std::string::npos ? std::string::npos : stop - at));
        if (stop == std::string::npos) {
            break;
        }
        at = stop + 1;
    }
    lines.resize(world::signs::kLines);
    return lines;
}

[[nodiscard]] bool canSee(Context& context, const std::int32_t entityId) {
    const player::PlayerExtras& extras = context.session.extras;
    if (extras.hiddenEntities.contains(entityId)) {
        return false;
    }
    if (const player::PlayerSession* other = playerWithEntity(context, entityId); other != nullptr) {
        return !other->extras.hiddenByDefault || extras.shownEntities.contains(entityId);
    }
    world::LoadedWorld* world = worldOf(context);
    const entity::ServerEntity* target = world == nullptr ? nullptr : world->state().entity(entityId);
    if (target == nullptr || !target->visibleTo.has_value()) {
        return true;
    }
    return std::ranges::find(*target->visibleTo, context.session.uuid().toString()) != target->visibleTo->end();
}

[[nodiscard]] wire::nbt::NbtTag lineTag(const std::string& mini) {
    wire::nbt::NbtTag tag = wire::text::componentNbt::serialize(wire::text::miniMessage::parse(mini));
    if (const std::string* plain = tag.get<std::string>(); plain != nullptr) {
        wire::nbt::NbtCompound wrapped;
        wrapped.putString("text", *plain);
        return wire::nbt::NbtTag::ofCompound(std::move(wrapped));
    }
    return tag;
}

void writeSide(wire::nbt::NbtCompound& data, const bool front, const std::string& text, const std::string& color,
               const bool glowing) {
    wire::nbt::NbtList messages{wire::nbt::NbtType::Compound};
    for (const std::string& line : linesOf(text)) {
        messages.push(lineTag(line));
    }
    wire::nbt::NbtCompound side;
    side.set("messages", wire::nbt::NbtTag::ofList(std::move(messages)));
    side.putString("color", color);
    side.putBoolean("has_glowing_text", glowing);
    data.set(front ? "front_text" : "back_text", wire::nbt::NbtTag::ofCompound(std::move(side)));
}

} // namespace

std::optional<Value> view(Context& context, const Verb verb, const std::string_view name, const Args& args) {
    player::PlayerSession& session = context.session;
    player::PlayerExtras& extras = session.extras;
    const player::PlayerConnection& connection = session.connection();
    const Value& first = arg(args, 0);
    if (name == "player_time") {
        return extras.timeOverride.has_value() ? Value{*extras.timeOverride} : Value{};
    }
    if (name == "player_time_relative") {
        return extras.timeRelative;
    }
    if (name == "set_player_time" || name == "reset_player_time") {
        if (name == "reset_player_time" || isNone(first)) {
            extras.timeOverride.reset();
        } else {
            extras.timeOverride = asInt(first);
            extras.timeRelative = asBool(arg(args, 1));
        }
        if (world::LoadedWorld* world = worldOf(context); world != nullptr && connection.sendTime) {
            connection.sendTime(*world);
        }
        return true;
    }
    if (name == "player_weather") {
        if (!extras.rainOverride.has_value()) {
            return Value{};
        }
        return std::string{*extras.rainOverride > 0.0F ? "downfall" : "clear"};
    }
    if (name == "set_player_weather") {
        if (isNone(first)) {
            extras.rainOverride.reset();
            extras.thunderOverride = 0.0F;
        } else {
            extras.rainOverride = asText(first) == "downfall" ? 1.0F : 0.0F;
            extras.thunderOverride = 0.0F;
        }
        if (world::LoadedWorld* world = worldOf(context); world != nullptr && connection.sendWeather) {
            connection.sendWeather(world->weather());
        }
        return true;
    }
    if (name == "hide_entity") {
        const auto id = static_cast<std::int32_t>(asInt(first, -1));
        if (id == session.entityId()) {
            return false;
        }
        forgetGone(context, extras.hiddenEntities);
        forgetGone(context, extras.shownEntities);
        extras.hiddenEntities.insert(id);
        extras.shownEntities.erase(id);
        return true;
    }
    if (name == "show_entity") {
        const auto id = static_cast<std::int32_t>(asInt(first, -1));
        forgetGone(context, extras.hiddenEntities);
        forgetGone(context, extras.shownEntities);
        extras.hiddenEntities.erase(id);
        extras.shownEntities.insert(id);
        world::LoadedWorld* world = worldOf(context);
        entity::ServerEntity* target = world == nullptr ? nullptr : world->state().entity(id);
        if (target != nullptr && target->visibleTo.has_value() &&
            std::ranges::find(*target->visibleTo, session.uuid().toString()) == target->visibleTo->end()) {
            target->visibleTo->push_back(session.uuid().toString());
        }
        return true;
    }
    if (name == "can_see") {
        return canSee(context, static_cast<std::int32_t>(asInt(first, -1)));
    }
    if (name == "visible_by_default") {
        const auto id = static_cast<std::int32_t>(asInt(first, -1));
        player::PlayerSession* other = playerWithEntity(context, id);
        world::LoadedWorld* world = worldOf(context);
        entity::ServerEntity* target = world == nullptr ? nullptr : world->state().entity(id);
        if (verb == Verb::Get || args.size() < 2) {
            if (other != nullptr) {
                return !other->extras.hiddenByDefault;
            }
            return target == nullptr || !target->visibleTo.has_value();
        }
        const bool visible = asBool(arg(args, 1), true);
        if (other != nullptr) {
            other->extras.hiddenByDefault = !visible;
            return true;
        }
        if (target == nullptr) {
            return false;
        }
        if (visible) {
            target->visibleTo.reset();
        } else {
            target->visibleTo = std::vector<std::string>{};
        }
        return true;
    }
    if (name == "tracked_by") {
        const auto id = static_cast<std::int32_t>(asInt(first, session.entityId()));
        std::vector<std::string> viewers;
        for (const player::PlayerSession* viewer : context.registry.snapshot()) {
            if (viewer->entityId() != id && viewer->connection().tracksEntity &&
                viewer->connection().tracksEntity(id)) {
                viewers.push_back(viewer->uuid().toString());
            }
        }
        return viewers;
    }
    if (name == "sent_chunks") {
        std::vector<std::string> keys;
        if (session.connection().sentChunks) {
            for (const auto& [x, z] : session.connection().sentChunks()) {
                keys.push_back(std::to_string(x));
                keys.push_back(std::to_string(z));
            }
        }
        return keys;
    }
    if (name == "pickup_animation") {
        const play::TakeItemEntityPacket packet{.collectedEntityId = static_cast<std::int32_t>(asInt(first)),
                                                .collectorEntityId = session.entityId(),
                                                .pickupItemCount = static_cast<std::int32_t>(asInt(arg(args, 1), 1))};
        for (const player::PlayerSession* viewer : context.registry.snapshot()) {
            if (viewer->worldName == session.worldName && viewer->connection().sendPlayPacket) {
                viewer->connection().sendPlayPacket(play::TakeItemEntityPacket::kId,
                                                    [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
            }
        }
        return true;
    }
    if (name == "list_unlisted") {
        return extras.unlistedPlayers.contains(static_cast<std::int32_t>(asInt(first, -1)));
    }
    if (name == "list_player") {
        const auto id = static_cast<std::int32_t>(asInt(first, -1));
        const player::PlayerSession* other = playerWithEntity(context, id);
        if (other == nullptr) {
            return false;
        }
        if (asBool(arg(args, 1), true)) {
            extras.unlistedPlayers.erase(id);
        } else {
            forgetGone(context, extras.unlistedPlayers);
            extras.unlistedPlayers.insert(id);
        }
        player::sendListEntry(session, *other, !extras.unlistedPlayers.contains(id));
        return true;
    }
    if (name == "spectator_target") {
        if (verb == Verb::Get) {
            return static_cast<std::int64_t>(extras.spectatorTarget.value_or(-1));
        }
        const auto id = static_cast<std::int32_t>(asInt(first, -1));
        if (!connection.setCamera) {
            return false;
        }
        extras.spectatorTarget = id < 0 ? std::nullopt : std::optional{id};
        connection.setCamera(id < 0 ? session.entityId() : id);
        return true;
    }
    if (name == "compass_target") {
        if (!extras.compassTarget.has_value()) {
            return Value{};
        }
        return std::vector<std::string>{std::to_string((*extras.compassTarget)[0]),
                                        std::to_string((*extras.compassTarget)[1]),
                                        std::to_string((*extras.compassTarget)[2])};
    }
    if (name == "set_compass_target") {
        world::LoadedWorld* world = worldOf(context);
        if (world == nullptr || !connection.sendSpawnPosition) {
            return false;
        }
        const std::array<std::int32_t, 3> at{static_cast<std::int32_t>(asInt(first)),
                                             static_cast<std::int32_t>(asInt(arg(args, 1))),
                                             static_cast<std::int32_t>(asInt(arg(args, 2)))};
        extras.compassTarget = at;
        connection.sendSpawnPosition(*world, at[0], at[1], at[2], 0.0F);
        return true;
    }
    if (name == "block_change") {
        if (!connection.sendBlockChange) {
            return false;
        }
        connection.sendBlockChange(
            static_cast<std::int32_t>(asInt(first)), static_cast<std::int32_t>(asInt(arg(args, 1))),
            static_cast<std::int32_t>(asInt(arg(args, 2))), static_cast<std::int32_t>(asInt(arg(args, 3))));
        return true;
    }
    if (name == "block_damage") {
        if (!connection.sendBlockDestruction) {
            return false;
        }
        connection.sendBlockDestruction(
            static_cast<std::int32_t>(asInt(arg(args, 4), session.entityId())), static_cast<std::int32_t>(asInt(first)),
            static_cast<std::int32_t>(asInt(arg(args, 1))), static_cast<std::int32_t>(asInt(arg(args, 2))),
            static_cast<std::int32_t>(asInt(arg(args, 3))), 0.0F);
        return true;
    }
    if (name == "sign_text") {
        const auto x = static_cast<std::int32_t>(asInt(first));
        const auto y = static_cast<std::int32_t>(asInt(arg(args, 1)));
        const auto z = static_cast<std::int32_t>(asInt(arg(args, 2)));
        world::LoadedWorld* world = worldOf(context);
        const std::optional<std::int32_t> stateId =
            world == nullptr ? std::nullopt : world->state().blockState(x, y, z);
        std::optional<wire::common::Identifier> type =
            stateId.has_value() ? world::signs::typeOf(*stateId) : std::nullopt;
        if (!type.has_value()) {
            const auto parsed = wire::common::Identifier::parse(asText(arg(args, 7), "minecraft:sign"));
            type = parsed.has_value() ? std::optional{*parsed} : std::nullopt;
        }
        const std::optional<std::int32_t> typeId =
            type.has_value() ? server::registry::blockEntityTypes::idOf(*type) : std::nullopt;
        if (!typeId.has_value() || !connection.sendPlayPacket) {
            return false;
        }
        const std::string color = asText(arg(args, 5), "black");
        const bool glowing = asBool(arg(args, 6));
        wire::nbt::NbtCompound data = world::signs::emptyData();
        writeSide(data, true, asText(arg(args, 3)), color, glowing);
        writeSide(data, false, asText(arg(args, 4)), color, glowing);
        const play::BlockEntityDataPacket packet{.pos = {.x = x, .y = y, .z = z}, .typeId = *typeId, .tag = data};
        connection.sendPlayPacket(play::BlockEntityDataPacket::kId,
                                  [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
        return true;
    }
    if (name == "open_sign") {
        if (!connection.sendPlayPacket) {
            return false;
        }
        const play::OpenSignEditorPacket packet{.pos = {.x = static_cast<std::int32_t>(asInt(first)),
                                                        .y = static_cast<std::int32_t>(asInt(arg(args, 1))),
                                                        .z = static_cast<std::int32_t>(asInt(arg(args, 2)))},
                                                .isFrontText = asBool(arg(args, 3), true)};
        connection.sendPlayPacket(play::OpenSignEditorPacket::kId,
                                  [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
        return true;
    }
    return std::nullopt;
}

} // namespace basalt::server::runtime::plugins::playerOps
