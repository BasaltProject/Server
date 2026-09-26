#pragma once

#include <basalt/server/runtime/entity/EntityCombat.hpp>
#include <basalt/server/runtime/entity/MobEquipment.hpp>
#include <basalt/server/runtime/player/PlayerDamage.hpp>
#include <basalt/server/runtime/player/PlayerSpear.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>
#include <basalt/wire/protocol/play/PlayerStatePackets.hpp>
#include <basalt/wire/protocol/play/VehiclePackets.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

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

    /// parity: `checkAutoSpinAttack` for a riptide, on the move the client reports: the first living
    /// thing in the way takes the spin's blow and ends it.
    void spinAttack(player::PlayerSession& session, double toX, double toY, double toZ);

    /// parity: the `STAB` of `handlePlayerAction`: a spear's jab along the look.
    void jab();

    /// parity: `Item.use` for a `kinetic_weapon`: the spear held up in that hand slot to charge.
    bool startCharge(player::PlayerSession& session, std::int32_t slot);

    /// parity: `KineticWeapon.damageEntities`, one tick of the charge.
    void tickCharge(player::PlayerSession& session);

    /// parity: `stopUsingItem`, which forgets whoever the charge struck.
    void stopCharge();

    /// parity: `ServerGamePacketListenerImpl.handleMoveVehicle`: the driver's client moves the vehicle
    /// and the server takes it, unless it went too far at once.
    void handleMoveVehicle(const wire::protocol::play::ServerboundMoveVehiclePacket& packet);

    /// parity: `handlePaddleBoat`, the oars everyone else sees.
    void handlePaddleBoat(const wire::protocol::play::ServerboundPaddleBoatPacket& packet);

    /// parity: the riding jump and `OPEN_INVENTORY` of `handlePlayerCommand`.
    void handleRidingCommand(wire::protocol::play::PlayerCommandPacket::Action action, std::int32_t data);

    /// A mount's saddle or body armour, as a click or its window sets it: seen by everyone, with the
    /// equip sound and game event.
    void equipMount(entity::ServerEntity& mount, entity::mobEquipment::Slot slot,
                    const std::optional<server::world::loot::ItemStack>& stack);

    /// parity: `Entity.getControllingPassenger` as it answers this player.
    [[nodiscard]] bool controls(const player::PlayerSession& session, const entity::ServerEntity& vehicle) const;

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

    /// Whoever a spear may reach from here, mobs and, with pvp on, players.
    [[nodiscard]] std::vector<player::spear::Candidate> spearTargets(const player::PlayerSession& session,
                                                                     world::LoadedWorld& world);

    /// parity: `Player.stabAttack`; `strength` is the swing's charge for a jab and nothing for a charge.
    bool stab(player::PlayerSession& session, world::LoadedWorld& world, std::int32_t targetId, std::int32_t slot,
              float base, bool dealsDamage, bool dealsKnockback, bool dismounts, std::optional<float> strength);

    /// parity: `LivingEntity.postPiercingAttack`, which is lunge.
    void postPiercingAttack(player::PlayerSession& session, world::LoadedWorld& world, std::int32_t slot);

    void soundAroundExceptSelf(const player::PlayerSession& session, std::string_view sound);

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

    /// parity: `getDismountLocationForPassenger`, where a vehicle sets its rider down, or nothing for
    /// the vehicles that keep the old lift.
    [[nodiscard]] std::optional<std::array<double, 3>> dismountSpot(const player::PlayerSession& session,
                                                                    const entity::ServerEntity& vehicle) const;

    /// parity: each horse kind's `mobInteract`. Answers whether the click was the mount's.
    bool interactMount(player::PlayerSession& session, entity::ServerEntity& horse, bool mainHand);

    /// parity: `fedFood` over `handleEating`. Answers whether it ate.
    bool feedMount(player::PlayerSession& session, entity::ServerEntity& horse, const wire::common::Identifier& item);

    void rideMount(player::PlayerSession& session, entity::ServerEntity& horse);

    bool openMountInventory(player::PlayerSession& session, entity::ServerEntity& horse);

    void mountSound(const entity::ServerEntity& mount, std::string_view sound, float volume, float pitch);

    /// `nextFloat() - nextFloat()`, what vanilla spreads a pitch with.
    [[nodiscard]] float soundRoll() const;

    /// parity: `AbstractBoat.interact`. Answers whether the player got in.
    bool boardBoat(player::PlayerSession& session, entity::ServerEntity& boat);

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
    /// parity: `vehiclePositionLastResetAt`, the turn a vehicle this player does not drive was last put back.
    std::int64_t vehicleResetAt_ = 0;
    /// Whether the attack under way is a riptide's, which reaches anywhere and hits for the spin.
    bool spinning_ = false;
    /// The hand slot a spear is being charged from, the ticks of it so far, and who it struck when.
    /// parity: `useItemRemaining` and `recentKineticEnemies`.
    std::optional<std::int32_t> chargeSlot_;
    std::int32_t chargeTicks_ = 0;
    std::vector<std::pair<std::int32_t, std::int64_t>> stabbed_;
};

} // namespace basalt::server::runtime
