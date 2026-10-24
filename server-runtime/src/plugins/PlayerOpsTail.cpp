#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/player/PlayerPersistence.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/Dialogs.hpp>
#include <basalt/server/runtime/plugins/PlayerOps.hpp>
#include <basalt/server/runtime/plugins/PluginItemConversion.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/storage/PlayerStore.hpp>
#include <basalt/wire/common/json/Json.hpp>
#include <basalt/wire/nbt/Snbt.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>
#include <basalt/wire/protocol/play/BlockEntityPackets.hpp>
#include <basalt/wire/text/ComponentJson.hpp>

#include <algorithm>

namespace basalt::server::runtime::plugins::playerOps {
namespace {

std::optional<wire::nbt::NbtCompound>& shoulderSide(player::PlayerSession& session, const std::int64_t side) {
    return side == 0 ? session.shoulderLeft : session.shoulderRight;
}

std::optional<wire::nbt::NbtCompound> compoundOf(const std::string& snbt) {
    if (snbt.empty()) {
        return std::nullopt;
    }
    const std::expected<wire::nbt::NbtTag, wire::nbt::NbtError> parsed = wire::nbt::snbt::parse(snbt);
    const wire::nbt::NbtCompound* root = parsed.has_value() ? parsed->get<wire::nbt::NbtCompound>() : nullptr;
    return root == nullptr ? std::nullopt : std::optional{*root};
}

wire::nbt::NbtCompound itemNbt(const storage::StoredItemStack& stack) {
    wire::nbt::NbtCompound item;
    item.putString("id", stack.id);
    item.putInt("count", stack.count);
    if (!stack.components.empty()) {
        item.set("components", wire::nbt::NbtTag::ofCompound(stack.components));
    }
    return item;
}

wire::nbt::NbtTag itemsNbt(const storage::StoredInventory& inventory) {
    wire::nbt::NbtList list{wire::nbt::NbtType::Compound};
    for (std::size_t slot = 0; slot < inventory.slots.size(); ++slot) {
        if (!inventory.slots[slot].has_value()) {
            continue;
        }
        wire::nbt::NbtCompound entry = itemNbt(*inventory.slots[slot]);
        entry.putByte("Slot", static_cast<std::int8_t>(slot));
        list.push(wire::nbt::NbtTag::ofCompound(std::move(entry)));
    }
    return wire::nbt::NbtTag::ofList(std::move(list));
}

wire::nbt::NbtTag vector3(double x, double y, double z) {
    wire::nbt::NbtList list{wire::nbt::NbtType::Double};
    for (const double value : {x, y, z}) {
        list.push(wire::nbt::NbtTag::ofDouble(value));
    }
    return wire::nbt::NbtTag::ofList(std::move(list));
}

/// parity: `Entity.saveAsPassenger` of a player, the part of it that Basalt keeps.
wire::nbt::NbtCompound playerNbt(const player::PlayerSession& session) {
    const storage::StoredPlayer stored = player::playerPersistence::toStored(session, session.worldName, 0, 0, 0);
    wire::nbt::NbtCompound root;
    root.putString("id", "minecraft:player");
    root.set("Pos", vector3(stored.x, stored.y, stored.z));
    root.set("Motion", vector3(0.0, 0.0, 0.0));
    wire::nbt::NbtList rotation{wire::nbt::NbtType::Float};
    rotation.push(wire::nbt::NbtTag::ofFloat(stored.yaw));
    rotation.push(wire::nbt::NbtTag::ofFloat(stored.pitch));
    root.set("Rotation", wire::nbt::NbtTag::ofList(std::move(rotation)));
    root.putString("Dimension", stored.world);
    root.putFloat("Health", stored.health);
    root.putInt("foodLevel", stored.food);
    root.putFloat("foodSaturationLevel", stored.saturation);
    root.putFloat("foodExhaustionLevel", stored.exhaustion);
    root.putInt("XpLevel", stored.xpLevel);
    root.putFloat("XpP", stored.xpProgress);
    root.putInt("XpTotal", stored.xpTotal);
    root.putInt("playerGameType", static_cast<std::int32_t>(stored.gamemode));
    root.set("Inventory", itemsNbt(stored.inventory));
    root.set("EnderItems", itemsNbt(stored.enderChest));
    const wire::common::Uuid uuid = session.uuid();
    root.putIntArray("UUID", {static_cast<std::int32_t>(uuid.high() >> 32), static_cast<std::int32_t>(uuid.high()),
                              static_cast<std::int32_t>(uuid.low() >> 32), static_cast<std::int32_t>(uuid.low())});
    if (!stored.tags.empty()) {
        wire::nbt::NbtList tags{wire::nbt::NbtType::String};
        for (const std::string& tag : stored.tags) {
            tags.push(wire::nbt::NbtTag::ofString(tag));
        }
        root.set("Tags", wire::nbt::NbtTag::ofList(std::move(tags)));
    }
    return root;
}

void broadcastWaypoint(Context& context) {
    player::PlayerSession& session = context.session;
    const player::PlayerExtras& extras = session.extras;
    const std::string style = extras.waypointStyle;
    const std::optional<std::int32_t> color =
        extras.waypointColor < 0 ? std::nullopt : std::optional{extras.waypointColor};
    for (const player::PlayerSession* viewer : context.registry.snapshot()) {
        if (viewer->worldName == session.worldName && viewer->connection().sendWaypoint) {
            viewer->connection().sendWaypoint(session.uuid(), session.entityId(), style, color, session.x, session.y,
                                              session.z, true);
        }
    }
}

std::optional<server::world::loot::ItemStack> lootStackOf(const wire::nbt::NbtCompound* tag) {
    if (tag == nullptr) {
        return std::nullopt;
    }
    const std::expected<wire::common::Identifier, wire::common::IdentifierError> id =
        wire::common::Identifier::parse(std::string{tag->getString("id")});
    if (!id.has_value()) {
        return std::nullopt;
    }
    const wire::nbt::NbtCompound* components = tag->getCompound("components");
    return server::world::loot::ItemStack{.item = *id,
                                          .count = std::max(tag->getInt("count", 1), 1),
                                          .components = components == nullptr ? wire::nbt::NbtCompound{} : *components};
}

std::vector<world::trades::Offer> offersOf(const std::string& snbt) {
    std::vector<world::trades::Offer> offers;
    const std::expected<wire::nbt::NbtTag, wire::nbt::NbtError> parsed = wire::nbt::snbt::parse(snbt);
    const wire::nbt::NbtCompound* root = parsed.has_value() ? parsed->get<wire::nbt::NbtCompound>() : nullptr;
    const wire::nbt::NbtList* list = root == nullptr ? nullptr : root->getList("offers");
    if (list == nullptr) {
        return offers;
    }
    for (const wire::nbt::NbtTag& element : *list) {
        const wire::nbt::NbtCompound* entry = element.get<wire::nbt::NbtCompound>();
        if (entry == nullptr) {
            continue;
        }
        std::optional<server::world::loot::ItemStack> buy = lootStackOf(entry->getCompound("buy"));
        std::optional<server::world::loot::ItemStack> sell = lootStackOf(entry->getCompound("sell"));
        if (!buy.has_value() || !sell.has_value()) {
            continue;
        }
        offers.push_back(world::trades::Offer{.costA = *buy,
                                              .costB = lootStackOf(entry->getCompound("buy2")),
                                              .result = *sell,
                                              .maxUses = entry->getInt("max_uses", 12),
                                              .experience = entry->getInt("xp"),
                                              .priceMultiplier = entry->getFloat("price_multiplier", 0.0F),
                                              .uses = entry->getInt("uses"),
                                              .demand = entry->getInt("demand"),
                                              .specialPrice = entry->getInt("special_price")});
    }
    return offers;
}

} // namespace

std::optional<Value> tail(Context& context, const Verb verb, const std::string_view name, const Args& args) {
    player::PlayerSession& session = context.session;
    player::PlayerExtras& extras = session.extras;
    const player::PlayerConnection& connection = session.connection();
    const Value& first = arg(args, 0);
    if (name == "pose") {
        return static_cast<std::int64_t>(session.dead ? wire::protocol::play::Pose::Dying : session.pose);
    }
    if (name == "use_ticks" || name == "use_remaining") {
        if (verb == Verb::Set) {
            return connection.itemUse && connection.itemUse("set_remaining", asInt(first)) != 0;
        }
        return connection.itemUse ? connection.itemUse(name == "use_ticks" ? "used" : "remaining", 0) : -1;
    }
    if (name == "use_start") {
        return connection.itemUse && connection.itemUse("start", asBool(first) ? 1 : 0) != 0;
    }
    if (name == "use_complete" || name == "use_clear") {
        return connection.itemUse && connection.itemUse(name == "use_complete" ? "complete" : "clear", 0) != 0;
    }
    if (name == "visual_fire") {
        if (verb == Verb::Get) {
            return static_cast<std::int64_t>(extras.visualFire);
        }
        extras.visualFire = static_cast<std::int32_t>(std::clamp<std::int64_t>(asInt(first, -1), -1, 1));
        return true;
    }
    if (name == "fixed_pose") {
        if (verb == Verb::Get) {
            return static_cast<std::int64_t>(extras.fixedPose);
        }
        extras.fixedPose = static_cast<std::int32_t>(asInt(first, -1));
        if (extras.fixedPose >= 0) {
            session.pose = static_cast<wire::protocol::play::Pose>(extras.fixedPose);
        }
        return true;
    }
    if (name == "no_physics") {
        const world::LoadedWorld* world = worldOf(context);
        const std::int64_t now = world == nullptr ? 0 : world->gameTime();
        if (verb == Verb::Get) {
            return session.gamemode == wire::protocol::play::Gamemode::Spectator || extras.noPhysicsTick == now;
        }
        extras.noPhysicsTick = asBool(first) ? now : -1;
        return true;
    }
    if (name == "slot_limit") {
        if (verb == Verb::Get) {
            return static_cast<std::int64_t>(session.inventory.slotLimit());
        }
        session.inventory.setSlotLimit(static_cast<std::int32_t>(
            std::clamp<std::int64_t>(asInt(first), 1, player::PlayerInventory::kSlotLimit)));
        return true;
    }
    if (name == "freeze_locked") {
        if (verb == Verb::Get) {
            return extras.freezeLocked;
        }
        extras.freezeLocked = asBool(first);
        return true;
    }
    if (name == "waypoint_style") {
        if (verb == Verb::Get) {
            return extras.waypointStyle;
        }
        extras.waypointStyle = asText(first, "minecraft:default");
        broadcastWaypoint(context);
        return true;
    }
    if (name == "waypoint_color") {
        if (verb == Verb::Get) {
            return static_cast<std::int64_t>(extras.waypointColor);
        }
        extras.waypointColor = static_cast<std::int32_t>(asInt(first, -1));
        broadcastWaypoint(context);
        return true;
    }
    if (name == "body_yaw") {
        if (verb == Verb::Get) {
            return static_cast<double>(extras.bodyYaw.value_or(session.yaw));
        }
        extras.bodyYaw = static_cast<float>(asReal(first));
        return true;
    }
    if (name == "hurt_direction") {
        if (verb == Verb::Get) {
            return static_cast<double>(extras.hurtDirection);
        }
        extras.hurtDirection = static_cast<float>(asReal(first));
        return true;
    }
    if (name == "no_action_ticks") {
        if (verb == Verb::Get) {
            return static_cast<std::int64_t>(extras.noActionTicks);
        }
        extras.noActionTicks = static_cast<std::int32_t>(asInt(first));
        return true;
    }
    if (name == "maximum_air") {
        if (verb == Verb::Get) {
            return static_cast<std::int64_t>(extras.maxAir);
        }
        extras.maxAir = std::max<std::int32_t>(1, static_cast<std::int32_t>(asInt(first, 300)));
        return true;
    }
    if (name == "shield_delay") {
        if (verb == Verb::Get) {
            return static_cast<std::int64_t>(extras.shieldBlockingDelay);
        }
        extras.shieldBlockingDelay = std::max<std::int32_t>(0, static_cast<std::int32_t>(asInt(first, 5)));
        return true;
    }
    if (name == "max_no_damage_ticks") {
        if (verb == Verb::Get) {
            return static_cast<std::int64_t>(extras.maxNoDamageTicks);
        }
        extras.maxNoDamageTicks = std::max<std::int32_t>(0, static_cast<std::int32_t>(asInt(first, 20)));
        return true;
    }
    if (name == "saturated_regen_ticks" || name == "unsaturated_regen_ticks" || name == "starvation_ticks") {
        std::int32_t& ticks = name == "saturated_regen_ticks"     ? session.food.saturatedRegenTicks
                              : name == "unsaturated_regen_ticks" ? session.food.unsaturatedRegenTicks
                                                                  : session.food.starveTicks;
        if (verb == Verb::Get) {
            return static_cast<std::int64_t>(ticks);
        }
        ticks = std::max<std::int32_t>(1, static_cast<std::int32_t>(asInt(first, 1)));
        return true;
    }
    if (name == "killer") {
        if (verb == Verb::Get) {
            for (const player::PlayerSession* other : context.registry.snapshot()) {
                if (other->entityId() == session.lastHurtByPlayerId && session.lastHurtByPlayerTime.load() > 0) {
                    return other->uuid().toString();
                }
            }
            return Value{};
        }
        const std::string wanted = asText(first);
        session.lastHurtByPlayerTime.store(0);
        for (const player::PlayerSession* other : context.registry.snapshot()) {
            if (other->uuid().toString() == wanted) {
                session.lastHurtByPlayerId = other->entityId();
                session.lastHurtByPlayerTime.store(100);
            }
        }
        return true;
    }
    if (name == "as_string") {
        return wire::nbt::snbt::stringify(wire::nbt::NbtTag::ofCompound(playerNbt(session)));
    }
    if (name == "shoulder") {
        const std::optional<wire::nbt::NbtCompound>& held = shoulderSide(session, asInt(first));
        return held.has_value() ? wire::nbt::snbt::stringify(wire::nbt::NbtTag::ofCompound(*held)) : std::string{};
    }
    if (name == "set_shoulder") {
        shoulderSide(session, asInt(first)) = compoundOf(asText(arg(args, 1)));
        return true;
    }
    if (name == "firework_boost") {
        return connection.attachFirework ? static_cast<std::int64_t>(connection.attachFirework(asText(first)))
                                         : static_cast<std::int64_t>(-1);
    }
    if (name == "riptide") {
        session.autoSpinTicks = std::max<std::int32_t>(0, static_cast<std::int32_t>(asInt(first)));
        session.autoSpinDamage = static_cast<float>(asReal(arg(args, 1)));
        session.autoSpinItem = stackFromSnbt(asText(arg(args, 2)));
        session.shoulderDropPending = true;
        return true;
    }
    if (name == "set_riptiding") {
        session.autoSpinTicks = asBool(first) ? std::max(session.autoSpinTicks, 1) : 0;
        return true;
    }
    if (name == "sleep") {
        return connection.sleepAt &&
               connection.sleepAt(static_cast<std::int32_t>(asInt(first)),
                                  static_cast<std::int32_t>(asInt(arg(args, 1))),
                                  static_cast<std::int32_t>(asInt(arg(args, 2))), asBool(arg(args, 3)));
    }
    if (name == "block_entity") {
        if (!connection.sendPlayPacket) {
            return false;
        }
        const wire::common::Identifier block =
            wire::common::Identifier::parse(asText(arg(args, 3))).value_or(wire::common::Identifier::vanilla("air"));
        const wire::common::Identifier* type = server::registry::blockEntityTypes::ofBlock(block);
        const std::optional<std::int32_t> typeId =
            type == nullptr ? std::nullopt : server::registry::blockEntityTypes::idOf(*type);
        if (!typeId.has_value()) {
            return false;
        }
        const wire::protocol::play::BlockEntityDataPacket packet{
            .pos = wire::protocol::play::BlockPos{static_cast<std::int32_t>(asInt(first)),
                                                  static_cast<std::int32_t>(asInt(arg(args, 1))),
                                                  static_cast<std::int32_t>(asInt(arg(args, 2)))},
            .typeId = *typeId,
            .tag = compoundOf(asText(arg(args, 4)))};
        connection.sendPlayPacket(wire::protocol::play::BlockEntityDataPacket::kId,
                                  [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
        return true;
    }
    if (name == "combat") {
        std::vector<std::string> flat;
        for (const player::PlayerSession::CombatRecord& record : session.combatRecords) {
            flat.insert(flat.end(),
                        {record.damageType, record.messageId, std::to_string(record.directId),
                         std::to_string(record.causeId), std::to_string(record.damage),
                         std::to_string(record.fallDistance), record.fallLocation, std::to_string(record.tick)});
        }
        return flat;
    }
    if (name == "combat_clear") {
        session.combatRecords.clear();
        return true;
    }
    if (name == "combat_push") {
        session.combatRecords.push_back(
            player::PlayerSession::CombatRecord{.damageType = asText(first),
                                                .messageId = asText(arg(args, 1)),
                                                .directId = static_cast<std::int32_t>(asInt(arg(args, 2), -1)),
                                                .causeId = static_cast<std::int32_t>(asInt(arg(args, 3), -1)),
                                                .damage = static_cast<float>(asReal(arg(args, 4))),
                                                .fallDistance = static_cast<float>(asReal(arg(args, 5))),
                                                .fallLocation = asText(arg(args, 6)),
                                                .tick = asInt(arg(args, 7))});
        return true;
    }
    if (name == "set_profile") {
        session.profileProperties.clear();
        for (std::size_t at = 0; at + 2 < args.size(); at += 3) {
            session.profileProperties.push_back(wire::protocol::login::GameProfileProperty{
                .name = asText(args[at]),
                .value = asText(args[at + 1]),
                .signature = asText(args[at + 2]).empty() ? std::nullopt : std::optional{asText(args[at + 2])}});
        }
        ++extras.profileVersion;
        if (connection.refreshProfile) {
            connection.refreshProfile();
        }
        return true;
    }
    if (name == "window_title") {
        const auto title = wire::common::parseJson(asText(first));
        return connection.retitleScreen &&
               connection.retitleScreen(title.has_value() ? wire::text::componentJson::deserialize(*title)
                                                          : wire::text::Component::empty());
    }
    if (name == "show_dialog") {
        const std::string written = asText(first);
        if (!written.starts_with('{')) {
            return connection.showDialogNamed && connection.showDialogNamed(written);
        }
        const std::optional<wire::nbt::NbtCompound> dialog = compoundOf(written);
        if (!dialog.has_value()) {
            return false;
        }
        if (connection.showDialogData) {
            return connection.showDialogData(*dialog);
        }
        const std::optional<world::dialogs::Dialog> form =
            world::dialogs::read(*dialog, [](const wire::common::Identifier&) -> const wire::nbt::NbtCompound* {
                return nullptr;
            });
        if (!form.has_value() || !connection.showForm) {
            return false;
        }
        connection.showForm(*form);
        return true;
    }
    if (name == "reenter_configuration") {
        return connection.reenterConfiguration && connection.reenterConfiguration();
    }
    if (name == "clear_dialog") {
        constexpr std::int32_t kClearDialogId = 142;
        if (!connection.sendPlayPacket) {
            return false;
        }
        connection.sendPlayPacket(kClearDialogId, [](wire::io::ByteBuffer&) {});
        return true;
    }
    if (name == "delete_chat") {
        constexpr std::int32_t kDeleteChatId = 31;
        const std::string hex = asText(first);
        if (!connection.sendPlayPacket || hex.size() != 512) {
            return false;
        }
        connection.sendPlayPacket(kDeleteChatId, [&hex](wire::io::ByteBuffer& buffer) {
            wire::protocol::varInt::write(buffer, 0);
            for (std::size_t at = 0; at < hex.size(); at += 2) {
                buffer.writeByte(static_cast<std::uint8_t>(std::stoi(hex.substr(at, 2), nullptr, 16)));
            }
        });
        return true;
    }
    if (name == "report_details") {
        if (!connection.sendPlayPacket) {
            return false;
        }
        constexpr std::int32_t kCustomReportDetailsId = 139;
        const std::size_t count = args.size() / 2;
        connection.sendPlayPacket(kCustomReportDetailsId, [&](wire::io::ByteBuffer& buffer) {
            wire::protocol::varInt::write(buffer, static_cast<std::int32_t>(count));
            for (std::size_t at = 0; at < count; ++at) {
                wire::protocol::writeString(buffer, asText(args[at * 2]));
                wire::protocol::writeString(buffer, asText(args[at * 2 + 1]));
            }
        });
        return true;
    }
    if (name == "open_trades") {
        return connection.openTrades && connection.openTrades(static_cast<std::int32_t>(asInt(first)));
    }
    if (name == "open_merchant") {
        if (!connection.openMerchant) {
            return false;
        }
        const auto title = wire::common::parseJson(asText(first));
        return connection.openMerchant(title.has_value() ? wire::text::componentJson::deserialize(*title)
                                                         : wire::text::Component::empty(),
                                       offersOf(asText(arg(args, 1))));
    }
    if (name == "resend_recipes") {
        if (!connection.resendRecipes) {
            return false;
        }
        connection.resendRecipes();
        return true;
    }
    if (name == "load_data") {
        storage::PlayerStore* store = context.hooks.store ? context.hooks.store() : nullptr;
        if (store == nullptr || !store->isOpen()) {
            return false;
        }
        const std::optional<storage::StoredPlayer> stored = store->players().load(session.uuid().toString());
        if (!stored.has_value()) {
            return false;
        }
        player::playerPersistence::restore(session, *stored);
        if (connection.setGamemode) {
            connection.setGamemode(session.gamemode);
        }
        if (connection.teleportTo) {
            connection.teleportTo(session.x, session.y, session.z, session.yaw, session.pitch);
        }
        if (connection.sendInventoryAll) {
            connection.sendInventoryAll();
        }
        if (connection.syncHealth) {
            connection.syncHealth();
        }
        if (connection.syncExperience) {
            connection.syncExperience();
        }
        return true;
    }
    return std::nullopt;
}

} // namespace basalt::server::runtime::plugins::playerOps
