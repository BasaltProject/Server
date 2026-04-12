#pragma once

#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::world {
class WorldState;
}

namespace basalt::server::runtime::entity {

class ServerEntity;

/// A minecart on its rails, or off them.
///
/// parity: `AbstractMinecart` with its two behaviours: `OldMinecartBehavior`, and
/// `NewMinecartBehavior` when the `minecart_improvements` feature is on.
namespace minecarts {

/// parity: `OldMinecartBehavior.getMaxSpeed`, 0.4 on land and half that in water.
inline constexpr double kMaxSpeed = 0.4;
inline constexpr double kMaxSpeedInWater = 0.2;

/// parity: `MinecartTNT.primeFuse`'s own eighty ticks.
inline constexpr std::int32_t kTntCartFuse = 80;

/// parity: `MinecartTNT`'s explosion power base and speed factor defaults.
inline constexpr float kTntCartPower = 4.0F;
inline constexpr float kTntCartSpeedFactor = 1.0F;
inline constexpr double kTntCartSpeedCap = 5.0;

/// parity: `MinecartFurnace.FUEL_TICKS_PER_ITEM` and `MAX_FUEL_TICKS`.
inline constexpr std::int32_t kFuelPerItem = 3600;
inline constexpr std::int32_t kMaxFuel = 32000;

/// parity: `NewMinecartBehavior.MinecartStep`, one point the client lerps the cart through.
struct Step {
    std::array<double, 3> position{};
    std::array<double, 3> movement{};
    float yaw = 0.0F;
    float pitch = 0.0F;
    float weight = 0.0F;
};

/// What a cart keeps between turns. parity: `AbstractMinecart` and its subclasses' fields.
struct CartState {
    bool flipped = false;
    bool onRails = false;
    /// parity: `Entity.firstTick`, saved under `HasTicked`.
    bool firstTick = true;
    bool inWater = false;
    bool inLava = false;
    /// Whether it last moved under `minecart_improvements`, which is how it is sent.
    bool experimental = false;
    bool horizontalCollision = false;
    bool verticalCollision = false;
    /// parity: `NewMinecartBehavior.lerpSteps`, what this turn's movement went through.
    std::vector<Step> lerpSteps;
    std::int64_t lerpStepsTick = -1;
    /// parity: `MinecartFurnace.fuel` and `push`.
    std::int32_t fuel = 0;
    double pushX = 0.0;
    double pushZ = 0.0;
    /// parity: `MinecartCommandBlock.lastActivated`, in the cart's own tick count.
    std::int32_t lastActivated = 0;
    /// parity: `MinecartTNT`'s explosion tuning and who set it off.
    float explosionPower = kTntCartPower;
    float explosionSpeedFactor = kTntCartSpeedFactor;
    std::optional<std::int32_t> ignitedBy;
    bool ignited = false;
    /// parity: `Entity.stuckSpeedMultiplier`.
    std::array<double, 3> stuck{};
};

[[nodiscard]] CartState& stateOf(ServerEntity& cart);

[[nodiscard]] bool isMinecart(const wire::common::Identifier& type);
/// parity: `isRideable`, only the plain minecart.
[[nodiscard]] bool isRideable(const ServerEntity& cart);
[[nodiscard]] bool isFurnace(const ServerEntity& cart);

/// What one turn of a cart needs from outside.
struct Env {
    /// parity: `AbstractMinecart.useExperimentalMovement`, the `minecart_improvements` feature.
    bool experimental = false;
    /// parity: the `max_minecart_speed` game rule.
    std::int32_t maxMinecartSpeed = 8;
    /// parity: `ServerPlayer.getLastClientMoveIntent` of the first passenger, when that is a player.
    std::optional<std::array<double, 3>> riderIntent;
    /// parity: `activateMinecart`, handed the rail and whether it is powered.
    std::function<void(ServerEntity& cart, const std::array<std::int32_t, 3>& rail, bool powered)> activate;
    /// parity: `pushAndPickupEntities`; answers whether a new move should go on after picking up.
    std::function<bool(ServerEntity& cart)> pushAndPickup;
    /// parity: `applyEffectsFromBlocks` for the box where the cart is now.
    std::function<void(ServerEntity& cart)> insideBlocks;
    /// parity: `checkFallDamage`'s landing: `fallOn`, and so `causeFallDamage`, with the distance.
    std::function<void(ServerEntity& cart, double fallDistance)> landed;
};

/// parity: `getCurrentBlockPosOrRailBelow`, which asks a little further down under the new
/// behaviour.
[[nodiscard]] std::array<std::int32_t, 3> currentBlockPosOrRailBelow(const world::WorldState& world,
                                                                     const ServerEntity& cart, bool experimental);

/// Whether a block state is a rail. parity: `BaseRailBlock.isRail`.
[[nodiscard]] bool isRail(const world::WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

/// The rail the cart is on, whichever behaviour: its own square or the one under it.
[[nodiscard]] std::optional<std::array<std::int32_t, 3>> railUnder(const world::WorldState& world,
                                                                   const ServerEntity& cart, bool experimental = false);

/// Exactly where on the rail a point sits, height included, or nothing when there is no rail there.
///
/// parity: `OldMinecartBehavior.getPos`, whose sixteenth of a block of lift and its half-block step
/// on a slope are what the cart actually rides on.
[[nodiscard]] std::optional<std::array<double, 3>> railPosition(const world::WorldState& world, double x, double y,
                                                                double z);

/// parity: `getMaxSpeed`, with the furnace's own share.
[[nodiscard]] double maxSpeed(const ServerEntity& cart, const Env& env);

/// parity: the whole of `MinecartBehavior.tick` on the server.
void tick(world::WorldState& world, ServerEntity& cart, const Env& env);

/// parity: `NewMinecartBehavior.adjustToRails`, which a freshly placed cart runs at once.
void adjustToRails(world::WorldState& world, ServerEntity& cart, const std::array<std::int32_t, 3>& rail, bool instant);

/// parity: `AbstractMinecart.move(SELF, delta)`: the move against the blocks, the collisions, the
/// fall and the block's own speed factor.
void move(world::WorldState& world, ServerEntity& cart, const Env& env, double dx, double dy, double dz);

/// The direction the keys a rider is holding point, in the world.
///
/// parity: `ServerPlayer.getLastClientMoveIntent` over `Entity.getInputVector`.
[[nodiscard]] std::array<double, 3> intentOf(bool forward, bool back, bool left, bool right, float yaw);

/// parity: `AbstractMinecart.pushOtherMinecart`, `self` being the cart that was pushed.
void pushOtherMinecart(ServerEntity& self, ServerEntity& other, double xa, double za, bool experimental);

/// parity: `MinecartFurnace.addFuel`, from where the player stands. Answers whether it took.
bool addFuel(ServerEntity& cart, double fromX, double fromZ);

/// parity: `AbstractMinecart.addAdditionalSaveData` and its subclasses' own fields.
void save(const ServerEntity& cart, wire::nbt::NbtCompound& tag);

/// parity: `AbstractMinecart.readAdditionalSaveData` and its subclasses' own fields.
void load(ServerEntity& cart, const wire::nbt::NbtCompound& tag);

/// How fast the cart is going, for the callers that only care about that.
[[nodiscard]] double speedOf(const ServerEntity& cart);

// Internal to the two behaviours.
namespace detail {

struct Exits {
    std::array<std::int32_t, 3> a;
    std::array<std::int32_t, 3> b;
};

[[nodiscard]] std::optional<std::string_view> shapeAt(const world::WorldState& world, std::int32_t x, std::int32_t y,
                                                      std::int32_t z);
[[nodiscard]] const Exits* exitsOf(std::string_view shape);
[[nodiscard]] bool poweredRail(const world::WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                               bool& isPoweredRail);
[[nodiscard]] bool isActivator(const world::WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);
/// The rail's own `powered` property, for an activator or a powered rail.
[[nodiscard]] bool poweredProperty(const world::WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);
/// parity: `AbstractMinecart.move` under the new behaviour: the push and pick up after the move,
/// and the rest of the way when that says to go on. The old behaviour just moves.
void moveAndPush(world::WorldState& world, ServerEntity& cart, const Env& env, double dx, double dy, double dz);
[[nodiscard]] bool isConductor(const world::WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);
[[nodiscard]] double gravityOf(const ServerEntity& cart);
[[nodiscard]] std::array<double, 3> naturalSlowdown(ServerEntity& cart, const Env& env,
                                                    const std::array<double, 3>& movement);
void comeOffTrack(world::WorldState& world, ServerEntity& cart, const Env& env);
void newTick(world::WorldState& world, ServerEntity& cart, const Env& env);

} // namespace detail

} // namespace minecarts

} // namespace basalt::server::runtime::entity
