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

class VehicleSystem {
public:
    struct Hit {
        float amount = 0.0F;
        wire::common::Identifier type = wire::common::Identifier::vanilla("generic");
        std::optional<std::int32_t> causeId;
        std::optional<std::int32_t> directId;
        bool creativePlayer = false;
        bool burningArrow = false;
        double arrowSpeedSqr = 0.0;
        bool burningProjectile = false;
    };

    struct Hooks {
        std::function<std::optional<float>(const world::LoadedWorld& world, const entity::ServerEntity& vehicle,
                                           const Hit& hit)>
            damageAllowed;
        std::function<bool(const world::LoadedWorld& world, const entity::ServerEntity& vehicle, const Hit& hit)>
            destroyAllowed;
    };

    struct Options {
        std::function<std::int64_t()> nextSeed;
        std::function<bool()> minecartImprovements;
        const world::realItemTags::TagMembers* itemTags = nullptr;
        const world::realBlockTags::TagMembers* blockTags = nullptr;
        const world::realDamageTypeTags::TagMembers* damageTypeTags = nullptr;
        const world::realEntityTypeTags::TagMembers* entityTags = nullptr;
        std::function<std::int32_t(world::LoadedWorld& world, entity::ServerEntity& cart, const std::string& command)>
            runCommand;
        std::function<bool(world::LoadedWorld& world, wire::nbt::NbtCompound& data,
                           const std::array<std::int32_t, 3>& pos,
                           const std::function<void(std::int32_t eventId)>& broadcastEvent)>
            tickSpawner;
    };

    VehicleSystem(player::PlayerRegistry& players, BlockChangeBroadcaster& broadcaster, ExplosionSystem& explosions,
                  EntityPushSystem& pushes, MobActionSystem& mobs, Options options);

    void setHooks(Hooks hooks) { hooks_ = std::move(hooks); }

    /// parity: `AbstractMinecart.tick` and the subclass that owns the cart.
    void tickMinecart(world::LoadedWorld& world, entity::ServerEntity& cart);

    bool hurt(world::LoadedWorld& world, entity::ServerEntity& vehicle, const Hit& hit);

    [[nodiscard]] static bool ignoresExplosion(world::LoadedWorld& world, bool sourceIsMob);

    bool fuel(world::LoadedWorld& world, player::PlayerSession& player, entity::ServerEntity& cart, bool fuelItem);

    void program(world::LoadedWorld& world, entity::ServerEntity& cart, const std::string& command, bool trackOutput);

    void tickBoatBubbles(world::LoadedWorld& world, entity::ServerEntity& boat, bool driverIsPlayer);

    void boatPushes(world::LoadedWorld& world, entity::ServerEntity& boat, bool driverIsPlayer);

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
    Hooks hooks_;
};

} // namespace basalt::server::runtime
