#pragma once

#include <basalt/server/runtime/entity/EntityCombat.hpp>
#include <basalt/server/runtime/player/PlayerDamage.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>

namespace basalt::server::runtime::player {
class PlayerSession;
}

namespace basalt::server::runtime::world {
class LoadedWorld;
}

namespace basalt::server::runtime {

class ClientSession;

/// Everything that hurts, heals or kills the player on this connection, plus what they hit.
class CombatHandler {
public:
    /// What a plugin gets a say in. Answering nothing to `damaged` stops the hit entirely.
    struct Hooks {
        std::function<std::optional<float>(const player::PlayerSession& player, float amount, std::string_view cause,
                                           const std::optional<std::int32_t>& sourceEntityId)>
            damaged;
        std::function<std::optional<float>(const player::PlayerSession& player, float amount)> healed;
        std::function<void(const player::PlayerSession& player, const std::string& deathMessage)> died;
        /// Whether this player may hit that entity, for a plugin that runs a protected area.
        std::function<bool(const player::PlayerSession& player, std::int32_t targetEntityId)> attackAllowed;
    };

    /// The rolls a hit makes: the critical roll and the sound seeds.
    struct Options {
        std::function<double()> nextDouble;
        std::function<std::int64_t()> nextSeed;
    };

    explicit CombatHandler(ClientSession& owner, Hooks hooks = {}, Options options = {});

    /// How far this player has fallen so far, which is what a landing is charged for.
    double fallDistance = 0.0;

    /// parity: `ServerPlayer.startingToFallPosition`, where the fall now being counted began.
    std::optional<std::array<double, 3>> fallStart;

    /// parity: `ServerPlayer.resetFallDistance`, which grants `fall_from_height` to a fall survived.
    void resetFall(const player::PlayerSession& session);

    void dismount(player::PlayerSession& session);

    /// parity: `ServerGamePacketListenerImpl.handleInteract`'s attack branch.
    void handleAttack(const wire::protocol::play::AttackPacket& attack);

    void handleInteract(const wire::protocol::play::InteractPacket& interact);

    /// What is left of a hit after armour, resistance, protection and the tags that bypass them.
    [[nodiscard]] float reduced(const player::PlayerSession& session, float amount,
                                const std::optional<wire::common::Identifier>& damageType) const;

    /// Counts the fall, and charges for it on landing.
    void trackFall(player::PlayerSession& session, double y, bool onGround,
                   const std::optional<std::int32_t>& feetState);

    void hurt(float amount, std::string_view deathMessageId,
              const std::optional<wire::common::Identifier>& damageType = std::nullopt,
              const std::optional<entity::DamageSource>& source = std::nullopt);

    void heal(float amount);

    void killPlayer(std::string_view deathMessageId);

private:
    /// What the edition-agnostic damage needs, built from what this connection knows.
    [[nodiscard]] player::DamageContext damageContext() const;

    void die(std::string_view deathMessageId, const std::optional<wire::text::Component>& killer);

    /// parity: `Entity.restituteMovementAfterCollisions` calling `Block.bounceOn` on landing.
    void bounceOn(player::PlayerSession& session, world::LoadedWorld& world, double landingY);

    void applyFallDamage(player::PlayerSession& session, double landingY);

    /// parity: `Player.attack`'s sweep, which hits what is standing beside what was hit.
    void sweepAround(player::PlayerSession& session, const entity::ServerEntity& target, world::LoadedWorld& world,
                     float strength);

    /// parity: `TamableAnimal.mobInteract`: a pet sits or stands, and a wild one may be tamed.
    bool tameOrSit(const world::LoadedWorld& world, player::PlayerSession& session, entity::ServerEntity& entity);

    /// parity: `ItemFrame.interact`, which takes what is in the hand and turns what is already
    /// behind the glass. Answers whether the click was a frame's.
    bool useItemFrame(player::PlayerSession& session, entity::ServerEntity& frame);

    /// parity: `ItemFrame.hurtServer`: a frame with something behind the glass drops what it holds
    /// and stays on the wall, and an empty one comes off.
    void breakFrame(player::PlayerSession& session, entity::ServerEntity& frame);

    /// parity: `Cushion.hurtServer`: any hit takes it off, and a survival player gets it back.
    void breakCushion(player::PlayerSession& session, entity::ServerEntity& cushion);

    /// The item and the turn a frame is showing, sent to everyone who can see it.
    void broadcastFrame(const entity::ServerEntity& frame);

    void mount(player::PlayerSession& session, entity::ServerEntity& vehicle);

    /// parity: `VehicleEntity.hurtServer`, which is damage points rather than health.
    void breakVehicle(player::PlayerSession& session, entity::ServerEntity& vehicle);

    void killEntity(entity::ServerEntity& entity);

    void broadcastEntityEvent(std::int32_t entityId, std::int32_t event);

    void broadcastDamageEvent(const entity::ServerEntity& entity, const wire::common::Identifier& damageType,
                              std::int32_t causeEntityId);

    void broadcastMotion(const entity::ServerEntity& entity);

    void broadcastTameFlags(const entity::ServerEntity& entity);

    void broadcastPassengers(const entity::ServerEntity& vehicle);

    void broadcastAnimation(std::int32_t entityId, std::int32_t animation);

    void broadcastParticle(std::string_view particle, double x, double y, double z, std::int32_t count);

    ClientSession* owner_;
    Hooks hooks_;
    Options options_;
};

} // namespace basalt::server::runtime
