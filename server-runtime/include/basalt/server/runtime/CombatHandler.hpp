#pragma once

#include <basalt/server/runtime/entity/EntityCombat.hpp>
#include <basalt/server/runtime/entity/MobEquipment.hpp>
#include <basalt/server/runtime/player/PlayerDamage.hpp>
#include <basalt/server/runtime/player/PlayerSpear.hpp>
#include <basalt/server/runtime/player/PlayerVersusPlayer.hpp>
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

class CombatHandler {
public:
    struct Hooks {
        std::function<std::optional<float>(const player::PlayerSession& player, float amount, std::string_view cause,
                                           const std::optional<std::int32_t>& sourceEntityId)>
            damaged;
        std::function<std::optional<float>(const player::PlayerSession& player, float amount, std::string_view cause,
                                           bool fast)>
            healed;
        std::function<bool(const player::PlayerSession& player, bool offhand)> resurrectAllowed;
        std::function<Announcement(const player::PlayerSession& player, const std::string& deathMessage,
                                   player::DeathLoot& loot)>
            died;
        std::function<bool(const player::PlayerSession& player, std::int32_t targetEntityId)> attackAllowed;
        std::function<bool(const player::PlayerSession& player, const entity::ServerEntity& vehicle)> mountAllowed;
        std::function<bool(const player::PlayerSession& player, const entity::ServerEntity& vehicle)> dismountAllowed;
        std::function<bool(const player::PlayerSession& player, const entity::ServerEntity& hanging)>
            hangingBreakAllowed;
        std::function<bool(const player::PlayerSession& player, const entity::ServerEntity& animal)> tameAllowed;
    };

    struct Options {
        std::function<double()> nextDouble;
        std::function<std::int64_t()> nextSeed;
    };

    explicit CombatHandler(ClientSession& owner, Hooks hooks = {}, Options options = {});

    double fallDistance = 0.0;

    /// parity: `ServerPlayer.startingToFallPosition`, where the fall now being counted began.
    std::optional<std::array<double, 3>> fallStart;

    /// parity: `ServerPlayer.resetFallDistance`.
    void resetFall(const player::PlayerSession& session);

    void dismount(player::PlayerSession& session);

    /// parity: `ServerGamePacketListenerImpl.handleInteract`'s attack branch.
    void handleAttack(const wire::protocol::play::AttackPacket& attack);

    void handleInteract(const wire::protocol::play::InteractPacket& interact);

    /// parity: `checkAutoSpinAttack`.
    void spinAttack(player::PlayerSession& session, double toX, double toY, double toZ);

    /// parity: the `STAB` of `handlePlayerAction`: a spear's jab along the look.
    void jab();

    /// parity: `Item.use` for a `kinetic_weapon`.
    bool startCharge(player::PlayerSession& session, std::int32_t slot);

    /// parity: `KineticWeapon.damageEntities`, one tick of the charge.
    void tickCharge(player::PlayerSession& session);

    /// parity: `stopUsingItem`, which forgets whoever the charge struck.
    void stopCharge();

    /// parity: `ServerGamePacketListenerImpl.handleMoveVehicle`.
    void handleMoveVehicle(const wire::protocol::play::ServerboundMoveVehiclePacket& packet);

    /// parity: `handlePaddleBoat`, the oars everyone else sees.
    void handlePaddleBoat(const wire::protocol::play::ServerboundPaddleBoatPacket& packet);

    /// parity: the riding jump and `OPEN_INVENTORY` of `handlePlayerCommand`.
    void handleRidingCommand(wire::protocol::play::PlayerCommandPacket::Action action, std::int32_t data);

    void equipMount(entity::ServerEntity& mount, entity::mobEquipment::Slot slot,
                    const std::optional<server::world::loot::ItemStack>& stack);

    /// parity: `Entity.getControllingPassenger` as it answers this player.
    [[nodiscard]] bool controls(const player::PlayerSession& session, const entity::ServerEntity& vehicle) const;

    [[nodiscard]] float reduced(const player::PlayerSession& session, float amount,
                                const std::optional<wire::common::Identifier>& damageType) const;

    void trackFall(player::PlayerSession& session, double y, bool onGround,
                   const std::optional<std::int32_t>& feetState);

    [[nodiscard]] std::function<std::optional<player::DeathLabel>(std::int32_t)> deathLabels() const;

    void hurt(float amount, std::string_view deathMessageId,
              const std::optional<wire::common::Identifier>& damageType = std::nullopt,
              const std::optional<entity::DamageSource>& source = std::nullopt);

    void heal(float amount, std::string_view cause);

