#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/runtime/entity/Breeding.hpp>
#include <basalt/server/runtime/entity/EntityCombat.hpp>
#include <basalt/server/runtime/entity/EntityNbt.hpp>
#include <basalt/server/runtime/entity/MobEquipment.hpp>
#include <basalt/server/runtime/entity/MobRules.hpp>
#include <basalt/server/runtime/entity/MobStatusEffects.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/Villagers.hpp>
#include <basalt/server/runtime/entity/ai/Controls.hpp>
#include <basalt/server/runtime/player/PlayerConnection.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/plugins/EntityOps.hpp>
#include <basalt/server/runtime/plugins/ServerObjectModel.hpp>
#include <basalt/server/runtime/world/BlockInventory.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/wire/nbt/NbtIO.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <utility>

namespace basalt::server::runtime::plugins::entityOps {
namespace {

using entity::ServerEntity;
using server::plugins::PluginEntityValue;
using wire::common::Identifier;
using wire::nbt::NbtCompound;
using wire::nbt::NbtTag;
using Value = PluginEntityValue;
using Arguments = std::vector<Value>;

constexpr float kDefaultDropChance = 0.085F;

[[nodiscard]] bool asBool(const Value& value, bool fallback = false) {
    if (const auto* flag = std::get_if<bool>(&value); flag != nullptr) {
        return *flag;
    }
    if (const auto* whole = std::get_if<std::int64_t>(&value); whole != nullptr) {
        return *whole != 0;
    }
    return fallback;
}

[[nodiscard]] std::int64_t asInt(const Value& value, std::int64_t fallback = 0) {
    if (const auto* whole = std::get_if<std::int64_t>(&value); whole != nullptr) {
        return *whole;
    }
    if (const auto* real = std::get_if<double>(&value); real != nullptr) {
        return static_cast<std::int64_t>(*real);
    }
    if (const auto* flag = std::get_if<bool>(&value); flag != nullptr) {
        return *flag ? 1 : 0;
    }
    return fallback;
}

[[nodiscard]] double asReal(const Value& value, double fallback = 0.0) {
    if (const auto* real = std::get_if<double>(&value); real != nullptr) {
        return *real;
    }
    if (const auto* whole = std::get_if<std::int64_t>(&value); whole != nullptr) {
        return static_cast<double>(*whole);
    }
    return fallback;
}

[[nodiscard]] std::optional<std::string> asText(const Value& value) {
    const auto* text = std::get_if<std::string>(&value);
    return text == nullptr ? std::nullopt : std::optional{*text};
}

[[nodiscard]] const Value& at(const Arguments& arguments, std::size_t index) {
    static const Value kNone;
    return index < arguments.size() ? arguments[index] : kNone;
}

[[nodiscard]] Identifier idOf(std::string_view text) {
    const std::string full =
        text.find(':') == std::string_view::npos ? "minecraft:" + std::string{text} : std::string{text};
    const auto parsed = Identifier::parse(full);
    return parsed.has_value() ? *parsed : Identifier::vanilla("generic");
}

[[nodiscard]] std::string attributeKey(std::string_view text) {
    return text.find(':') == std::string_view::npos ? "minecraft:" + std::string{text} : std::string{text};
}

NbtCompound& source(ServerEntity& entity) {
    if (!entity.sourceNbt.has_value()) {
        entity.sourceNbt.emplace();
    }
    return *entity.sourceNbt;
}

struct Accessor {
    std::function<Value(const ServerEntity&)> read;
    std::function<bool(ServerEntity&, const Value&)> write;
};

template <typename Member>
Accessor field(Member ServerEntity::* member) {
    using Stored = Member;
    return Accessor{[member](const ServerEntity& entity) -> Value {
                        if constexpr (std::is_same_v<Stored, bool>) {
                            return entity.*member;
                        } else if constexpr (std::is_integral_v<Stored>) {
                            return static_cast<std::int64_t>(entity.*member);
                        } else {
                            return static_cast<double>(entity.*member);
                        }
                    },
                    [member](ServerEntity& entity, const Value& value) {
                        if constexpr (std::is_same_v<Stored, bool>) {
                            entity.*member = asBool(value);
                        } else if constexpr (std::is_integral_v<Stored>) {
                            entity.*member = static_cast<Stored>(asInt(value));
                        } else {
                            entity.*member = static_cast<Stored>(asReal(value));
                        }
                        return true;
                    }};
}

Accessor nbtFlag(std::string key) {
    return Accessor{[key](const ServerEntity& entity) -> Value {
                        return entity.sourceNbt.has_value() && entity.sourceNbt->getBoolean(key);
                    },
                    [key](ServerEntity& entity, const Value& value) {
                        source(entity).putBoolean(key, asBool(value));
                        return true;
                    }};
}

Accessor nbtInt(std::string key) {
    return Accessor{[key](const ServerEntity& entity) -> Value {
                        return static_cast<std::int64_t>(entity.sourceNbt.has_value() ? entity.sourceNbt->getInt(key)
                                                                                      : 0);
                    },
                    [key](ServerEntity& entity, const Value& value) {
                        source(entity).putInt(key, static_cast<std::int32_t>(asInt(value)));
                        return true;
                    }};
}

Accessor nbtFloat(std::string key) {
    return Accessor{[key](const ServerEntity& entity) -> Value {
                        return static_cast<double>(entity.sourceNbt.has_value() ? entity.sourceNbt->getFloat(key)
                                                                                : 0.0F);
                    },
                    [key](ServerEntity& entity, const Value& value) {
                        source(entity).putFloat(key, static_cast<float>(asReal(value)));
                        return true;
                    }};
}

[[nodiscard]] Value idOrNone(const std::optional<std::int32_t>& id) {
    return static_cast<std::int64_t>(id.value_or(-1));
}

using Accessors = std::map<std::string_view, Accessor, std::less<>>;

const Accessors& accessors() {
    static const Accessors kAll = [] {
        Accessors all;
        all["health"] = Accessor{[](const ServerEntity& entity) -> Value { return entity.health; },
                                 [](ServerEntity& entity, const Value& value) {
                                     const double wanted = asReal(value, entity.health);
                                     entity.health = std::clamp(
                                         wanted, 0.0, entity.attributeWithModifiers(entity::attribute::kMaxHealth));
                                     return true;
                                 }};
        all["love_cause"] = Accessor{[](const ServerEntity& entity) -> Value {
                                         return entity.loveCause.has_value() ? Value{*entity.loveCause} : Value{};
                                     },
                                     [](ServerEntity& entity, const Value& value) {
                                         entity.loveCause = asText(value);
                                         return true;
                                     }};
        all["fire_ticks"] = field(&ServerEntity::fireTicks);
        all["frozen_ticks"] = nbtInt("TicksFrozen");
        all["air"] = field(&ServerEntity::airSupply);
        all["max_air"] = Accessor{[](const ServerEntity& entity) -> Value {
                                      return static_cast<std::int64_t>(entity.type().path() == "dolphin"
                                                                           ? ServerEntity::kDolphinMaxAir
                                                                           : ServerEntity::kDefaultMaxAir);
                                  },
                                  nullptr};
        all["fall_distance"] = field(&ServerEntity::fallDistance);
        all["no_gravity"] = field(&ServerEntity::noGravity);
        all["invulnerable"] = nbtFlag("Invulnerable");
        all["glowing"] = nbtFlag("Glowing");
        all["silent"] = nbtFlag("Silent");
        all["invisible"] = nbtFlag("Invisible");
        all["custom_name_visible"] = nbtFlag("CustomNameVisible");
        all["left_handed"] = nbtFlag("LeftHanded");
        all["custom_name"] = Accessor{[](const ServerEntity& entity) -> Value {
                                          return entity.customName.has_value() ? Value{*entity.customName} : Value{};
                                      },
                                      [](ServerEntity& entity, const Value& value) {
                                          const std::optional<std::string> text = asText(value);
                                          if (text.has_value() && !text->empty()) {
                                              entity.customName = *text;
                                          } else {
                                              entity.customName.reset();
                                          }
                                          return true;
                                      }};
        all["persistent"] = field(&ServerEntity::persistent);
        all["ticks_lived"] = field(&ServerEntity::tickCount);
        all["tags"] = Accessor{[](const ServerEntity& entity) -> Value { return entity.tags; },
                               [](ServerEntity& entity, const Value& value) {
                                   const auto* list = std::get_if<std::vector<std::string>>(&value);
                                   entity.tags = list == nullptr ? std::vector<std::string>{} : *list;
                                   return true;
                               }};
        all["vehicle"] =
            Accessor{[](const ServerEntity& entity) -> Value { return idOrNone(entity.vehicleId); }, nullptr};
        all["passengers"] = Accessor{[](const ServerEntity& entity) -> Value {
                                         std::vector<std::string> ids;
                                         for (const std::int32_t id : entity.passengerIds) {
                                             ids.push_back(std::to_string(id));
                                         }
                                         return ids;
                                     },
                                     nullptr};
        all["max_health"] =
            Accessor{[](const ServerEntity& entity) -> Value {
                         return entity.attributeWithModifiers(entity::attribute::kMaxHealth);
                     },
                     [](ServerEntity& entity, const Value& value) {
                         const double base = std::max(asReal(value, 20.0), 1.0);
                         entity.attributeValues.insert_or_assign(std::string{entity::attribute::kMaxHealth}, base);
                         entity.health =
                             std::min(entity.health, entity.attributeWithModifiers(entity::attribute::kMaxHealth));
                         return true;
                     }};
        all["absorption"] = nbtFloat("AbsorptionAmount");
        all["no_ai"] = field(&ServerEntity::noAi);
        all["can_pick_up_loot"] = field(&ServerEntity::canPickUpLoot);
        all["last_damage"] = field(&ServerEntity::lastHurt);
        all["last_damage_cause"] =
            Accessor{[](const ServerEntity& entity) -> Value { return entity.lastDamageCause; }, nullptr};
        all["no_damage_ticks"] = field(&ServerEntity::hurtCooldown);
        all["arrows_in_body"] = field(&ServerEntity::arrowCount);
        all["sleeping"] = Accessor{[](const ServerEntity& entity) -> Value { return entity.sleeping; }, nullptr};
        all["breed_age"] = field(&ServerEntity::age);
        all["love_ticks"] = field(&ServerEntity::loveTicks);
        all["sitting"] = field(&ServerEntity::sitting);
        all["tamed_horse"] = field(&ServerEntity::tamedHorse);
        all["fuse"] = field(&ServerEntity::fuse);
        all["object_data"] = field(&ServerEntity::objectData);
        all["bolt_visual_only"] = field(&ServerEntity::boltVisualOnly);
        all["bolt_life"] = field(&ServerEntity::boltLife);
        all["bolt_flashes"] = field(&ServerEntity::boltFlashes);
        all["category"] = Accessor{
            [](const ServerEntity& entity) -> Value { return std::string{entity::mobs::categoryOf(entity.type())}; },
            nullptr};
        all["loot_table"] =
            Accessor{[](const ServerEntity& entity) -> Value {
                         return entity.deathLootTable.has_value() ? Value{*entity.deathLootTable} : Value{};
                     },
                     [](ServerEntity& entity, const Value& value) {
                         entity.deathLootTable = asText(value);
                         return true;
                     }};
        all["loot_seed"] = Accessor{[](const ServerEntity& entity) -> Value { return entity.deathLootTableSeed; },
                                    [](ServerEntity& entity, const Value& value) {
                                        entity.deathLootTableSeed = asInt(value);
                                        return true;
                                    }};
        all["critical"] = field(&ServerEntity::critical);
        all["pierce_level"] = field(&ServerEntity::pierceLevel);
        all["explosion_power"] = field(&ServerEntity::explosionPower);
        all["owner_id"] = Accessor{[](const ServerEntity& entity) -> Value { return idOrNone(entity.ownerId); },
                                   [](ServerEntity& entity, const Value& value) {
                                       const std::int64_t id = asInt(value, -1);
                                       if (id < 0) {
                                           entity.ownerId.reset();
                                           entity.ownerUuid.reset();
                                       } else {
                                           entity.ownerId = static_cast<std::int32_t>(id);
                                       }
                                       return true;
                                   }};
        all["arrow_pickup"] =
            Accessor{[](const ServerEntity& entity) -> Value { return static_cast<std::int64_t>(entity.arrowPickup); },
                     [](ServerEntity& entity, const Value& value) {
                         entity.arrowPickup =
                             static_cast<ServerEntity::ArrowPickup>(std::clamp<std::int64_t>(asInt(value), 0, 2));
                         return true;
                     }};
        all["last_hurt_by_player"] = Accessor{[](const ServerEntity& entity) -> Value {
                                                  return entity.lastHurtByPlayer.has_value()
                                                             ? Value{entity.lastHurtByPlayer->toString()}
                                                             : Value{};
                                              },
                                              nullptr};
        all["powered"] = field(&ServerEntity::powered);
        all["swell"] = field(&ServerEntity::swell);
        all["villager_level"] = field(&ServerEntity::villagerLevel);
        all["villager_experience"] = field(&ServerEntity::villagerExperience);
        all["profession"] = Accessor{[](const ServerEntity& entity) -> Value { return entity.profession; },
                                     [](ServerEntity& entity, const Value& value) {
                                         const std::optional<std::string> text = asText(value);
                                         if (!text.has_value()) {
                                             return false;
                                         }
                                         entity.profession = text->substr(text->find(':') + 1);
                                         return true;
                                     }};
        all["cube_size"] =
            Accessor{[](const ServerEntity& entity) -> Value { return static_cast<std::int64_t>(entity.cubeSize); },
                     [](ServerEntity& entity, const Value& value) {
                         if (!entity::cubeMobs::isCube(entity.type())) {
                             return false;
                         }
                         entity::cubeMobs::applySize(
                             entity, static_cast<std::int32_t>(std::clamp<std::int64_t>(
                                         asInt(value, 1), entity::cubeMobs::kMinSize, entity::cubeMobs::kMaxSize)));
                         return true;
                     }};
        all["owner"] = Accessor{[](const ServerEntity& entity) -> Value {
                                    return entity.owner.has_value() ? Value{*entity.owner} : Value{};
                                },
                                [](ServerEntity& entity, const Value& value) {
                                    entity.owner = asText(value);
                                    return true;
                                }};
        return all;
    }();
    return kAll;
}

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
        return std::nullopt;
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
    return found != accessors().end() && found->second.write && found->second.write(entity, value);
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
        const world::ActiveEffect effect{.effect = idOf(asText(at(arguments, 0)).value_or("")),
                                         .amplifier = static_cast<std::int32_t>(asInt(at(arguments, 2))),
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
        return true;
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
    if (operation == "push") {
        entity.velocityX += asReal(at(arguments, 0));
        entity.velocityY += asReal(at(arguments, 1));
        entity.velocityZ += asReal(at(arguments, 2));
        return true;
    }
    return std::nullopt;
}

} // namespace basalt::server::runtime::plugins::entityOps
