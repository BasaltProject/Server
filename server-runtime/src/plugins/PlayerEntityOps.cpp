#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/entity/EntityCombat.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerConnection.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/plugins/PlayerEntityOps.hpp>
#include <basalt/server/runtime/world/Fluids.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/MobEffects.hpp>

#include <algorithm>
#include <cmath>
#include <string>

namespace basalt::server::runtime::plugins {
namespace {

namespace reg = server::registry;

using server::plugins::PluginEntityRef;
using server::plugins::PluginEntityValue;
using wire::common::Identifier;
using Value = PluginEntityValue;

constexpr double kStandingHeight = 1.8;
constexpr double kSneakingHeight = 1.5;
constexpr double kWidth = 0.6;
constexpr std::string_view kMaxHealthId = "minecraft:max_health";

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

[[nodiscard]] const Value& at(const std::vector<Value>& arguments, std::size_t index) {
    static const Value kNone;
    return index < arguments.size() ? arguments[index] : kNone;
}

[[nodiscard]] Identifier idOf(std::string_view text) {
    const std::string full =
        text.find(':') == std::string_view::npos ? "minecraft:" + std::string{text} : std::string{text};
    const auto parsed = Identifier::parse(full);
    return parsed.has_value() ? *parsed : Identifier::vanilla("generic");
}

class PlayerEntityRef final : public PluginEntityRef {
public:
    PlayerEntityRef(world::LoadedWorld& world, std::int32_t entityId, SessionList sessions)
        : world_(world.keepAlive()), entityId_(entityId), sessions_(std::move(sessions)) {}

    [[nodiscard]] std::int32_t id() const override { return entityId_; }

    [[nodiscard]] std::string type() const override { return "minecraft:player"; }

    [[nodiscard]] std::string world() const override { return world_->name(); }

    [[nodiscard]] double x() const override {
        const player::PlayerSession* player = session();
        return player == nullptr ? 0.0 : player->x;
    }

    [[nodiscard]] double y() const override {
        const player::PlayerSession* player = session();
        return player == nullptr ? 0.0 : player->y;
    }

    [[nodiscard]] double z() const override {
        const player::PlayerSession* player = session();
        return player == nullptr ? 0.0 : player->z;
    }

    [[nodiscard]] double health() const override {
        const player::PlayerSession* player = session();
        return player == nullptr ? 0.0 : static_cast<double>(player->health());
    }

    void setHealth(double value) override {
        player::PlayerSession* player = session();
        if (player == nullptr) {
            return;
        }
        player->setHealth(std::clamp(static_cast<float>(value), 0.0F, player->maxHealth()));
        if (player->connection().syncHealth) {
            player->connection().syncHealth();
        }
    }

    [[nodiscard]] bool alive() const override { return session() != nullptr; }

    void remove() override {}

    bool teleport(double x, double y, double z) override {
        player::PlayerSession* player = session();
        if (player == nullptr || !player->connection().teleportTo) {
            return false;
        }
        player->connection().teleportTo(x, y, z, player->yaw, player->pitch);
        return true;
    }

    [[nodiscard]] std::string uuid() const override {
        const player::PlayerSession* player = session();
        return player == nullptr ? std::string{} : player->uuid().toString();
    }

    [[nodiscard]] std::array<float, 2> rotation() const override {
        const player::PlayerSession* player = session();
        return player == nullptr ? std::array<float, 2>{0.0F, 0.0F} : std::array<float, 2>{player->yaw, player->pitch};
    }

    bool setRotation(float yaw, float pitch) override {
        player::PlayerSession* player = session();
        if (player == nullptr || !player->connection().teleportTo) {
            return false;
        }
        player->connection().teleportTo(player->x, player->y, player->z, yaw, pitch);
        return true;
    }

    bool setVelocity(double x, double y, double z) override {
        player::PlayerSession* player = session();
        if (player == nullptr || !player->connection().pushBack) {
            return false;
        }
        player->connection().pushBack(x, y, z, player->yaw);
        return true;
    }

