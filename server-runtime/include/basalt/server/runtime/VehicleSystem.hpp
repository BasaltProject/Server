#pragma once

#include <basalt/server/runtime/world/RealBlockTags.hpp>
#include <basalt/server/runtime/world/RealDamageTypeTags.hpp>
#include <basalt/server/runtime/world/RealEntityTypeTags.hpp>
#include <basalt/server/runtime/world/RealItemTags.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace basalt::server::random {
class RandomSource;
}

namespace basalt::server::runtime::entity {
class ServerEntity;
}

namespace basalt::server::runtime::player {
class PlayerRegistry;
class PlayerSession;
} // namespace basalt::server::runtime::player

namespace basalt::server::runtime::world {
class LoadedWorld;
}

namespace basalt::server::runtime {

class BlockChangeBroadcaster;
class EntityPushSystem;
class ExplosionSystem;
class MobActionSystem;

/// Minecarts and boats: a cart's turn, the damage both take and how they break, and a player's hand on a cart.
/// parity: `VehicleEntity`, `AbstractMinecart` and its subclasses, `AbstractBoat`'s pushes and bubble columns.
class VehicleSystem {
public:
    /// One hit on a vehicle, as `hurtServer` sees its `DamageSource`.
    struct Hit {
        float amount = 0.0F;
        wire::common::Identifier type = wire::common::Identifier::vanilla("generic");
        /// parity: `getEntity` and `getDirectEntity`.
        std::optional<std::int32_t> causeId;
        std::optional<std::int32_t> directId;
        /// A player in creative, which takes a vehicle away whole.
        bool creativePlayer = false;
        /// An arrow on fire, and how fast it went, which sets a tnt cart off at once.
        bool burningArrow = false;
        double arrowSpeedSqr = 0.0;
        /// A projectile on fire, which `damageSourceIgnitesTnt` counts as fire.
        bool burningProjectile = false;
    };

    struct Options {
        std::function<std::int64_t()> nextSeed;
        /// parity: `AbstractMinecart.useExperimentalMovement`.
        std::function<bool()> minecartImprovements;
        const world::realItemTags::TagMembers* itemTags = nullptr;
        const world::realBlockTags::TagMembers* blockTags = nullptr;
        const world::realDamageTypeTags::TagMembers* damageTypeTags = nullptr;
        const world::realEntityTypeTags::TagMembers* entityTags = nullptr;
        /// parity: `BaseCommandBlock.performCommand` for a command block cart: the line run from the
        /// cart, answering how many times it succeeded.
        std::function<std::int32_t(world::LoadedWorld& world, entity::ServerEntity& cart, const std::string& command)>
            runCommand;
        /// parity: `BaseSpawner.serverTick` at the spawner cart's square, over the cart's saved data;
        /// `broadcastEvent` is the entity event it raises. Unset means no spawner cart ever spawns.
        std::function<bool(world::LoadedWorld& world, wire::nbt::NbtCompound& data,
                           const std::array<std::int32_t, 3>& pos,
                           const std::function<void(std::int32_t eventId)>& broadcastEvent)>
            tickSpawner;
    };

    VehicleSystem(player::PlayerRegistry& players, BlockChangeBroadcaster& broadcaster, ExplosionSystem& explosions,
                  EntityPushSystem& pushes, MobActionSystem& mobs, Options options);

    /// parity: `AbstractMinecart.tick` and the subclass that owns the cart.
    void tickMinecart(world::LoadedWorld& world, entity::ServerEntity& cart);

    /// parity: `VehicleEntity.hurtServer` with `MinecartTNT`'s override. Answers whether it counted.
    bool hurt(world::LoadedWorld& world, entity::ServerEntity& vehicle, const Hit& hit);

    /// parity: `VehicleEntity.ignoreExplosion`, a mob's blast with griefing off.
    [[nodiscard]] static bool ignoresExplosion(world::LoadedWorld& world, bool sourceIsMob);

    /// parity: `MinecartFurnace.interact`: the fuel, from where the player stands. Answers whether
    /// one of the stack in that hand slot is spent.
    bool fuel(world::LoadedWorld& world, player::PlayerSession& player, entity::ServerEntity& cart, bool fuelItem);

    /// parity: `ServerGamePacketListenerImpl.handleSetCommandMinecart` once allowed.
    void program(world::LoadedWorld& world, entity::ServerEntity& cart, const std::string& command, bool trackOutput);

    /// parity: `AbstractBoat.onAboveBubbleColumn` and `tickBubbleColumn`, a turn of the boat over one.
    void tickBoatBubbles(world::LoadedWorld& world, entity::ServerEntity& boat, bool driverIsPlayer);

    /// parity: `AbstractBoat.tick`'s last part: what it touches is taken aboard or pushed.
    void boatPushes(world::LoadedWorld& world, entity::ServerEntity& boat, bool driverIsPlayer);

    /// parity: `Entity.isInWater`, as `updateFluidHeightAndDoFluidPushing` finds it for a box.
    [[nodiscard]] static bool touchesFluid(world::LoadedWorld& world, double x, double y, double z, double width,
                                           double height, bool lava);

private:
    [[nodiscard]] std::vector<player::PlayerSession*> watchersOf(const world::LoadedWorld& world) const;
    [[nodiscard]] bool typeIn(const wire::common::Identifier& damageType, std::string_view tag) const;
    [[nodiscard]] bool ignitesTnt(const Hit& hit) const;

    void activate(world::LoadedWorld& world, entity::ServerEntity& cart, const std::array<std::int32_t, 3>& rail,
                  bool powered);
    bool pushAndPickup(world::LoadedWorld& world, entity::ServerEntity& cart, bool experimental);
    void insideBlocks(world::LoadedWorld& world, entity::ServerEntity& cart);
    void landed(world::LoadedWorld& world, entity::ServerEntity& cart, double fallDistance);
    void destroy(world::LoadedWorld& world, entity::ServerEntity& vehicle, const Hit& hit);
    void primeTnt(world::LoadedWorld& world, entity::ServerEntity& cart, std::optional<std::int32_t> cause);
    void explodeTnt(world::LoadedWorld& world, entity::ServerEntity& cart, double speedSqr);
    void ejectPassengers(world::LoadedWorld& world, entity::ServerEntity& vehicle);
    bool board(world::LoadedWorld& world, entity::ServerEntity& vehicle, entity::ServerEntity& mob);
    void entityEvent(world::LoadedWorld& world, const entity::ServerEntity& entity, std::int8_t event);
    void sendPassengers(world::LoadedWorld& world, const entity::ServerEntity& vehicle);

    player::PlayerRegistry* players_;
    BlockChangeBroadcaster* broadcaster_;
    ExplosionSystem* explosions_;
    EntityPushSystem* pushes_;
    MobActionSystem* mobs_;
    Options options_;
};

} // namespace basalt::server::runtime
