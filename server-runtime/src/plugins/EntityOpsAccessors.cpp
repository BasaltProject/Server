#include "EntityOpsInternal.hpp"

namespace basalt::server::runtime::plugins::entityOps::internal {
namespace {
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
} // namespace

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
        all["max_air"] = Accessor{
            [](const ServerEntity& entity) -> Value {
                return static_cast<std::int64_t>(entity.maxAirOverride.value_or(
                    entity.type().path() == "dolphin" ? ServerEntity::kDolphinMaxAir : ServerEntity::kDefaultMaxAir));
            },
            [](ServerEntity& entity, const Value& value) {
                entity.maxAirOverride = static_cast<std::int32_t>(asInt(value, 300));
                return true;
            }};
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
                                              // parity: `Vindicator.setCustomName` turns a Johnny on for good.
                                              if (*text == "Johnny" && entity.type().path() == "vindicator") {
                                                  source(entity).putBoolean("Johnny", true);
                                              }
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
        all["display_text"] = Accessor{[](const ServerEntity& entity) -> Value {
                                           return entity.displayText.has_value() ? Value{*entity.displayText} : Value{};
                                       },
                                       [](ServerEntity& entity, const Value& value) {
                                           entity.displayText = asText(value);
                                           return true;
                                       }};
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
        all["last_hurt_by_player"] = Accessor{
            [](const ServerEntity& entity) -> Value {
                return entity.lastHurtByPlayer.has_value() ? Value{entity.lastHurtByPlayer->toString()} : Value{};
            },
            [](ServerEntity& entity, const Value& value) {
                const std::optional<std::string> text = asText(value);
                const auto parsed = text.has_value() ? wire::common::Uuid::parse(*text) : std::nullopt;
                entity.lastHurtByPlayer = parsed;
                entity.lastHurtByPlayerTime = parsed.has_value() ? entity::entityCombat::kPlayerHitMemoryTicks : 0;
                return true;
            }};
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
        all["bounce"] = field(&ServerEntity::bounces);
        all["incendiary"] =
            Accessor{[](const ServerEntity& entity) -> Value {
                         const std::string_view path = entity.type().path();
                         return entity.incendiary.value_or(path == "fireball" || path == "small_fireball");
                     },
                     [](ServerEntity& entity, const Value& value) {
                         entity.incendiary = asBool(value);
                         return true;
                     }};
        all["arrow_knockback"] =
            Accessor{[](const ServerEntity& entity) -> Value {
                         if (entity.arrowKnockback.has_value()) {
                             return static_cast<std::int64_t>(*entity.arrowKnockback);
                         }
                         const NbtCompound* levels =
                             entity.firedFromWeaponComponents.getCompound("minecraft:enchantments");
                         return static_cast<std::int64_t>(levels == nullptr ? 0 : levels->getInt("minecraft:punch"));
                     },
                     [](ServerEntity& entity, const Value& value) {
                         entity.arrowKnockback = static_cast<std::int32_t>(std::max<std::int64_t>(0, asInt(value)));
                         return true;
                     }};
        all["fired_from"] = Accessor{
            [](const ServerEntity& entity) -> Value {
                return entity.firedFromWeapon.has_value() ? Value{entity.firedFromWeapon->toString()} : Value{};
            },
            [](ServerEntity& entity, const Value& value) {
                const std::optional<std::string> text = asText(value);
                entity.firedFromWeapon.reset();
                if (text.has_value() && !text->empty()) {
                    if (const auto parsed = wire::common::Identifier::parse(*text); parsed.has_value()) {
                        entity.firedFromWeapon = *parsed;
                    }
                }
                return true;
            }};
        all["bolt_cause"] = Accessor{[](const ServerEntity& entity) -> Value { return idOrNone(entity.boltCause); },
                                     [](ServerEntity& entity, const Value& value) {
                                         const std::int64_t id = asInt(value, -1);
                                         entity.boltCause =
                                             id < 0 ? std::nullopt : std::optional{static_cast<std::int32_t>(id)};
                                         return true;
                                     }};
        all["arrow_life"] = field(&ServerEntity::arrowLife);
        all["wither_charged"] = field(&ServerEntity::dangerousSkull);
        all["trident_glint"] = field(&ServerEntity::tridentFoil);
        all["attached_to"] = Accessor{[](const ServerEntity& entity) -> Value {
                                          return entity.firework == nullptr ? static_cast<std::int64_t>(-1)
                                                                            : idOrNone(entity.firework->attachedTo);
                                      },
                                      [](ServerEntity& entity, const Value& value) {
                                          if (!entity::fireworkRockets::isRocket(entity)) {
                                              return false;
                                          }
                                          if (entity.firework == nullptr) {
                                              entity.firework =
                                                  std::make_shared<entity::fireworkRockets::RocketState>();
                                              entity.firework->item = entity::fireworkRockets::plainRocket();
                                          }
                                          const std::int64_t id = asInt(value, -1);
                                          entity.firework->attachedTo =
                                              id < 0 ? std::nullopt : std::optional{static_cast<std::int32_t>(id)};
                                          return true;
                                      }};
        all["eye_despawn"] =
            Accessor{[](const ServerEntity& entity) -> Value {
                         return static_cast<std::int64_t>(entity.enderEye == nullptr ? 0 : entity.enderEye->life);
                     },
                     [](ServerEntity& entity, const Value& value) {
                         if (!world::enderEyes::isEye(entity)) {
                             return false;
                         }
                         if (entity.enderEye == nullptr) {
                             entity.enderEye = std::make_shared<entity::enderEyes::EyeState>();
                         }
                         entity.enderEye->life = static_cast<std::int32_t>(asInt(value));
                         return true;
                     }};
        all["eye_drops"] = Accessor{[](const ServerEntity& entity) -> Value {
                                        return entity.enderEye != nullptr && entity.enderEye->surviveAfterDeath;
                                    },
                                    [](ServerEntity& entity, const Value& value) {
                                        if (!world::enderEyes::isEye(entity)) {
                                            return false;
                                        }
                                        if (entity.enderEye == nullptr) {
                                            entity.enderEye = std::make_shared<entity::enderEyes::EyeState>();
                                        }
                                        entity.enderEye->surviveAfterDeath = asBool(value);
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

} // namespace basalt::server::runtime::plugins::entityOps::internal
