#include <basalt/server/runtime/player/PlayerSession.hpp>

#include <algorithm>
#include <cstdint>
#include <utility>

namespace basalt::server::runtime::player {

PlayerSession::PlayerSession(wire::common::Uuid uuid, std::string name, std::int32_t entityId,
                             PlayerConnection connection, wire::protocol::play::Gamemode gamemode)
    : gamemode(gamemode), inventory(connection.enchantmentIds), uuid_(uuid), name_(std::move(name)),
      entityId_(entityId), connection_(std::move(connection)) {}

void PlayerSession::setMaxHealth(float value) {
    maxHealth_ = std::max(value, 1.0F);
    if (health_ > maxHealth_) {
        health_ = maxHealth_;
    }
}

float PlayerSession::luck() const {
    // parity: `Attributes.LUCK`, base 0 and clamped to 1024 either way.
    constexpr double kLuckLimit = 1024.0;
    const auto base = attributeBases.find("minecraft:luck");
    double value = base == attributeBases.end() ? 0.0 : base->second;
    for (const auto& [id, effect] : effects) {
        if (id.namespaceName() == "minecraft" && (id.path() == "luck" || id.path() == "unluck")) {
            value += (id.path() == "luck" ? 1.0 : -1.0) * static_cast<double>(effect.amplifier + 1);
        }
    }
    return static_cast<float>(std::clamp(value, -kLuckLimit, kLuckLimit));
}

bool PlayerSession::mayFly() const noexcept {
    return gamemode == wire::protocol::play::Gamemode::Creative ||
           gamemode == wire::protocol::play::Gamemode::Spectator;
}

bool PlayerSession::invulnerable() const noexcept { return mayFly(); }

bool PlayerSession::instabuild() const noexcept { return gamemode == wire::protocol::play::Gamemode::Creative; }

void PlayerSession::sendSystemMessage(const wire::text::Component& message) const {
    if (connection_.sendSystemMessage) {
        connection_.sendSystemMessage(message);
    }
}

void PlayerSession::resetForRespawn() {
    health_ = maxHealth_;
    food.reset();
    experience.reset();
    dead = false;
    flying = gamemode == wire::protocol::play::Gamemode::Spectator;
    sprinting = false;
    vehicleId.reset();
    // parity: `ServerPlayer.restoreFrom` on a respawn, which starts them breathing and unlit.
    airSupply = world::fluids::kMaxAir;
    fireTicks = 0;
    effects.clear();
}

} // namespace basalt::server::runtime::player