    void killPlayer(std::string_view deathMessageId);

private:
    [[nodiscard]] player::DamageContext damageContext() const;

    void die(std::string_view deathMessageId, const std::optional<wire::text::Component>& killer);

    [[nodiscard]] player::DeathLoot deathLootOf(player::PlayerSession& session) const;

    void spreadDeathLoot(player::PlayerSession& session, const player::DeathLoot& loot) const;

    /// parity: `Entity.restituteMovementAfterCollisions` calling `Block.bounceOn` on landing.
    void bounceOn(player::PlayerSession& session, world::LoadedWorld& world, double landingY);

    void applyFallDamage(player::PlayerSession& session, double landingY);

    [[nodiscard]] std::vector<player::spear::Candidate> spearTargets(const player::PlayerSession& session,
                                                                     world::LoadedWorld& world);

    /// parity: `Player.stabAttack`.
    bool stab(player::PlayerSession& session, world::LoadedWorld& world, std::int32_t targetId, std::int32_t slot,
              float base, bool dealsDamage, bool dealsKnockback, bool dismounts, std::optional<float> strength);

    struct StabBlow {
        float total = 0.0F;
        float magic = 0.0F;
        float extraKnockback = 0.0F;
        bool dealsDamage = false;
        bool dealsKnockback = false;
        bool dismounts = false;
    };

    bool stabPlayer(player::PlayerSession& session, player::PlayerSession& victim, std::int32_t slot,
                    const StabBlow& blow);

    /// parity: `LivingEntity.postPiercingAttack`, which is lunge.
    void postPiercingAttack(player::PlayerSession& session, world::LoadedWorld& world, std::int32_t slot);

    void soundAroundExceptSelf(const player::PlayerSession& session, std::string_view sound);

    /// parity: `Mob.attemptToShearEquipment` with the shears in that hand slot.
    bool shearEquipment(player::PlayerSession& session, entity::ServerEntity& mob, std::int32_t handSlot);

    /// parity: `Player.attack`'s sweep, which hits what is standing beside what was hit.
    void sweepAround(player::PlayerSession& session, const entity::ServerEntity& target, world::LoadedWorld& world,
                     float strength, float baseDamage, const std::optional<storage::StoredItemStack>& weapon);

    [[nodiscard]] player::pvp::Context meleeContext(const player::BreakContext& blocks);

    /// parity: `Player.attack` on another player, who is no entity of the world's.
    void attackPlayer(player::PlayerSession& session, player::PlayerSession& victim, world::LoadedWorld& world);

    /// parity: `TamableAnimal.mobInteract`.
    bool tameOrSit(const world::LoadedWorld& world, player::PlayerSession& session, entity::ServerEntity& entity);

    /// parity: `ItemFrame.interact`.
    bool useItemFrame(player::PlayerSession& session, entity::ServerEntity& frame);

    /// parity: `ItemFrame.hurtServer`.
    void breakFrame(player::PlayerSession& session, entity::ServerEntity& frame);

    /// parity: `Cushion.hurtServer`.
    void breakCushion(player::PlayerSession& session, entity::ServerEntity& cushion);

    void broadcastFrame(const entity::ServerEntity& frame);

    void mount(player::PlayerSession& session, entity::ServerEntity& vehicle);

    /// parity: `getDismountLocationForPassenger`.
    [[nodiscard]] std::optional<std::array<double, 3>> dismountSpot(const player::PlayerSession& session,
                                                                    const entity::ServerEntity& vehicle) const;

    /// parity: each horse kind's `mobInteract`.
    bool interactMount(player::PlayerSession& session, entity::ServerEntity& horse, bool mainHand);

    /// parity: `fedFood` over `handleEating`.
    bool feedMount(player::PlayerSession& session, entity::ServerEntity& horse, const wire::common::Identifier& item);

    void rideMount(player::PlayerSession& session, entity::ServerEntity& horse);

    bool openMountInventory(player::PlayerSession& session, entity::ServerEntity& horse);

    void mountSound(const entity::ServerEntity& mount, std::string_view sound, float volume, float pitch);

    [[nodiscard]] float soundRoll() const;

    /// parity: `AbstractBoat.interact`.
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
    /// parity: `vehiclePositionLastResetAt`.
    std::int64_t vehicleResetAt_ = 0;
    bool spinning_ = false;
    /// parity: `useItemRemaining` and `recentKineticEnemies`.
    std::optional<std::int32_t> chargeSlot_;
    std::int32_t chargeTicks_ = 0;
    std::vector<std::pair<std::int32_t, std::int64_t>> stabbed_;
};

} // namespace basalt::server::runtime
