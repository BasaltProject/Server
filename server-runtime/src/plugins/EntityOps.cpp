#include "EntityOpsInternal.hpp"

#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/runtime/entity/Bees.hpp>
#include <basalt/server/runtime/entity/Breeding.hpp>
#include <basalt/server/runtime/entity/EntityCombat.hpp>
#include <basalt/server/runtime/entity/EntityNbt.hpp>
#include <basalt/server/runtime/entity/FireworkRockets.hpp>
#include <basalt/server/runtime/entity/FishingHooks.hpp>
#include <basalt/server/runtime/entity/MobEquipment.hpp>
#include <basalt/server/runtime/entity/MobMisc.hpp>
#include <basalt/server/runtime/entity/MobRules.hpp>
#include <basalt/server/runtime/entity/MobStatusEffects.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/Villagers.hpp>
#include <basalt/server/runtime/entity/ai/Controls.hpp>
#include <basalt/server/runtime/player/PlayerConnection.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/plugins/EntityOps.hpp>
#include <basalt/server/runtime/plugins/MobStateOps.hpp>
#include <basalt/server/runtime/plugins/ServerObjectModel.hpp>
#include <basalt/server/runtime/world/BlockInventory.hpp>
#include <basalt/server/runtime/world/EnderEyes.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/wire/nbt/NbtIO.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <span>
#include <string>
#include <utility>