    [[nodiscard]] bool onGround() const override {
        const player::PlayerSession* player = session();
        return player != nullptr && player->onGround;
    }

    [[nodiscard]] double width() const override { return kWidth; }

    [[nodiscard]] double height() const override {
        const player::PlayerSession* player = session();
        return player != nullptr && player->sneaking ? kSneakingHeight : kStandingHeight;
    }

    [[nodiscard]] std::optional<std::string> customName() const override {
        const player::PlayerSession* player = session();
        return player == nullptr ? std::nullopt : std::optional{player->name()};
    }

    [[nodiscard]] std::optional<Value> property(std::string_view name) const override {
        const player::PlayerSession* player = session();
        if (player == nullptr) {
            return std::nullopt;
        }
        if (name == "health") {
            return static_cast<double>(player->health());
        }
        if (name == "max_health") {
            return static_cast<double>(player->maxHealth());
        }
        if (name == "absorption") {
            return static_cast<double>(player->absorption);
        }
        if (name == "fire_ticks") {
            return static_cast<std::int64_t>(player->fireTicks);
        }
        if (name == "air") {
            return static_cast<std::int64_t>(player->airSupply);
        }
        if (name == "frozen_ticks") {
            return static_cast<std::int64_t>(player->ticksFrozen);
        }
        if (name == "max_air") {
            return static_cast<std::int64_t>(world::fluids::kMaxAir);
        }
        if (name == "no_damage_ticks") {
            return static_cast<std::int64_t>(player->invulnerableTicks);
        }
        if (name == "invulnerable") {
            return player->invulnerable();
        }
        if (name == "vehicle") {
            return static_cast<std::int64_t>(player->vehicleId.value_or(-1));
        }
        if (name == "passengers") {
            std::vector<std::string> riders;
            for (const entity::ServerEntity* other : world_->state().entities()) {
                if (!other->removed && other->vehicleId == entityId_) {
                    riders.push_back(std::to_string(other->entityId()));
                }
            }
            return riders;
        }
        if (name == "custom_name") {
            return player->name();
        }
        if (name == "glowing") {
            return player->effects.contains(Identifier::vanilla("glowing"));
        }
        if (name == "invisible") {
            return player->effects.contains(Identifier::vanilla("invisibility"));
        }
        if (name == "persistent") {
            return true;
        }
        if (name == "no_gravity" || name == "silent" || name == "custom_name_visible" || name == "no_ai" ||
            name == "sleeping" || name == "can_pick_up_loot" || name == "left_handed") {
            return false;
        }
        if (name == "target" || name == "leash_holder") {
            return static_cast<std::int64_t>(-1);
        }
        if (name == "tags") {
            return std::vector<std::string>{};
        }
        return std::nullopt;
    }

    bool setProperty(std::string_view name, const Value& value) override {
        player::PlayerSession* player = session();
        if (player == nullptr) {
            return false;
        }
        if (name == "health") {
            setHealth(asReal(value, static_cast<double>(player->health())));
            return true;
        }
        if (name == "max_health") {
            player->attributeBases.insert_or_assign(std::string{kMaxHealthId}, asReal(value, 20.0));
            player->setMaxHealth(static_cast<float>(std::max(asReal(value, 20.0), 1.0)));
            if (player->connection().syncHealth) {
                player->connection().syncHealth();
            }
            return true;
        }
        if (name == "absorption") {
            player->absorption = static_cast<float>(std::max(0.0, asReal(value)));
            if (player->connection().syncAbsorption) {
                player->connection().syncAbsorption();
            }
            return true;
        }
        if (name == "fire_ticks") {
            player->fireTicks = static_cast<std::int32_t>(std::max<std::int64_t>(0, asInt(value)));
            return true;
        }
        if (name == "air") {
            player->airSupply = static_cast<std::int32_t>(asInt(value));
            return true;
        }
        if (name == "frozen_ticks") {
            player->ticksFrozen = static_cast<std::int32_t>(std::max<std::int64_t>(0, asInt(value)));
            return true;
        }
        if (name == "no_damage_ticks") {
            player->invulnerableTicks = static_cast<std::int32_t>(asInt(value));
            return true;
        }
        return false;
    }

