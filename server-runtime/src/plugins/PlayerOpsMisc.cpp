#include <basalt/server/registry/Items.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerCooldowns.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/plugins/PlayerOps.hpp>
#include <basalt/server/runtime/plugins/PluginItemConversion.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/MobEffects.hpp>
#include <basalt/server/runtime/world/PlayerProgress.hpp>
#include <basalt/server/runtime/world/RealRecipes.hpp>
#include <basalt/server/runtime/world/RecipeBook.hpp>
#include <basalt/server/storage/AccessRepository.hpp>
#include <basalt/server/storage/PlayerStore.hpp>
#include <basalt/wire/nbt/NbtIO.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/configuration/ConfigurationPackets.hpp>
#include <basalt/wire/protocol/play/BorderPackets.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>
#include <basalt/wire/protocol/play/InventoryPackets.hpp>
#include <basalt/wire/protocol/play/WorldPackets.hpp>
#include <basalt/wire/text/ComponentNbt.hpp>
#include <basalt/wire/text/MiniMessage.hpp>

#include <algorithm>

namespace basalt::server::runtime::plugins::playerOps {
namespace {

namespace play = wire::protocol::play;
namespace configuration = wire::protocol::configuration;

constexpr std::int32_t kResourcePackPopId = 0x51;
constexpr std::int32_t kLookAtId = 0x48;
constexpr std::int32_t kOpenBookId = 0x3b;
constexpr std::int32_t kStoreCookieId = 0x7a;
constexpr std::int32_t kServerLinksId = 0x8c;
constexpr std::int32_t kCookieRequestId = 0x15;
constexpr std::size_t kMaxCookieBytes = 5120;
constexpr std::int32_t kCustomCompletionsId = 0x17;

[[nodiscard]] wire::common::Identifier itemId(const std::string& text) {
    const auto parsed =
        wire::common::Identifier::parse(text.find(':') == std::string::npos ? "minecraft:" + text : text);
    return parsed.has_value() ? *parsed : wire::common::Identifier::vanilla("air");
}

[[nodiscard]] std::int64_t gameTimeOf(Context& context) {
    const world::LoadedWorld* world = worldOf(context);
    return world == nullptr ? 0 : world->gameTime();
}

[[nodiscard]] std::optional<storage::StoredItemStack> decodeItem(const std::string& bytes) {
    const std::expected<wire::nbt::AnonymousTag, wire::nbt::NbtError> read = wire::nbt::nbtIO::readAnonymous(
        std::span<const std::uint8_t>{reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size()});
    const wire::nbt::NbtCompound* root = read.has_value() ? read->tag.get<wire::nbt::NbtCompound>() : nullptr;
    if (root == nullptr || !root->contains("id")) {
        return std::nullopt;
    }
    storage::StoredItemStack stack{.id = std::string{root->getString("id")}, .count = root->getInt("count", 1)};
    if (const wire::nbt::NbtCompound* components = root->getCompound("components"); components != nullptr) {
        stack.components = *components;
    }
    return stack;
}

[[nodiscard]] std::string encodeItem(const storage::StoredItemStack& stack) {
    wire::nbt::NbtCompound root;
    root.putString("id", stack.id);
    root.putInt("count", stack.count);
    root.set("components", wire::nbt::NbtTag::ofCompound(stack.components));
    const std::vector<std::uint8_t> bytes =
        wire::nbt::nbtIO::writeAnonymous(wire::nbt::NbtTag::ofCompound(std::move(root)));
    return std::string{bytes.begin(), bytes.end()};
}

} // namespace

std::optional<Value> misc(Context& context, const Verb verb, const std::string_view name, const Args& args) {
    player::PlayerSession& session = context.session;
    player::PlayerExtras& extras = session.extras;
    const player::PlayerConnection& connection = session.connection();
    const Value& first = arg(args, 0);
    if (name == "ping") {
        return static_cast<std::int64_t>(extras.pingMillis);
    }
    if (name == "locale") {
        return extras.client.locale;
    }
    if (name == "client_view_distance") {
        return static_cast<std::int64_t>(extras.client.viewDistance);
    }
    if (name == "client_particles") {
        return static_cast<std::int64_t>(extras.client.particleStatus);
    }
    if (name == "chat_colors") {
        return extras.client.chatColors;
    }
    if (name == "text_filtering") {
        return extras.client.textFiltering;
    }
    if (name == "server_listings") {
        return extras.client.serverListings;
    }
    if (name == "main_hand") {
        return static_cast<std::int64_t>(session.mainArm);
    }
    if (name == "skin_parts") {
        return static_cast<std::int64_t>(static_cast<std::uint8_t>(session.skinParts));
    }
    if (name == "chat_visibility") {
        return static_cast<std::int64_t>(session.chatVisibility.load());
    }
    if (name == "client_brand") {
        return extras.brand;
    }
    if (name == "protocol_version") {
        return static_cast<std::int64_t>(session.protocolVersion());
    }
    if (name == "virtual_host") {
        return session.connectedHost() + ":" + std::to_string(session.connection().connectedPort);
    }
    if (name == "transferred") {
        return session.connection().transferred;
    }
    if (name == "bee_stingers") {
        if (verb == Verb::Get) {
            return static_cast<std::int64_t>(session.stingerCount);
        }
        session.stingerCount = static_cast<std::int32_t>(asInt(first));
        return true;
    }
    if (name == "bee_stinger_cooldown") {
        if (verb == Verb::Get) {
            return static_cast<std::int64_t>(session.removeStingerTime);
        }
        session.removeStingerTime = static_cast<std::int32_t>(asInt(first));
        return true;
    }
    if (name == "arrow_cooldown") {
        if (verb == Verb::Get) {
            return static_cast<std::int64_t>(session.removeArrowTime);
        }
        session.removeArrowTime = static_cast<std::int32_t>(asInt(first));
        return true;
    }
    if (name == "riptiding") {
        return session.autoSpinTicks > 0;
    }
    if (name == "cooldown") {
        const std::int64_t now = gameTimeOf(context);
        const auto found = session.itemCooldowns.find(itemId(asText(first)).path());
        return static_cast<std::int64_t>(
            found == session.itemCooldowns.end() ? 0 : std::max<std::int64_t>(0, found->second - now));
    }
    if (name == "set_cooldown") {
        const wire::common::Identifier item = itemId(asText(first));
        const auto ticks = static_cast<std::int32_t>(asInt(arg(args, 1)));
        if (ticks <= 0) {
            session.itemCooldowns.erase(item.path());
            if (connection.startItemCooldown) {
                connection.startItemCooldown(item, 0);
            }
            return true;
        }
        player::startCooldownFor(session, item, gameTimeOf(context), ticks);
        return true;
    }
    if (name == "statistic") {
        return session.stats.get(asText(first));
    }
    if (name == "set_statistic") {
        const std::string key = asText(first);
        session.stats.reset(key);
        session.stats.increment(key, asInt(arg(args, 1)));
        return true;
    }
    if (name == "inc_statistic") {
        session.stats.increment(asText(first), asInt(arg(args, 1), 1));
        return true;
    }
    if (name == "transfer") {
        if (!connection.transferTo) {
            return false;
        }
        connection.transferTo(asText(first), static_cast<std::int32_t>(asInt(arg(args, 1), 25565)));
        return true;
    }
    if (name == "resource_pack") {
        if (!connection.sendPlayPacket) {
            return false;
        }
        const auto id = wire::common::Uuid::parse(asText(first));
        if (!id.has_value()) {
            return false;
        }
        const std::string prompt = asText(arg(args, 4));
        const configuration::ResourcePackPushPacket packet{
            .id = *id,
            .url = asText(arg(args, 1)),
            .hash = asText(arg(args, 2)),
            .required = asBool(arg(args, 3)),
            .prompt = prompt.empty() ? std::nullopt : std::optional{wire::text::miniMessage::parse(prompt)}};
        connection.sendPlayPacket(configuration::ResourcePackPushPacket::kPlayId,
                                  [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
        return true;
    }
    if (name == "remove_resource_pack") {
        if (!connection.sendPlayPacket) {
            return false;
        }
        const auto id = isNone(first) ? std::nullopt : wire::common::Uuid::parse(asText(first));
        connection.sendPlayPacket(kResourcePackPopId, [&id](wire::io::ByteBuffer& buffer) {
            wire::protocol::writeBoolean(buffer, id.has_value());
            if (id.has_value()) {
                wire::protocol::writeUuid(buffer, *id);
            }
        });
        return true;
    }
    if (name == "pack_status") {
        return static_cast<std::int64_t>(extras.packStatus);
    }
    if (name == "game_event") {
        if (!connection.sendPlayPacket) {
            return false;
        }
        const play::GameEventPacket packet{.event = static_cast<std::uint8_t>(asInt(first)),
                                           .param = static_cast<float>(asReal(arg(args, 1)))};
        connection.sendPlayPacket(play::GameEventPacket::kId,
                                  [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
        return true;
    }
    if (name == "border_set" || name == "border_reset") {
        if (!connection.sendPlayPacket) {
            return false;
        }
        play::InitializeBorderPacket packet;
        if (name == "border_reset") {
            const world::LoadedWorld* world = worldOf(context);
            if (world == nullptr) {
                return false;
            }
            packet = const_cast<world::LoadedWorld*>(world)->border().initializePacket();
        } else {
            packet.centerX = asReal(first);
            packet.centerZ = asReal(arg(args, 1));
            packet.oldSize = packet.newSize = asReal(arg(args, 2), 5.9999968E7);
            packet.warningBlocks = static_cast<std::int32_t>(asInt(arg(args, 3), 5));
            packet.warningSeconds = static_cast<std::int32_t>(asInt(arg(args, 4), 15));
        }
        connection.sendPlayPacket(play::InitializeBorderPacket::kId,
                                  [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
        return true;
    }
    if (name == "known_recipes" || name == "discover_recipes" || name == "undiscover_recipes" || name == "has_recipe") {
        world::PlayerProgress* progress = context.hooks.progress ? context.hooks.progress() : nullptr;
        world::RealRecipes* recipes = context.hooks.recipes ? context.hooks.recipes() : nullptr;
        if (progress == nullptr || recipes == nullptr) {
            return name == "known_recipes" ? Value{std::vector<std::string>{}} : Value{};
        }
        const std::string uuid = session.uuid().toString();
        if (name == "known_recipes") {
            std::vector<std::string> known;
            for (const wire::common::Identifier& id : progress->knownRecipes(uuid)) {
                known.push_back(id.toString());
            }
            return known;
        }
        if (name == "has_recipe") {
            const auto id = wire::common::Identifier::parse(asText(first));
            return id.has_value() && progress->knownRecipes(uuid).contains(*id);
        }
        const auto* requested = std::get_if<std::vector<std::string>>(&first);
        std::vector<wire::common::Identifier> changed;
        for (const std::string& text : requested == nullptr ? std::vector<std::string>{} : *requested) {
            const auto id = wire::common::Identifier::parse(text);
            if (!id.has_value()) {
                continue;
            }
            if (name == "discover_recipes") {
                if (recipes->recipeFor(*id) != nullptr && progress->learn(uuid, *id)) {
                    changed.push_back(*id);
                }
            } else if (progress->forget(uuid, *id)) {
                changed.push_back(*id);
            }
        }
        if (!changed.empty()) {
            if (name == "discover_recipes") {
                world::recipeBook::add(session, *recipes, changed);
            } else {
                world::recipeBook::remove(session, *recipes, changed);
            }
        }
        return static_cast<std::int64_t>(changed.size());
    }
    if (name == "spawn_point" || name == "set_spawn_point" || name == "clear_spawn_point") {
        storage::PlayerStore* store = context.hooks.store ? context.hooks.store() : nullptr;
        if (store == nullptr || !store->isOpen()) {
            return name == "spawn_point" ? Value{} : Value{false};
        }
        const std::string uuid = session.uuid().toString();
        if (name == "spawn_point") {
            const std::optional<storage::StoredSpawnPoint> point = store->access().spawnPointOf(uuid);
            if (!point.has_value()) {
                return Value{};
            }
            return std::vector<std::string>{point->world, std::to_string(point->x), std::to_string(point->y),
                                            std::to_string(point->z), std::to_string(point->yaw)};
        }
        if (name == "clear_spawn_point") {
            store->access().clearSpawnPoint(uuid);
            return true;
        }
        store->access().setSpawnPoint(storage::StoredSpawnPoint{.uuid = uuid,
                                                                .world = asText(first),
                                                                .x = asReal(arg(args, 1)),
                                                                .y = asReal(arg(args, 2)),
                                                                .z = asReal(arg(args, 3)),
                                                                .yaw = static_cast<float>(asReal(arg(args, 4)))});
        return true;
    }
    if (name == "last_death") {
        if (!session.lastDeath.has_value()) {
            return Value{};
        }
        return std::vector<std::string>{session.lastDeath->dimensionName, std::to_string(session.lastDeath->location.x),
                                        std::to_string(session.lastDeath->location.y),
                                        std::to_string(session.lastDeath->location.z)};
    }
    if (name == "set_last_death") {
        if (isNone(first)) {
            session.lastDeath.reset();
            return true;
        }
        session.lastDeath = play::GlobalPos{.dimensionName = asText(first),
                                            .location = play::BlockPos{static_cast<std::int32_t>(asInt(arg(args, 1))),
                                                                       static_cast<std::int32_t>(asInt(arg(args, 2))),
                                                                       static_cast<std::int32_t>(asInt(arg(args, 3)))}};
        return true;
    }
    if (name == "post_effects") {
        return std::vector<std::string>{session.postEffects.begin(), session.postEffects.end()};
    }
    if (name == "set_post_effects") {
        const auto* list = std::get_if<std::vector<std::string>>(&first);
        const std::vector<std::string> next = list == nullptr ? std::vector<std::string>{} : *list;
        const bool changed = next != session.postEffects;
        session.postEffects = next;
        if (connection.sendPlayPacket) {
            const configuration::PostEffectsPacket packet{.effects = next};
            connection.sendPlayPacket(configuration::PostEffectsPacket::kPlayId,
                                      [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
        }
        return changed;
    }
    if (name == "ender_slot" || name == "set_ender_slot") {
        const auto slot = asInt(first, -1);
        if (slot < 0 || slot >= static_cast<std::int64_t>(session.enderChest.size())) {
            return name == "ender_slot" ? Value{std::string{}} : Value{false};
        }
        auto& held = session.enderChest[static_cast<std::size_t>(slot)];
        if (name == "ender_slot") {
            return held.has_value() ? encodeItem(*held) : std::string{};
        }
        const std::string snbt = asText(arg(args, 1));
        held = snbt.empty() ? std::nullopt : decodeItem(snbt);
        return true;
    }
    if (name == "save_data") {
        storage::PlayerStore* store = context.hooks.store ? context.hooks.store() : nullptr;
        if (store == nullptr || !store->isOpen() || !connection.storedSnapshot) {
            return false;
        }
        const std::optional<storage::StoredPlayer> snapshot = connection.storedSnapshot();
        if (!snapshot.has_value()) {
            return false;
        }
        store->players().save(*snapshot);
        return true;
    }
    if (name == "respawn") {
        if (!connection.respawn || session.health() > 0.0F) {
            return false;
        }
        connection.respawn();
        return true;
    }
    if (name == "chat") {
        if (!connection.chat) {
            return false;
        }
        connection.chat(asText(first));
        return true;
    }
    if (name == "wake_up") {
        if (!connection.wakeUp) {
            return false;
        }
        connection.wakeUp();
        return true;
    }
    if (name == "swing") {
        const play::SwingAnimationPacket packet{.entityId = session.entityId(), .hand = asBool(first) ? 1 : 0};
        for (const player::PlayerSession* viewer : context.registry.snapshot()) {
            if (viewer->worldName == session.worldName && viewer->connection().sendPlayPacket) {
                viewer->connection().sendPlayPacket(play::SwingAnimationPacket::kId,
                                                    [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
            }
        }
        return true;
    }
    if (name == "hurt_animation") {
        const play::HurtAnimationPacket packet{.entityId = session.entityId(),
                                               .yaw = static_cast<float>(asReal(first))};
        const std::string only = asText(arg(args, 1));
        for (const player::PlayerSession* viewer : context.registry.snapshot()) {
            if (!only.empty() && only.find(viewer->uuid().toString()) == std::string::npos) {
                continue;
            }
            if (viewer->worldName == session.worldName && viewer->connection().sendPlayPacket) {
                viewer->connection().sendPlayPacket(play::HurtAnimationPacket::kId,
                                                    [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
            }
        }
        return true;
    }
    if (name == "send_equipment") {
        if (!connection.sendPlayPacket) {
            return false;
        }
        static constexpr std::array<play::SetEquipmentPacket::SlotType, 8> kSlots{
            play::SetEquipmentPacket::SlotType::MainHand, play::SetEquipmentPacket::SlotType::OffHand,
            play::SetEquipmentPacket::SlotType::Feet,     play::SetEquipmentPacket::SlotType::Legs,
            play::SetEquipmentPacket::SlotType::Chest,    play::SetEquipmentPacket::SlotType::Head,
            play::SetEquipmentPacket::SlotType::Body,     play::SetEquipmentPacket::SlotType::Saddle};
        const auto slot = static_cast<std::size_t>(std::clamp<std::int64_t>(asInt(arg(args, 1)), 0, 7));
        const std::string snbt = asText(arg(args, 2));
        const std::optional<storage::StoredItemStack> stack = snbt.empty() ? std::nullopt : decodeItem(snbt);
        const play::SetEquipmentPacket packet{
            .entityId = static_cast<std::int32_t>(asInt(first)),
            .slots = {
                play::SetEquipmentPacket::Slot{.type = kSlots[slot], .stack = session.inventory.toWireStack(stack)}}};
        connection.sendPlayPacket(play::SetEquipmentPacket::kId,
                                  [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
        return true;
    }
    if (name == "send_effect" || name == "send_effect_removed") {
        if (!connection.sendPlayPacket) {
            return false;
        }
        const auto effect = wire::common::Identifier::parse(asText(arg(args, 1)).find(':') == std::string::npos
                                                                ? "minecraft:" + asText(arg(args, 1))
                                                                : asText(arg(args, 1)));
        const std::optional<std::int32_t> id =
            effect.has_value() ? world::mobEffects::protocolIdOf(*effect) : std::nullopt;
        if (!id.has_value()) {
            return false;
        }
        const auto entityId = static_cast<std::int32_t>(asInt(first));
        if (name == "send_effect_removed") {
            const play::RemoveMobEffectPacket packet{.entityId = entityId, .effect = *id};
            connection.sendPlayPacket(play::RemoveMobEffectPacket::kId,
                                      [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
            return true;
        }
        const play::UpdateMobEffectPacket packet{.entityId = entityId,
                                                 .effect = *id,
                                                 .amplifier = static_cast<std::int32_t>(asInt(arg(args, 2))),
                                                 .durationTicks = static_cast<std::int32_t>(asInt(arg(args, 3))),
                                                 .ambient = asBool(arg(args, 4)),
                                                 .visible = asBool(arg(args, 5), true),
                                                 .showIcon = asBool(arg(args, 6), true)};
        connection.sendPlayPacket(play::UpdateMobEffectPacket::kId,
                                  [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
        return true;
    }
    if (name == "server_links") {
        if (!connection.sendPlayPacket) {
            return false;
        }
        static constexpr std::array<std::string_view, 10> kKinds{
            "bug_report", "community_guidelines", "support", "status", "feedback", "community", "website", "forums",
            "news",       "announcements"};
        const auto* flat = std::get_if<std::vector<std::string>>(&first);
        const std::vector<std::string> rows = flat == nullptr ? std::vector<std::string>{} : *flat;
        connection.sendPlayPacket(kServerLinksId, [&rows](wire::io::ByteBuffer& buffer) {
            wire::protocol::varInt::write(buffer, static_cast<std::int32_t>(rows.size() / 3));
            for (std::size_t at = 0; at + 2 < rows.size(); at += 3) {
                const auto kind = std::ranges::find(kKinds, rows[at]);
                if (kind != kKinds.end()) {
                    wire::protocol::writeBoolean(buffer, true);
                    wire::protocol::varInt::write(buffer, static_cast<std::int32_t>(kind - kKinds.begin()));
                } else {
                    wire::protocol::writeBoolean(buffer, false);
                    wire::protocol::writeAnonymousNbt(
                        buffer, wire::text::componentNbt::serialize(wire::text::miniMessage::parse(rows[at + 1])));
                }
                wire::protocol::writeString(buffer, rows[at + 2]);
            }
        });
        return true;
    }
    if (name == "store_cookie") {
        if (!connection.sendPlayPacket) {
            return false;
        }
        const std::string key = asText(first);
        const std::string payload = asText(arg(args, 1));
        if (payload.size() > kMaxCookieBytes) {
            return false;
        }
        connection.sendPlayPacket(kStoreCookieId, [&](wire::io::ByteBuffer& buffer) {
            wire::protocol::writeString(buffer, key);
            wire::protocol::writeByteArray(
                buffer,
                std::span<const std::uint8_t>{reinterpret_cast<const std::uint8_t*>(payload.data()), payload.size()});
        });
        return true;
    }
    if (name == "cookie_request") {
        if (!connection.sendPlayPacket) {
            return false;
        }
        const std::string key = asText(first);
        extras.state.erase("cookie:" + key);
        connection.sendPlayPacket(kCookieRequestId,
                                  [&key](wire::io::ByteBuffer& buffer) { wire::protocol::writeString(buffer, key); });
        return true;
    }
    if (name == "cookie_answer") {
        const auto found = extras.state.find("cookie:" + asText(first));
        return found == extras.state.end() ? Value{} : Value{found->second};
    }
    if (name == "open_book") {
        if (!connection.sendPlayPacket) {
            return false;
        }
        const std::optional<storage::StoredItemStack> book = decodeItem(asText(first));
        const play::ContainerSetSlotPacket slot{.containerId = 0,
                                                .stateId = 0,
                                                .slot = static_cast<std::int16_t>(36 + session.inventory.heldSlot()),
                                                .item = session.inventory.toWireStack(book)};
        connection.sendPlayPacket(play::ContainerSetSlotPacket::kId,
                                  [&slot](wire::io::ByteBuffer& buffer) { slot.write(buffer); });
        connection.sendPlayPacket(kOpenBookId,
                                  [](wire::io::ByteBuffer& buffer) { wire::protocol::varInt::write(buffer, 0); });
        if (connection.sendInventoryAll) {
            connection.sendInventoryAll();
        }
        return true;
    }
    if (name == "entity_event") {
        if (!connection.sendPlayPacket) {
            return false;
        }
        const play::EntityEventPacket packet{.entityId = static_cast<std::int32_t>(asInt(first)),
                                             .eventId = static_cast<std::int8_t>(asInt(arg(args, 1)))};
        connection.sendPlayPacket(play::EntityEventPacket::kId,
                                  [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
        return true;
    }
    if (name == "look_at") {
        if (!connection.sendPlayPacket) {
            return false;
        }
        const double x = asReal(first);
        const double y = asReal(arg(args, 1));
        const double z = asReal(arg(args, 2));
        const bool eyes = asBool(arg(args, 3));
        connection.sendPlayPacket(kLookAtId, [&](wire::io::ByteBuffer& buffer) {
            wire::protocol::varInt::write(buffer, eyes ? 1 : 0);
            buffer.writeDouble(x);
            buffer.writeDouble(y);
            buffer.writeDouble(z);
            wire::protocol::writeBoolean(buffer, false);
        });
        return true;
    }
    if (name == "chat_completions") {
        if (!connection.sendPlayPacket) {
            return false;
        }
        const auto* list = std::get_if<std::vector<std::string>>(&arg(args, 1));
        const std::int32_t action = static_cast<std::int32_t>(asInt(first));
        connection.sendPlayPacket(kCustomCompletionsId, [&](wire::io::ByteBuffer& buffer) {
            wire::protocol::varInt::write(buffer, action);
            const std::size_t count = list == nullptr ? 0 : list->size();
            wire::protocol::varInt::write(buffer, static_cast<std::int32_t>(count));
            for (std::size_t at = 0; at < count; ++at) {
                wire::protocol::writeString(buffer, (*list)[at]);
            }
        });
        return true;
    }
    static_cast<void>(verb);
    return std::nullopt;
}

} // namespace basalt::server::runtime::plugins::playerOps