namespace basalt::server::runtime::plugins::entityOps {
namespace {
using namespace internal;

bool setTarget(const Environment& environment, ServerEntity& entity, const Value& value) {
    const std::int64_t id = asInt(value, -1);
    if (id < 0) {
        entity.targetId.reset();
        return true;
    }
    if (environment.world == nullptr || environment.world->state().entity(static_cast<std::int32_t>(id)) == nullptr) {
        return false;
    }
    entity.targetId = static_cast<std::int32_t>(id);
    return true;
}

bool setLeash(const Environment& environment, ServerEntity& entity, const Value& value) {
    const std::int64_t id = asInt(value, -1);
    if (id < 0) {
        entity.leashHolderId.reset();
        entity.leashHolderUuid.reset();
        entity.leashKnotAt.reset();
        return true;
    }
    const ServerEntity* holder =
        environment.world == nullptr ? nullptr : environment.world->state().entity(static_cast<std::int32_t>(id));
    if (holder == nullptr || holder == &entity) {
        return false;
    }
    entity.leashHolderId = holder->entityId();
    entity.leashHolderUuid = holder->uuid().toString();
    entity.leashKnotAt.reset();
    return true;
}

[[nodiscard]] std::vector<std::uint8_t> nbtBytes(NbtCompound compound) {
    return wire::nbt::nbtIO::write(NbtTag::ofCompound(std::move(compound)));
}

[[nodiscard]] Value bytesValue(const std::vector<std::uint8_t>& bytes) {
    return std::string{bytes.begin(), bytes.end()};
}

[[nodiscard]] std::optional<NbtCompound> compoundOf(const Value& value) {
    const auto* text = std::get_if<std::string>(&value);
    if (text == nullptr || text->empty()) {
        return std::nullopt;
    }
    const std::vector<std::uint8_t> bytes{text->begin(), text->end()};
    const auto parsed = wire::nbt::nbtIO::read(bytes);
    const NbtCompound* compound = parsed.has_value() ? parsed->tag.get<NbtCompound>() : nullptr;
    return compound == nullptr ? std::nullopt : std::optional{*compound};
}

[[nodiscard]] std::optional<entity::mobEquipment::Slot> slotOf(std::string_view name) {
    for (std::uint8_t index = 0; index <= static_cast<std::uint8_t>(entity::mobEquipment::Slot::Saddle); ++index) {
        const auto slot = static_cast<entity::mobEquipment::Slot>(index);
        if (entity::mobEquipment::keyOf(slot) == name) {
            return slot;
        }
    }
    return std::nullopt;
}

[[nodiscard]] const NbtTag* walk(const NbtCompound& root, std::string_view path) {
    const NbtCompound* current = &root;
    const NbtTag* found = nullptr;
    while (!path.empty()) {
        const std::size_t dot = path.find('.');
        found = current->find(path.substr(0, dot));
        if (found == nullptr) {
            return nullptr;
        }
        if (dot == std::string_view::npos) {
            return found;
        }
        current = found->get<NbtCompound>();
        if (current == nullptr) {
            return nullptr;
        }
        path = path.substr(dot + 1);
    }
    return found;
}

[[nodiscard]] Value scalarOf(const NbtTag& tag) {
    if (const auto* value = tag.get<std::int8_t>(); value != nullptr) {
        return static_cast<std::int64_t>(*value);
    }
    if (const auto* value = tag.get<std::int16_t>(); value != nullptr) {
        return static_cast<std::int64_t>(*value);
    }
    if (const auto* value = tag.get<std::int32_t>(); value != nullptr) {
        return static_cast<std::int64_t>(*value);
    }
    if (const auto* value = tag.get<std::int64_t>(); value != nullptr) {
        return *value;
    }
    if (const auto* value = tag.get<float>(); value != nullptr) {
        return static_cast<double>(*value);
    }
    if (const auto* value = tag.get<double>(); value != nullptr) {
        return *value;
    }
    if (const auto* value = tag.get<std::string>(); value != nullptr) {
        return *value;
    }
    return Value{};
}

void putAt(NbtCompound& root, std::string_view path, NbtTag tag) {
    NbtCompound* current = &root;
    while (true) {
        const std::size_t dot = path.find('.');
        if (dot == std::string_view::npos) {
            current->set(path, std::move(tag));
            return;
        }
        const std::string_view key = path.substr(0, dot);
        NbtTag* inner = current->find(key);
        if (inner == nullptr || inner->get<NbtCompound>() == nullptr) {
            current->set(key, NbtTag::ofCompound(NbtCompound{}));
            inner = current->find(key);
        }
        current = inner->get<NbtCompound>();
        path = path.substr(dot + 1);
    }
}

bool removeAt(NbtCompound& root, std::string_view path) {
    NbtCompound* current = &root;
    while (true) {
        const std::size_t dot = path.find('.');
        if (dot == std::string_view::npos) {
            const bool held = current->contains(path);
            current->remove(path);
            return held;
        }
        NbtTag* inner = current->find(path.substr(0, dot));
        if (inner == nullptr || inner->get<NbtCompound>() == nullptr) {
            return false;
        }
        current = inner->get<NbtCompound>();
        path = path.substr(dot + 1);
    }
}

[[nodiscard]] NbtTag typedTag(std::string_view type, const Value& value) {
    if (type == "bool" || type == "byte") {
        return NbtTag::ofByte(static_cast<std::int8_t>(asInt(value)));
    }
    if (type == "short") {
        return NbtTag::ofShort(static_cast<std::int16_t>(asInt(value)));
    }
    if (type == "int") {
        return NbtTag::ofInt(static_cast<std::int32_t>(asInt(value)));
    }
    if (type == "long") {
        return NbtTag::ofLong(asInt(value));
    }
    if (type == "float") {
        return NbtTag::ofFloat(static_cast<float>(asReal(value)));
    }
    if (type == "double") {
        return NbtTag::ofDouble(asReal(value));
    }
    if (type == "uuid") {
        const std::optional<wire::common::Uuid> parsed =
            wire::common::Uuid::parse(asText(value).value_or(std::string{}));
        if (parsed.has_value()) {
            const auto word = [](std::uint64_t part) {
                return static_cast<std::int32_t>(static_cast<std::uint32_t>(part));
            };
            return NbtTag::ofIntArray(
                {word(parsed->high() >> 32), word(parsed->high()), word(parsed->low() >> 32), word(parsed->low())});
        }
    }
    if (type == "pos") {
        std::vector<std::int32_t> parts;
        const std::string text = asText(value).value_or(std::string{});
        for (std::size_t from = 0; from <= text.size();) {
            const std::size_t comma = text.find(',', from);
            const std::size_t end = comma == std::string::npos ? text.size() : comma;
            parts.push_back(static_cast<std::int32_t>(std::strtol(text.substr(from, end - from).c_str(), nullptr, 10)));
            from = end + 1;
        }
        return NbtTag::ofIntArray(std::move(parts));
    }
    return NbtTag::ofString(asText(value).value_or(std::string{}));
}

constexpr std::array<std::pair<std::string_view, std::string_view>, 11> kLiveKeys{{
    {"Fire", "fire_ticks"},
    {"Air", "air"},
    {"NoGravity", "no_gravity"},
    {"PersistenceRequired", "persistent"},
    {"NoAI", "no_ai"},
    {"CanPickUpLoot", "can_pick_up_loot"},
    {"CustomName", "custom_name"},
    {"Age", "breed_age"},
    {"InLove", "love_ticks"},
    {"Sitting", "sitting"},
    {"Owner", "owner"},
}};

void merge(NbtCompound& into, const NbtCompound& patch) {
    for (std::size_t index = 0; index < patch.names().size(); ++index) {
        const std::string& key = patch.names()[index];
        const NbtTag& value = patch.values()[index];
        NbtTag* existing = into.find(key);
        const auto* incoming = value.get<NbtCompound>();
        auto* current = existing == nullptr ? nullptr : existing->get<NbtCompound>();
        if (incoming != nullptr && current != nullptr) {
            merge(*current, *incoming);
        } else {
            into.set(key, value);
        }
    }
}

void sourceChanged(ServerEntity& entity) {
    ++entity.sourceNbtVersion;
    if (entity::bees::isBee(entity.type())) {
        entity::bees::reload(entity);
    }
    if (!entity.sourceNbt.has_value()) {
        return;
    }
    const NbtCompound& saved = *entity.sourceNbt;
    const std::string_view path = entity.type().path();
    if (path == "turtle") {
        auto& held = entity::mobMisc::of(entity);
        if (saved.contains("has_egg")) {
            held.turtleHasEgg = saved.getBoolean("has_egg");
        }
        if (const std::span<const std::int32_t> home = saved.getIntArray("home_pos"); home.size() >= 3) {
            held.turtleHome = std::array<std::int32_t, 3>{home[0], home[1], home[2]};
        }
    } else if (path == "wandering_trader") {
        auto& held = entity::mobMisc::of(entity);
        held.traderNoPotion = saved.getBoolean("Paper.CanDrinkPotionDisabled");
        held.traderNoMilk = saved.getBoolean("Paper.CanDrinkMilkDisabled");
    }
}

bool mergeNbt(ServerEntity& entity, const NbtCompound& patch) {
    NbtCompound remainder = patch;
    for (const auto& [key, property] : kLiveKeys) {
        const NbtTag* value = patch.find(key);
        if (value == nullptr) {
            continue;
        }
        const auto found = accessors().find(property);
        if (found != accessors().end() && found->second.write) {
            Value converted = scalarOf(*value);
            found->second.write(entity, converted);
        }
        remainder.remove(key);
    }
    for (const char* skipped : {"id", "UUID", "Pos", "Motion", "Rotation", "Health", "Passengers"}) {
        remainder.remove(skipped);
    }
    if (patch.contains("Offers") && entity.type().path() == "villager") {
        entity::villagers::readOffers(patch, entity);
        remainder.remove("Offers");
    }
    merge(source(entity), remainder);
    sourceChanged(entity);
    if (const NbtTag* health = patch.find("Health"); health != nullptr) {
        entity.health = asReal(scalarOf(*health), entity.health);
    }
    return true;
}

void markDead(ServerEntity& entity) {
    entity.health = 0.0;
    entity.killed = true;
    (server::registry::entityAttributes::isLiving(entity.type()) ? entity.dying : entity.removed) = true;
}

} // namespace

std::optional<Value> get(const Environment& environment, const ServerEntity& entity, std::string_view name) {
    static_cast<void>(environment);
    if (name == "target") {
        return idOrNone(entity.targetId);
    }
    if (name == "leash_holder") {
        return idOrNone(entity.leashHolderId);
    }
    const auto found = accessors().find(name);
    if (found == accessors().end() || !found->second.read) {
        return mobState::get(environment, entity, name);
    }
    return found->second.read(entity);
}

bool set(const Environment& environment, ServerEntity& entity, std::string_view name, const Value& value) {
    if (name == "target") {
        return setTarget(environment, entity, value);
    }
    if (name == "leash_holder") {
        return setLeash(environment, entity, value);
    }
    const auto found = accessors().find(name);
    if (found == accessors().end()) {
        return mobState::set(environment, entity, name, value).value_or(false);
    }
    return found->second.write && found->second.write(entity, value);
}

std::optional<Value> invoke(const Environment& environment, ServerEntity& entity, std::string_view operation,
                            const Arguments& arguments) {
    const ModelHooks* hooks = environment.hooks;
    if (operation == "damage") {
        const std::string cause = asText(at(arguments, 1)).value_or("generic");
        const std::int64_t attackerId = asInt(at(arguments, 2), -1);
        ServerEntity* attacker = attackerId < 0 || environment.world == nullptr
                                     ? nullptr
                                     : environment.world->state().entity(static_cast<std::int32_t>(attackerId));
        if (attacker != nullptr) {
            entity::entityCombat::blame(entity, attacker->entityId(), attacker->entityId(), attacker->type());
        }
        const entity::entityCombat::Hooks combat =
            hooks != nullptr && hooks->combatHooks ? hooks->combatHooks() : entity::entityCombat::Hooks{};
        const entity::entityCombat::Result result =
            entity::entityCombat::hurt(entity, static_cast<float>(asReal(at(arguments, 0))),
                                       attacker == nullptr ? std::nullopt : std::optional{attacker->x},
                                       attacker == nullptr ? std::nullopt : std::optional{attacker->z}, combat, cause,
                                       attacker == nullptr ? std::nullopt : std::optional{attacker->type().toString()});
        if (result.landed && hooks != nullptr && hooks->announceHit && environment.world != nullptr) {
            hooks->announceHit(*environment.world, entity, idOf(cause), attacker == nullptr ? -1 : attacker->entityId(),
                               result.died);
        }
        return result.landed;
    }
    if (operation == "kill") {
        if (entity.removed || entity.dying) {
            return false;
        }
        markDead(entity);
        if (hooks != nullptr && hooks->combatHooks) {
            if (const entity::entityCombat::Hooks combat = hooks->combatHooks(); combat.died) {
                combat.died(entity, std::nullopt, std::nullopt);
            }
        }
        if (hooks != nullptr && hooks->announceHit && environment.world != nullptr) {
            hooks->announceHit(*environment.world, entity, idOf("generic_kill"), -1, true);
        }
        return true;
    }
    if (operation == "add_effect") {
        const world::ActiveEffect effect{
            .effect = idOf(asText(at(arguments, 0)).value_or("")),
            .amplifier = static_cast<std::int32_t>(std::clamp<std::int64_t>(asInt(at(arguments, 2)), 0, 255)),
            .remainingTicks = static_cast<std::int32_t>(asInt(at(arguments, 1))),
            .ambient = asBool(at(arguments, 3)),
            .visible = asBool(at(arguments, 4), true),
            .showIcon = asBool(at(arguments, 5), true)};
        if (!world::mobEffects::protocolIdOf(effect.effect).has_value()) {
            return false;
        }
        if (hooks != nullptr && hooks->giveEffect && environment.world != nullptr) {
            return hooks->giveEffect(*environment.world, entity, effect,
                                     static_cast<std::int32_t>(asInt(at(arguments, 6), -1)));
        }
        return entity::mobStatusEffects::add(entity, effect);
    }
    if (operation == "remove_effect") {
        return entity::mobStatusEffects::remove(entity, idOf(asText(at(arguments, 0)).value_or("")));
    }
    if (operation == "clear_effects") {
        entity::mobStatusEffects::clear(entity);
        return true;
    }
    if (operation == "has_effect") {
        return entity::mobStatusEffects::has(entity, idOf(asText(at(arguments, 0)).value_or("")));
    }
    if (operation == "effects") {
        std::vector<std::string> rows;
        for (const auto& [id, active] : entity.effects) {
            rows.push_back(id.toString());
            rows.push_back(std::to_string(active.remainingTicks));
            rows.push_back(std::to_string(active.amplifier));
            rows.push_back(active.ambient ? "1" : "0");
            rows.push_back(active.visible ? "1" : "0");
            rows.push_back(active.showIcon ? "1" : "0");
        }
        return rows;
    }
    if (operation == "attribute") {
        return entity.attributeWithModifiers(attributeKey(asText(at(arguments, 0)).value_or("")));
    }
    if (operation == "attribute_base") {
        return entity.attribute(attributeKey(asText(at(arguments, 0)).value_or("")));
    }
    if (operation == "attribute_default") {
        const std::string full = attributeKey(asText(at(arguments, 0)).value_or(""));
        const std::string shortName = full.substr(full.find(':') + 1);
        return server::registry::entityAttributes::baseValue(entity.type(), shortName)
            .value_or(entity::attribute::defaultFor(full).value_or(0.0));
    }
    if (operation == "has_attribute") {
        const std::string full = attributeKey(asText(at(arguments, 0)).value_or(""));
        return server::registry::entityAttributes::isLiving(entity.type()) &&
               server::registry::attributeIds::exists(full.substr(full.find(':') + 1));
    }
    if (operation == "set_attribute_base") {
        entity.attributeValues.insert_or_assign(attributeKey(asText(at(arguments, 0)).value_or("")),
                                                asReal(at(arguments, 1)));
        return true;
    }
    if (operation == "attribute_modifiers") {
        std::vector<std::string> rows;
        const auto found = entity.attributeModifiers.find(attributeKey(asText(at(arguments, 0)).value_or("")));
        if (found != entity.attributeModifiers.end()) {
            for (const auto& [id, modifier] : found->second) {
                rows.push_back(modifier.id);
                rows.push_back(std::to_string(modifier.amount));
                rows.emplace_back(entity::attributeOperationName(modifier.operation));
            }
        }
        return rows;
    }
    if (operation == "add_attribute_modifier") {
        const std::optional<entity::AttributeOperation> kind =
            entity::attributeOperationByName(asText(at(arguments, 3)).value_or("add_value"));
        const std::string id = asText(at(arguments, 1)).value_or("");
        if (!kind.has_value() || id.empty()) {
            return false;
        }
        entity.attributeModifiers[attributeKey(asText(at(arguments, 0)).value_or(""))].insert_or_assign(
            id, entity::AttributeModifier{.id = id, .amount = asReal(at(arguments, 2)), .operation = *kind});
        return true;
    }
    if (operation == "remove_attribute_modifier") {
        const auto found = entity.attributeModifiers.find(attributeKey(asText(at(arguments, 0)).value_or("")));
        return found != entity.attributeModifiers.end() &&
               found->second.erase(asText(at(arguments, 1)).value_or("")) > 0;
    }
    if (operation == "equipment") {
        const std::optional<entity::mobEquipment::Slot> slot = slotOf(asText(at(arguments, 0)).value_or(""));
        const auto held = slot.has_value() ? entity::mobEquipment::get(entity, *slot) : std::nullopt;
        return held.has_value() ? bytesValue(nbtBytes(world::blockInventory::writeStack(*held))) : Value{};
    }
    if (operation == "set_equipment") {
        const std::optional<entity::mobEquipment::Slot> slot = slotOf(asText(at(arguments, 0)).value_or(""));
        if (!slot.has_value()) {
            return false;
        }
        const std::optional<NbtCompound> compound = compoundOf(at(arguments, 1));
        std::optional<server::world::loot::ItemStack> stack;
        if (compound.has_value()) {
            stack = world::blockInventory::readStack(*compound);
            if (!stack.has_value()) {
                return false;
            }
        }
        entity::mobEquipment::set(entity, *slot, stack, false);
        return true;
    }
    if (operation == "drop_chance") {
        const NbtCompound* chances =
            entity.sourceNbt.has_value() ? entity.sourceNbt->getCompound("drop_chances") : nullptr;
        const std::string key = asText(at(arguments, 0)).value_or("");
        return chances != nullptr && chances->contains(key) ? static_cast<double>(chances->getFloat(key))
                                                            : static_cast<double>(kDefaultDropChance);
    }
    if (operation == "set_drop_chance") {
        NbtCompound chances;
        if (const NbtCompound* existing = source(entity).getCompound("drop_chances"); existing != nullptr) {
            chances = *existing;
        }
        chances.putFloat(asText(at(arguments, 0)).value_or(""), static_cast<float>(asReal(at(arguments, 1))));
        source(entity).set("drop_chances", NbtTag::ofCompound(std::move(chances)));
        return true;
    }
    if (operation == "mount") {
        ServerEntity* vehicle =
            environment.world == nullptr
                ? nullptr
                : environment.world->state().entity(static_cast<std::int32_t>(asInt(at(arguments, 0), -1)));
        if (vehicle == nullptr || vehicle == &entity || vehicle->removed || entity.vehicleId == vehicle->entityId() ||
            vehicle->vehicleId == entity.entityId()) {
            return false;
        }
        std::int32_t depth = 0;
        for (const ServerEntity* above = vehicle; above != nullptr && above->vehicleId.has_value() && depth < 64;
             ++depth) {
            if (*above->vehicleId == entity.entityId()) {
                return false;
            }
            above = environment.world->state().entity(*above->vehicleId);
        }
        if (entity.vehicleId.has_value()) {
            if (ServerEntity* old = environment.world->state().entity(*entity.vehicleId); old != nullptr) {
                std::erase(old->passengerIds, entity.entityId());
            }
        }
        entity.vehicleId = vehicle->entityId();
        vehicle->passengerIds.push_back(entity.entityId());
        return true;
    }
    if (operation == "dismount") {
        if (!entity.vehicleId.has_value()) {
            return false;
        }
        if (ServerEntity* vehicle =
                environment.world == nullptr ? nullptr : environment.world->state().entity(*entity.vehicleId);
            vehicle != nullptr) {
            std::erase(vehicle->passengerIds, entity.entityId());
        }
        entity.vehicleId.reset();
        entity.boardingCooldown = 60;
        return true;
    }
    if (operation == "eject") {
        bool any = false;
        for (const std::int32_t id : std::vector<std::int32_t>{entity.passengerIds}) {
            if (ServerEntity* rider = environment.world == nullptr ? nullptr : environment.world->state().entity(id);
                rider != nullptr) {
                rider->vehicleId.reset();
                rider->boardingCooldown = 60;
                any = true;
            } else if (environment.players) {
                for (player::PlayerSession* session : environment.players()) {
                    if (session->entityId() == id && session->connection().dismount) {
                        session->connection().dismount();
                        any = true;
                    }
                }
            }
        }
        entity.passengerIds.clear();
        return any;
    }
    if (operation == "merge_nbt") {
        const std::optional<NbtCompound> patch = compoundOf(at(arguments, 0));
        return patch.has_value() && mergeNbt(entity, *patch);
    }
    if (operation == "nbt") {
        NbtCompound whole = entity::entityNbt::encode(entity);
        const std::string path = asText(at(arguments, 0)).value_or("");
        if (path.empty()) {
            return bytesValue(nbtBytes(std::move(whole)));
        }
        const NbtTag* found = walk(whole, path);
        const NbtCompound* inner = found == nullptr ? nullptr : found->get<NbtCompound>();
        return inner == nullptr ? Value{} : bytesValue(nbtBytes(*inner));
    }
    if (operation == "nbt_value") {
        const NbtCompound whole = entity::entityNbt::encode(entity);
        const NbtTag* found = walk(whole, asText(at(arguments, 0)).value_or(""));
        return found == nullptr ? Value{} : scalarOf(*found);
    }
    if (operation == "nbt_set") {
        const std::string path = asText(at(arguments, 0)).value_or("");
        if (path.empty()) {
            return false;
        }
        putAt(source(entity), path, typedTag(asText(at(arguments, 1)).value_or("string"), at(arguments, 2)));
        sourceChanged(entity);
        return true;
    }
    if (operation == "nbt_remove") {
        const std::string path = asText(at(arguments, 0)).value_or("");
        const bool removed = !path.empty() && removeAt(source(entity), path);
        if (removed) {
            sourceChanged(entity);
        }
        return removed;
    }
    if (operation == "is_breed_item") {
        return entity::breeding::isFood(entity.type(), idOf(asText(at(arguments, 0)).value_or("")));
    }
    if (operation == "look_at") {
        entity::ai::lookControl::lookAt(entity, asReal(at(arguments, 0)), asReal(at(arguments, 1)),
                                        asReal(at(arguments, 2)), static_cast<float>(asReal(at(arguments, 3), 10.0)),
                                        static_cast<float>(asReal(at(arguments, 4), 40.0)));
        return true;
    }
    if (operation == "move_to" || operation == "can_reach") {
        if (environment.world == nullptr) {
            return false;
        }
        const double x = asReal(at(arguments, 0));
        const double y = asReal(at(arguments, 1));
        const double z = asReal(at(arguments, 2));
        return operation == "can_reach" ? entity::ai::navigation::canReach(environment.world->state(), entity, x, y, z)
                                        : entity::ai::navigation::moveTo(environment.world->state(), entity, x, y, z,
                                                                         asReal(at(arguments, 3), 1.0));
    }
    if (operation == "stop_path") {
        entity::ai::navigation::stop(entity);
        return true;
    }
    if (operation == "has_path") {
        return entity.path.has_value() && !entity::ai::navigation::isDone(entity);
    }
    if (operation == "knockback") {
        entity::entityCombat::knockback(entity, asReal(at(arguments, 0)), asReal(at(arguments, 1)),
                                        asReal(at(arguments, 2)));
        return true;
    }
    if (operation == "hook_own") {
        if (!environment.players) {
            return false;
        }
        const auto id = static_cast<std::int32_t>(asInt(at(arguments, 0), -1));
        for (player::PlayerSession* session : environment.players()) {
            if (session->entityId() == id) {
                entity.ownerId = id;
                if (entity.hook == nullptr) {
                    entity.hook = std::make_shared<entity::fishingHooks::HookState>();
                }
                entity.hook->ownerId = id;
                session->fishingHook = entity.entityId();
                return true;
            }
        }
        return false;
    }
    if (operation == "hook_retrieve") {
        if (environment.world == nullptr || environment.hooks == nullptr || !environment.hooks->reelIn ||
            !environment.players || !entity.ownerId.has_value()) {
            return static_cast<std::int64_t>(-1);
        }
        for (player::PlayerSession* session : environment.players()) {
            if (session->entityId() == *entity.ownerId) {
                return static_cast<std::int64_t>(
                    environment.hooks->reelIn(*environment.world, *session, asBool(at(arguments, 0))));
            }
        }
        return static_cast<std::int64_t>(-1);
    }
    if (operation == "hit_entity") {
        entity.forcedHitId = static_cast<std::int32_t>(asInt(at(arguments, 0), -1));
        return true;
    }
    if (operation == "land") {
        entity.forcedLand = true;
        return true;
    }
    if (operation == "eye_target") {
        if (entity.enderEye == nullptr || !entity.enderEye->target.has_value()) {
            return std::vector<std::string>{};
        }
        const auto& [x, y, z] = *entity.enderEye->target;
        return std::vector<std::string>{std::to_string(x), std::to_string(y), std::to_string(z)};
    }
    if (operation == "set_eye_target") {
        if (!world::enderEyes::isEye(entity)) {
            return false;
        }
        if (entity.enderEye == nullptr) {
            entity.enderEye = std::make_shared<entity::enderEyes::EyeState>();
        }
        entity.enderEye->target =
            std::array{asReal(at(arguments, 0)), asReal(at(arguments, 1)), asReal(at(arguments, 2))};
        if (asBool(at(arguments, 3))) {
            entity.enderEye->life = 0;
        }
        return true;
    }
    if (operation == "push") {
        entity.velocityX += asReal(at(arguments, 0));
        entity.velocityY += asReal(at(arguments, 1));
        entity.velocityZ += asReal(at(arguments, 2));
        return true;
    }
    return mobState::invoke(environment, entity, operation, arguments);
}

} // namespace basalt::server::runtime::plugins::entityOps