    std::optional<Value> invoke(std::string_view operation, const std::vector<Value>& arguments) override {
        player::PlayerSession* player = session();
        if (player == nullptr) {
            return std::nullopt;
        }
        if (operation == "damage") {
            if (!player->connection().hurt) {
                return false;
            }
            const std::string cause = asText(at(arguments, 1)).value_or("generic");
            std::optional<entity::DamageSource> source;
            if (const std::int64_t attacker = asInt(at(arguments, 2), -1); attacker >= 0) {
                if (const entity::ServerEntity* by = world_->state().entity(static_cast<std::int32_t>(attacker));
                    by != nullptr) {
                    source = entity::DamageSource{.entityId = by->entityId(), .x = by->x, .z = by->z};
                }
            }
            // parity: `DamageTypes.MOB_ATTACK` and `PLAYER_ATTACK` name their death messages `mob` and `player`.
            const std::string messageId = cause == "mob_attack" ? "mob" : cause == "player_attack" ? "player" : cause;
            player->connection().hurt(static_cast<float>(asReal(at(arguments, 0))), messageId, idOf(cause), source);
            return true;
        }
        if (operation == "kill") {
            if (!player->connection().killPlayer || player->dead) {
                return false;
            }
            player->connection().killPlayer("generic");
            return true;
        }
        if (operation == "add_effect") {
            const world::ActiveEffect effect{.effect = idOf(asText(at(arguments, 0)).value_or("")),
                                             .amplifier = static_cast<std::int32_t>(asInt(at(arguments, 2))),
                                             .remainingTicks = static_cast<std::int32_t>(asInt(at(arguments, 1))),
                                             .ambient = std::get_if<bool>(&at(arguments, 3)) != nullptr &&
                                                        std::get<bool>(at(arguments, 3)),
                                             .visible = true,
                                             .showIcon = true};
            if (!world::mobEffects::protocolIdOf(effect.effect).has_value()) {
                return false;
            }
            if (effect.effect == world::mobEffects::instantHealth() && player->connection().heal) {
                player->connection().heal(4.0F * static_cast<float>(1 << std::clamp(effect.amplifier, 0, 30)), "magic");
                return true;
            }
            if (effect.effect == world::mobEffects::instantDamage() && player->connection().hurt) {
                player->connection().hurt(6.0F * static_cast<float>(1 << std::clamp(effect.amplifier, 0, 30)), "magic",
                                          Identifier::vanilla("magic"), std::nullopt);
                return true;
            }
            player->effects.insert_or_assign(effect.effect, effect);
            if (player->connection().sendEffect) {
                player->connection().sendEffect(effect);
            }
            return true;
        }
        if (operation == "remove_effect") {
            const Identifier id = idOf(asText(at(arguments, 0)).value_or(""));
            if (player->effects.erase(id) == 0) {
                return false;
            }
            if (player->connection().sendEffectRemoved) {
                player->connection().sendEffectRemoved(id);
            }
            return true;
        }
        if (operation == "clear_effects") {
            std::vector<Identifier> ids;
            for (const auto& [id, active] : player->effects) {
                ids.push_back(id);
            }
            player->effects.clear();
            for (const Identifier& id : ids) {
                if (player->connection().sendEffectRemoved) {
                    player->connection().sendEffectRemoved(id);
                }
            }
            return true;
        }
        if (operation == "has_effect") {
            return player->effects.contains(idOf(asText(at(arguments, 0)).value_or("")));
        }
        if (operation == "effects") {
            std::vector<std::string> rows;
            for (const auto& [id, active] : player->effects) {
                rows.push_back(id.toString());
                rows.push_back(std::to_string(active.remainingTicks));
                rows.push_back(std::to_string(active.amplifier));
                rows.push_back(active.ambient ? "1" : "0");
                rows.push_back(active.visible ? "1" : "0");
                rows.push_back(active.showIcon ? "1" : "0");
            }
            return rows;
        }
        if (operation == "has_attribute") {
            const std::string text = asText(at(arguments, 0)).value_or("");
            return reg::attributeIds::exists(text.substr(text.find(':') + 1));
        }
        if (operation == "attribute" || operation == "attribute_base" || operation == "attribute_default") {
            const std::string text = asText(at(arguments, 0)).value_or("");
            const std::string full = text.find(':') == std::string::npos ? "minecraft:" + text : text;
            const auto found = player->attributeBases.find(full);
            if (operation != "attribute_default" && found != player->attributeBases.end()) {
                return found->second;
            }
            return full == kMaxHealthId ? 20.0 : entity::attribute::defaultFor(full).value_or(0.0);
        }
        if (operation == "set_attribute_base") {
            const std::string text = asText(at(arguments, 0)).value_or("");
            const std::string full = text.find(':') == std::string::npos ? "minecraft:" + text : text;
            if (full == kMaxHealthId) {
                return setProperty("max_health", at(arguments, 1));
            }
            player->attributeBases.insert_or_assign(full, asReal(at(arguments, 1)));
            return true;
        }
        if (operation == "attribute_modifiers") {
            return std::vector<std::string>{};
        }
        if (operation == "knockback") {
            if (!player->connection().pushBack) {
                return false;
            }
            const double x = asReal(at(arguments, 1));
            const double z = asReal(at(arguments, 2));
            const double length = std::sqrt((x * x) + (z * z));
            if (length == 0.0) {
                return false;
            }
            const double strength = asReal(at(arguments, 0));
            player->connection().pushBack(-x / length * strength, 0.4, -z / length * strength, player->yaw);
            return true;
        }
        if (operation == "push") {
            if (!player->connection().pushBack) {
                return false;
            }
            player->connection().pushBack(asReal(at(arguments, 0)), asReal(at(arguments, 1)), asReal(at(arguments, 2)),
                                          player->yaw);
            return true;
        }
        if (operation == "mount") {
            entity::ServerEntity* vehicle =
                world_->state().entity(static_cast<std::int32_t>(asInt(at(arguments, 0), -1)));
            if (vehicle == nullptr || vehicle->removed || player->vehicleId == vehicle->entityId()) {
                return false;
            }
            if (player->vehicleId.has_value() && player->connection().dismount) {
                player->connection().dismount();
            }
            player->vehicleId = vehicle->entityId();
            vehicle->passengerIds.push_back(entityId_);
            for (player::PlayerSession* watcher : sessions_()) {
                if (watcher->worldName == world_->name() && watcher->connection().sendPassengers) {
                    watcher->connection().sendPassengers(vehicle->entityId(), vehicle->passengerIds);
                }
            }
            return true;
        }
        if (operation == "dismount") {
            if (!player->vehicleId.has_value() || !player->connection().dismount) {
                return false;
            }
            player->connection().dismount();
            return true;
        }
        if (operation == "eject") {
            bool any = false;
            for (entity::ServerEntity* other : world_->state().entities()) {
                if (!other->removed && other->vehicleId == entityId_) {
                    other->vehicleId.reset();
                    other->boardingCooldown = 60;
                    any = true;
                }
            }
            return any;
        }
        return std::nullopt;
    }

private:
    [[nodiscard]] player::PlayerSession* session() const {
        for (player::PlayerSession* candidate : sessions_()) {
            if (candidate->entityId() == entityId_ && candidate->worldName == world_->name()) {
                return candidate;
            }
        }
        return nullptr;
    }

    std::shared_ptr<world::LoadedWorld> world_;
    std::int32_t entityId_;
    SessionList sessions_;
};

} // namespace

std::shared_ptr<PluginEntityRef> playerEntityRef(world::LoadedWorld& world, std::int32_t entityId,
                                                 SessionList sessions) {
    return std::make_shared<PlayerEntityRef>(world, entityId, std::move(sessions));
}

} // namespace basalt::server::runtime::plugins
