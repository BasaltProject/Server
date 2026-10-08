#include "MobStateTable.hpp"

#include <basalt/server/runtime/BossBarView.hpp>
#include <basalt/server/runtime/MobActionSystem.hpp>
#include <basalt/server/runtime/entity/Allays.hpp>
#include <basalt/server/runtime/entity/EnderDragon.hpp>
#include <basalt/server/runtime/entity/MobMisc.hpp>
#include <basalt/server/runtime/entity/Piglins.hpp>
#include <basalt/server/runtime/entity/WitherBoss.hpp>
#include <basalt/server/runtime/entity/villager/VillagerBrain.hpp>
#include <basalt/server/runtime/plugins/ServerObjectModel.hpp>
#include <basalt/server/runtime/world/BlockInventory.hpp>
#include <basalt/server/runtime/world/Fluids.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/Precipitation.hpp>
#include <basalt/wire/common/json/Json.hpp>
#include <basalt/wire/nbt/NbtIO.hpp>
#include <basalt/wire/text/ComponentJson.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <vector>

namespace basalt::server::runtime::plugins::entityOps::mobState {
namespace {

using entity::ServerEntity;
using server::world::loot::ItemStack;
using wire::nbt::NbtCompound;
using wire::nbt::NbtTag;
using namespace values;
namespace misc = entity::mobMisc;

using Gate = bool (*)(const ServerEntity&);

[[nodiscard]] MobActionSystem* mobSystem(const Environment& environment) {
    return environment.hooks != nullptr && environment.hooks->mobSystem ? environment.hooks->mobSystem() : nullptr;
}

[[nodiscard]] bool is(const ServerEntity& entity, std::string_view path) { return entity.type().path() == path; }

[[nodiscard]] bool isCreeper(const ServerEntity& entity) { return is(entity, "creeper"); }

[[nodiscard]] bool isEvoker(const ServerEntity& entity) { return is(entity, "evoker"); }

[[nodiscard]] bool isPolarBear(const ServerEntity& entity) { return is(entity, "polar_bear"); }

[[nodiscard]] bool isStrider(const ServerEntity& entity) { return is(entity, "strider"); }

[[nodiscard]] bool isTurtle(const ServerEntity& entity) { return is(entity, "turtle"); }

[[nodiscard]] bool isParrot(const ServerEntity& entity) { return is(entity, "parrot"); }

[[nodiscard]] bool isWither(const ServerEntity& entity) { return is(entity, "wither"); }

[[nodiscard]] bool isWitch(const ServerEntity& entity) { return is(entity, "witch"); }

[[nodiscard]] bool isSteerable(const ServerEntity& entity) { return is(entity, "pig") || is(entity, "strider"); }

[[nodiscard]] bool isCube(const ServerEntity& entity) {
    return is(entity, "slime") || is(entity, "magma_cube") || is(entity, "sulfur_cube");
}

template <typename T>
[[nodiscard]] Accessor miscField(T misc::State::* member, Gate gate) {
    return Accessor{[member](const ServerEntity& entity) -> Value {
                        static const misc::State kDefault;
                        const misc::State* held = misc::peek(entity);
                        return toValue((held == nullptr ? kDefault : *held).*member);
                    },
                    [member, gate](ServerEntity& entity, const Value& value) {
                        if (!gate(entity)) {
                            return false;
                        }
                        fromValue(misc::of(entity).*member, value);
                        return true;
                    }};
}

[[nodiscard]] Accessor miscId(std::optional<std::int32_t> misc::State::* member, Gate gate) {
    return Accessor{[member](const ServerEntity& entity) -> Value {
                        const misc::State* held = misc::peek(entity);
                        return static_cast<std::int64_t>(held == nullptr ? -1 : (held->*member).value_or(-1));
                    },
                    [member, gate](ServerEntity& entity, const Value& value) {
                        if (!gate(entity)) {
                            return false;
                        }
                        const std::int64_t id = asInt(value, -1);
                        misc::of(entity).*member = id < 0 ? std::nullopt : std::optional{static_cast<std::int32_t>(id)};
                        return true;
                    }};
}

[[nodiscard]] Value positionText(const std::array<std::int32_t, 3>& at) {
    return std::to_string(at[0]) + "," + std::to_string(at[1]) + "," + std::to_string(at[2]);
}

[[nodiscard]] std::vector<std::uint8_t> nbtBytes(NbtCompound compound) {
    return wire::nbt::nbtIO::write(NbtTag::ofCompound(std::move(compound)));
}

[[nodiscard]] Value stackValue(const std::optional<ItemStack>& stack) {
    if (!stack.has_value() || stack->count <= 0) {
        return Value{};
    }
    const std::vector<std::uint8_t> bytes = nbtBytes(world::blockInventory::writeStack(*stack));
    return std::string{bytes.begin(), bytes.end()};
}

[[nodiscard]] std::optional<ItemStack> stackOf(const Value& value) {
    const auto* text = std::get_if<std::string>(&value);
    if (text == nullptr || text->empty()) {
        return std::nullopt;
    }
    const std::vector<std::uint8_t> bytes{text->begin(), text->end()};
    const auto parsed = wire::nbt::nbtIO::read(bytes);
    const NbtCompound* compound = parsed.has_value() ? parsed->tag.get<NbtCompound>() : nullptr;
    return compound == nullptr ? std::nullopt : world::blockInventory::readStack(*compound);
}

/// The stack slots a mob keeps in its own state: `kind` names whose, and nothing answers a slot out of range.
[[nodiscard]] std::optional<ItemStack>* heldSlot(ServerEntity& entity, std::string_view kind, std::int64_t slot) {
    if (slot < 0) {
        return nullptr;
    }
    const auto index = static_cast<std::size_t>(slot);
    if (kind == "piglin" && entity::piglins::isPiglin(entity.type())) {
        auto& inventory = entity::piglins::stateOf(entity).inventory;
        return index < inventory.size() ? &inventory[index] : nullptr;
    }
    if (kind == "villager" && is(entity, "villager")) {
        auto& inventory = entity::villager::villagerBrain::stateOf(entity).inventory;
        return index < inventory.size() ? &inventory[index] : nullptr;
    }
    if (kind == "allay" && entity::allays::isAllay(entity.type())) {
        return index == 0 ? &entity::allays::stateOf(entity).inventory : nullptr;
    }
    return nullptr;
}

void addFlags(Table& table) {
    table["creeper_ignited"] = miscField(&misc::State::creeperIgnited, isCreeper);
    table["creeper_igniter"] = miscId(&misc::State::igniter, isCreeper);
    table["evoker_wololo"] = miscId(&misc::State::wololoTarget, isEvoker);
    table["polar_standing"] = miscField(&misc::State::polarStanding, isPolarBear);
    table["strider_shivering"] = miscField(&misc::State::striderCold, isStrider);
    table["turtle_laying_egg"] = miscField(&misc::State::turtleLaying, isTurtle);
    table["turtle_going_home"] = miscField(&misc::State::turtleGoingHome, isTurtle);
    table["parrot_dancing"] = miscField(&misc::State::partyParrot, isParrot);
    table["wither_portals"] = miscField(&misc::State::canUsePortals, isWither);
    table["cube_wander"] = miscField(&misc::State::cubeWanders, isCube);
    table["guardian_moving"] =
        Accessor{[](const ServerEntity& entity) -> Value {
                     const double speed = (entity.velocityX * entity.velocityX) +
                                          (entity.velocityY * entity.velocityY) + (entity.velocityZ * entity.velocityZ);
                     return (is(entity, "guardian") || is(entity, "elder_guardian")) && speed > 1.0E-4;
                 },
                 nullptr};
    table["boost_total"] = miscField(&misc::State::boostTotal, isSteerable);
    table["boost_time"] = miscField(&misc::State::boostTime, isSteerable);
    table["boost_active"] = Accessor{[](const ServerEntity& entity) -> Value {
                                         const misc::State* held = misc::peek(entity);
                                         return held != nullptr && held->boosting;
                                     },
                                     nullptr};
    table["witch_drinking"] = Accessor{[](const ServerEntity& entity) -> Value {
                                           const misc::State* held = misc::peek(entity);
                                           return held != nullptr && held->witchDrinking;
                                       },
                                       nullptr};
    table["witch_use_ticks"] = miscField(&misc::State::witchDrinkTicks, isWitch);
    table["bat_target"] =
        Accessor{[](const ServerEntity& entity) -> Value {
                     return entity.flight.batTarget.has_value() ? positionText(*entity.flight.batTarget) : Value{};
                 },
                 [](ServerEntity& entity, const Value& value) {
                     if (!is(entity, "bat")) {
                         return false;
                     }
                     const std::optional<std::string> text = asText(value);
                     if (!text.has_value() || text->empty()) {
                         entity.flight.batTarget.reset();
                         return true;
                     }
                     std::array<std::int32_t, 3> at{};
                     std::size_t from = 0;
                     for (std::int32_t& axis : at) {
                         const std::size_t end = text->find(',', from);
                         axis = std::atoi(text->substr(from, end == std::string::npos ? end : end - from).c_str());
                         from = end == std::string::npos ? text->size() : end + 1;
                     }
                     entity.flight.batTarget = at;
                     return true;
                 }};
}

void addPiglin(Operations& operations) {
    operations["piglin_items"] = [](const Environment&, ServerEntity& entity,
                                    const Arguments& arguments) -> std::optional<Value> {
        std::vector<std::string> names;
        if (entity::piglins::isPiglin(entity.type())) {
            const entity::piglins::PiglinState& state = entity::piglins::stateOf(entity);
            const auto& from =
                asText(at(arguments, 0)).value_or("") == "barter" ? state.barterItems : state.interestItems;
            names.assign(from.begin(), from.end());
        }
        return names;
    };
    operations["piglin_item_edit"] = [](const Environment&, ServerEntity& entity,
                                        const Arguments& arguments) -> std::optional<Value> {
        if (!entity::piglins::isPiglin(entity.type())) {
            return false;
        }
        entity::piglins::PiglinState& state = entity::piglins::stateOf(entity);
        auto& into = asText(at(arguments, 0)).value_or("") == "barter" ? state.barterItems : state.interestItems;
        const std::string item = asText(at(arguments, 1)).value_or("");
        if (item.empty()) {
            return false;
        }
        return asBool(at(arguments, 2)) ? into.insert(item).second : into.erase(item) > 0;
    };
    operations["piglin_dance"] = [](const Environment& environment, ServerEntity& entity,
                                    const Arguments& arguments) -> std::optional<Value> {
        if (!entity::piglins::isPiglin(entity.type()) || environment.world == nullptr) {
            return false;
        }
        entity::piglins::PiglinState& state = entity::piglins::stateOf(entity);
        // parity: `CraftPiglin.setDancing`, the dance and the spot it dances on together.
        if (!asBool(at(arguments, 0))) {
            state.dancing = false;
            state.celebrateLocation.erase();
            return true;
        }
        const entity::piglins::Pos spot{static_cast<std::int32_t>(std::floor(entity.x)),
                                        static_cast<std::int32_t>(std::floor(entity.y - 0.2)),
                                        static_cast<std::int32_t>(std::floor(entity.z))};
        const std::int64_t ticks = asInt(at(arguments, 1), -1);
        if (ticks < 0) {
            state.celebrateLocation.set(spot);
        } else {
            state.celebrateLocation.setFor(spot, environment.world->gameTime(), ticks);
        }
        state.dancing = true;
        return true;
    };
}

void addHeld(Operations& operations) {
    operations["held_get"] = [](const Environment&, ServerEntity& entity,
                                const Arguments& arguments) -> std::optional<Value> {
        const std::optional<ItemStack>* slot =
            heldSlot(entity, asText(at(arguments, 0)).value_or(""), asInt(at(arguments, 1), -1));
        return slot == nullptr ? Value{} : stackValue(*slot);
    };
    operations["held_set"] = [](const Environment&, ServerEntity& entity,
                                const Arguments& arguments) -> std::optional<Value> {
        std::optional<ItemStack>* slot =
            heldSlot(entity, asText(at(arguments, 0)).value_or(""), asInt(at(arguments, 1), -1));
        if (slot == nullptr) {
            return false;
        }
        *slot = stackOf(at(arguments, 2));
        return true;
    };
}

void addActions(Operations& operations) {
    operations["retype"] = [](const Environment& environment, ServerEntity& entity,
                              const Arguments& arguments) -> std::optional<Value> {
        MobActionSystem* system = mobSystem(environment);
        const std::string name = asText(at(arguments, 0)).value_or("");
        const auto id =
            wire::common::Identifier::parse(name.find(':') == std::string::npos ? "minecraft:" + name : name);
        if (system == nullptr || !id.has_value()) {
            return static_cast<std::int64_t>(-1);
        }
        return static_cast<std::int64_t>(system->pluginRetype(entity, *id));
    };
    operations["boost_start"] = [](const Environment&, ServerEntity& entity,
                                   const Arguments& arguments) -> std::optional<Value> {
        if (!isSteerable(entity)) {
            return false;
        }
        // parity: `ItemBasedSteering.boost`, which starts from nothing.
        misc::State& held = misc::of(entity);
        held.boosting = true;
        held.boostTime = 0;
        held.boostTotal = static_cast<std::int32_t>(std::max<std::int64_t>(asInt(at(arguments, 0)), 0));
        return true;
    };
    operations["school_follow"] = [](const Environment& environment, ServerEntity& entity,
                                     const Arguments& arguments) -> std::optional<Value> {
        ServerEntity* leader =
            environment.world == nullptr
                ? nullptr
                : environment.world->state().entity(static_cast<std::int32_t>(asInt(at(arguments, 0), -1)));
        if (leader == nullptr || leader == &entity || !misc::isSchooling(entity) || !misc::isSchooling(*leader)) {
            return false;
        }
        const misc::Lookup lookup = [&environment](std::int32_t id) { return environment.world->state().entity(id); };
        misc::stopFollowing(entity, lookup);
        misc::startFollowing(entity, *leader);
        return true;
    };
    operations["school_stop"] = [](const Environment& environment, ServerEntity& entity,
                                   const Arguments&) -> std::optional<Value> {
        if (environment.world == nullptr || !misc::isSchooling(entity)) {
            return false;
        }
        misc::stopFollowing(entity, [&environment](std::int32_t id) { return environment.world->state().entity(id); });
        return true;
    };
    operations["school_info"] = [](const Environment& environment, ServerEntity& entity,
                                   const Arguments&) -> std::optional<Value> {
        std::vector<std::string> facts;
        if (!misc::isSchooling(entity)) {
            return facts;
        }
        const misc::State* held = misc::peek(entity);
        const bool follower = environment.world != nullptr && misc::isFollower(entity, [&environment](std::int32_t id) {
                                  return environment.world->state().entity(id);
                              });
        facts.push_back(
            follower && held != nullptr && held->schoolLeader.has_value() ? std::to_string(*held->schoolLeader) : "-1");
        facts.push_back(std::to_string(held == nullptr ? 1 : held->schoolSize));
        facts.push_back(std::to_string(misc::maxSchoolSize(entity)));
        return facts;
    };
    operations["wolf_wet"] = [](const Environment& environment, ServerEntity& entity,
                                const Arguments&) -> std::optional<Value> {
        if (environment.world == nullptr || !is(entity, "wolf")) {
            return false;
        }
        // parity: `Entity.isInWaterOrRain`.
        const auto x = static_cast<std::int32_t>(std::floor(entity.x));
        const auto y = static_cast<std::int32_t>(std::floor(entity.y));
        const auto z = static_cast<std::int32_t>(std::floor(entity.z));
        const std::optional<std::int32_t> state = environment.world->state().blockState(x, y, z);
        return (state.has_value() && world::fluids::of(*state) == world::Submersion::Water) ||
               world::precipitation::entityInRain(*environment.world, entity.x, entity.y, entity.z, entity.height());
    };
    operations["shear_ready"] = [](const Environment& environment, ServerEntity& entity,
                                   const Arguments&) -> std::optional<Value> {
        MobActionSystem* system = mobSystem(environment);
        return system != nullptr && system->readyForShearing(entity);
    };
    operations["shear"] = [](const Environment& environment, ServerEntity& entity,
                             const Arguments& arguments) -> std::optional<Value> {
        MobActionSystem* system = mobSystem(environment);
        const auto source =
            static_cast<wire::protocol::play::SoundSource>(std::clamp<std::int64_t>(asInt(at(arguments, 0), 7), 0, 10));
        return system != nullptr && system->shearMob(entity, source);
    };
    operations["goat_ram"] = [](const Environment& environment, ServerEntity& entity,
                                const Arguments& arguments) -> std::optional<Value> {
        MobActionSystem* system = mobSystem(environment);
        return system != nullptr &&
               system->pluginGoatRam(entity, static_cast<std::int32_t>(asInt(at(arguments, 0), -1)));
    };
    operations["sniffer_set_state"] = [](const Environment& environment, ServerEntity& entity,
                                         const Arguments& arguments) -> std::optional<Value> {
        MobActionSystem* system = mobSystem(environment);
        return system != nullptr &&
               system->pluginSnifferState(entity, static_cast<std::int32_t>(asInt(at(arguments, 0), -1)));
    };
    operations["sniffer_can_dig"] = [](const Environment& environment, ServerEntity& entity,
                                       const Arguments&) -> std::optional<Value> {
        MobActionSystem* system = mobSystem(environment);
        return system != nullptr && system->pluginSnifferCanDig(entity);
    };
    operations["sniffer_dig_spot"] = [](const Environment& environment, ServerEntity& entity,
                                        const Arguments&) -> std::optional<Value> {
        MobActionSystem* system = mobSystem(environment);
        const std::optional<std::array<std::int32_t, 3>> spot =
            system == nullptr ? std::nullopt : system->pluginSnifferDigSpot(entity);
        return spot.has_value() ? positionText(*spot) : Value{};
    };
    operations["sulfur_swallow"] = [](const Environment& environment, ServerEntity& entity,
                                      const Arguments& arguments) -> std::optional<Value> {
        MobActionSystem* system = mobSystem(environment);
        const std::optional<ItemStack> item = stackOf(at(arguments, 0));
        return system != nullptr && item.has_value() && system->pluginSulfurSwallow(entity, *item);
    };
    operations["sulfur_can_explode"] = [](const Environment& environment, ServerEntity& entity,
                                          const Arguments&) -> std::optional<Value> {
        MobActionSystem* system = mobSystem(environment);
        return system != nullptr && system->pluginSulfurCanExplode(entity);
    };
    operations["sulfur_prime"] = [](const Environment& environment, ServerEntity& entity,
                                    const Arguments& arguments) -> std::optional<Value> {
        MobActionSystem* system = mobSystem(environment);
        return system != nullptr && system->pluginSulfurPrime(entity, asBool(at(arguments, 0)));
    };
    operations["witch_drink"] = [](const Environment& environment, ServerEntity& entity,
                                   const Arguments& arguments) -> std::optional<Value> {
        MobActionSystem* system = mobSystem(environment);
        return system != nullptr && system->pluginWitchDrink(entity, stackOf(at(arguments, 0)));
    };
}

constexpr std::array<std::string_view, 7> kBarColors{"pink", "blue", "red", "green", "yellow", "purple", "white"};
constexpr std::array<std::string_view, 5> kBarOverlays{"progress", "notched_6", "notched_10", "notched_12",
                                                       "notched_20"};

[[nodiscard]] std::string titleJson(const wire::text::Component& title) {
    return wire::common::writeJson(wire::text::componentJson::serialize(title));
}

[[nodiscard]] wire::text::Component titleOf(const std::string& json) {
    const auto parsed = wire::common::parseJson(json);
    return parsed.has_value() ? wire::text::componentJson::deserialize(*parsed) : wire::text::Component::text(json);
}

template <typename Use>
[[nodiscard]] bool withBar(const Environment& environment, const ServerEntity& entity, Use&& use) {
    return environment.hooks != nullptr && environment.hooks->entityBar &&
           environment.hooks->entityBar(entity.entityId(), std::forward<Use>(use));
}

void addBars(Operations& operations) {
    operations["bar_info"] = [](const Environment& environment, ServerEntity& entity,
                                const Arguments&) -> std::optional<Value> {
        std::vector<std::string> facts;
        static_cast<void>(withBar(environment, entity, [&facts](BossBarView& view, player::PlayerRegistry&) {
            const auto& style = view.style();
            facts = {titleJson(style.name),
                     std::string{kBarColors[static_cast<std::size_t>(style.color)]},
                     std::string{kBarOverlays[static_cast<std::size_t>(style.overlay)]},
                     style.darkenScreen ? "1" : "0",
                     style.playMusic ? "1" : "0",
                     style.createWorldFog ? "1" : "0",
                     std::to_string(view.progress()),
                     view.visible() ? "1" : "0"};
        }));
        return facts;
    };
    operations["bar_set"] = [](const Environment& environment, ServerEntity& entity,
                               const Arguments& arguments) -> std::optional<Value> {
        const std::string field = asText(at(arguments, 0)).value_or("");
        const std::string text = asText(at(arguments, 1)).value_or("");
        return withBar(environment, entity, [&](BossBarView& view, player::PlayerRegistry& registry) {
            const auto style = view.style();
            if (field == "title") {
                view.setName(registry, titleOf(text));
            } else if (field == "color" || field == "overlay") {
                auto color = style.color;
                auto overlay = style.overlay;
                for (std::size_t index = 0; index < kBarColors.size() && field == "color"; ++index) {
                    if (kBarColors[index] == text) {
                        color = static_cast<wire::protocol::play::BossBarColor>(index);
                    }
                }
                for (std::size_t index = 0; index < kBarOverlays.size() && field == "overlay"; ++index) {
                    if (kBarOverlays[index] == text) {
                        overlay = static_cast<wire::protocol::play::BossBarOverlay>(index);
                    }
                }
                view.setLook(registry, color, overlay);
            } else if (field == "darken") {
                view.setFlags(registry, text == "1", style.playMusic, style.createWorldFog);
            } else if (field == "music") {
                view.setFlags(registry, style.darkenScreen, text == "1", style.createWorldFog);
            } else if (field == "fog") {
                view.setFlags(registry, style.darkenScreen, style.playMusic, text == "1");
            } else if (field == "progress") {
                view.setProgress(registry, static_cast<float>(std::atof(text.c_str())));
            } else if (field == "visible") {
                view.setVisible(registry, text == "1");
            }
        });
    };
    operations["bar_players"] = [](const Environment& environment, ServerEntity& entity,
                                   const Arguments&) -> std::optional<Value> {
        std::vector<std::string> ids;
        static_cast<void>(withBar(environment, entity, [&ids](BossBarView& view, player::PlayerRegistry&) {
            for (const std::int32_t shown : view.players()) {
                ids.push_back(std::to_string(shown));
            }
        }));
        return ids;
    };
    operations["bar_pin"] = [](const Environment& environment, ServerEntity& entity,
                               const Arguments& arguments) -> std::optional<Value> {
        const auto player = static_cast<std::int32_t>(asInt(at(arguments, 0), -1));
        const bool shown = asBool(at(arguments, 1));
        return withBar(environment, entity,
                       [&](BossBarView& view, player::PlayerRegistry& registry) { view.pin(registry, player, shown); });
    };
}

void addBosses(Operations& operations) {
    operations["dragon_parts"] = [](const Environment&, ServerEntity& entity,
                                    const Arguments&) -> std::optional<Value> {
        std::vector<std::string> boxes;
        if (entity.dragon == nullptr && entity::enderDragon::isDragon(entity.type())) {
            entity.dragon = std::make_shared<entity::enderDragon::DragonState>(entity::enderDragon::freshState(entity));
        }
        if (entity.dragon != nullptr) {
            for (std::size_t part = 0; part < static_cast<std::size_t>(entity::enderDragon::kPartCount); ++part) {
                const std::array<double, 6> box =
                    entity::enderDragon::partBox(*entity.dragon, static_cast<entity::enderDragon::Part>(part));
                std::string row;
                for (const double axis : box) {
                    row += (row.empty() ? "" : ",") + std::to_string(axis);
                }
                boxes.push_back(std::move(row));
            }
        }
        return boxes;
    };
    operations["wither_target"] = [](const Environment&, ServerEntity& entity,
                                     const Arguments& arguments) -> std::optional<Value> {
        const std::int64_t head = asInt(at(arguments, 0), -1);
        if (entity.wither == nullptr || head < 0 || head > 2) {
            return static_cast<std::int64_t>(-1);
        }
        const std::int32_t id = entity.wither->targets[static_cast<std::size_t>(head)];
        return static_cast<std::int64_t>(id == 0 ? -1 : id);
    };
    operations["wither_set_target"] = [](const Environment&, ServerEntity& entity,
                                         const Arguments& arguments) -> std::optional<Value> {
        const std::int64_t head = asInt(at(arguments, 0), -1);
        if (!isWither(entity) || head < 0 || head > 2) {
            return false;
        }
        if (entity.wither == nullptr) {
            entity.wither = std::make_shared<entity::witherBoss::WitherState>();
        }
        const std::int64_t id = asInt(at(arguments, 1), -1);
        entity.wither->targets[static_cast<std::size_t>(head)] = id < 0 ? 0 : static_cast<std::int32_t>(id);
        if (head == 0) {
            entity.targetId = id < 0 ? std::nullopt : std::optional{static_cast<std::int32_t>(id)};
        }
        return true;
    };
}

} // namespace

void addMobMore(Table& table, Operations& operations) {
    addFlags(table);
    addPiglin(operations);
    addHeld(operations);
    addActions(operations);
    addBosses(operations);
    addBars(operations);
}

} // namespace basalt::server::runtime::plugins::entityOps::mobState
