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

namespace minecarts {

inline constexpr double kMaxSpeed = 0.4;

inline constexpr std::int32_t kTntCartFuse = 80;

inline constexpr float kTntCartPower = 4.0F;
inline constexpr float kTntCartSpeedFactor = 1.0F;
inline constexpr double kTntCartSpeedCap = 5.0;

inline constexpr std::int32_t kFuelPerItem = 3600;
inline constexpr std::int32_t kMaxFuel = 32000;

struct Step {
    std::array<double, 3> position{};
    std::array<double, 3> movement{};
    float yaw = 0.0F;
    float pitch = 0.0F;
    float weight = 0.0F;
};

struct CartState {
    /// Set by a plugin: the speed squared a tnt cart blows up with on its next turn.
    std::optional<double> detonateSpeedSqr;
    bool flipped = false;
    bool onRails = false;
    bool firstTick = true;
    bool inWater = false;
    bool inLava = false;
    bool experimental = false;
    bool horizontalCollision = false;
    bool verticalCollision = false;
    std::vector<Step> lerpSteps;
    std::int64_t lerpStepsTick = -1;
    std::int32_t fuel = 0;
    double pushX = 0.0;
    double pushZ = 0.0;
    std::int32_t lastActivated = 0;
    float explosionPower = kTntCartPower;
    float explosionSpeedFactor = kTntCartSpeedFactor;
    std::optional<std::int32_t> ignitedBy;
    bool ignited = false;
    std::array<double, 3> stuck{};
    bool slowWhenEmpty = true;
    std::optional<double> maxSpeedOverride;
    /// `-1` for the default friction, `0` for none at all, `1` for the vanilla one.
    std::int8_t frictionState = -1;
    std::array<double, 3> flyingVelocityMod{0.95F, 0.95F, 0.95F};
    std::array<double, 3> derailedVelocityMod{0.5, 0.5, 0.5};
};

[[nodiscard]] CartState& stateOf(ServerEntity& cart);

[[nodiscard]] bool isMinecart(const wire::common::Identifier& type);
/// parity: `isRideable`, only the plain minecart.
[[nodiscard]] bool isRideable(const ServerEntity& cart);
[[nodiscard]] bool isFurnace(const ServerEntity& cart);

struct Env {
    bool experimental = false;
    std::int32_t maxMinecartSpeed = 8;
    double legacyMaxSpeed = kMaxSpeed;
    double poweredRailBoost = 0.06;
    std::optional<std::array<double, 3>> riderIntent;
    std::function<void(ServerEntity& cart, const std::array<std::int32_t, 3>& rail, bool powered)> activate;
    /// parity: `pushAndPickupEntities`.
    std::function<bool(ServerEntity& cart)> pushAndPickup;
    std::function<void(ServerEntity& cart)> insideBlocks;
    std::function<void(ServerEntity& cart, double fallDistance)> landed;
};

[[nodiscard]] std::array<std::int32_t, 3> currentBlockPosOrRailBelow(const world::WorldState& world,
                                                                     const ServerEntity& cart, bool experimental);

[[nodiscard]] bool isRail(const world::WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

[[nodiscard]] std::optional<std::array<std::int32_t, 3>> railUnder(const world::WorldState& world,
                                                                   const ServerEntity& cart, bool experimental = false);

[[nodiscard]] std::optional<std::array<double, 3>> railPosition(const world::WorldState& world, double x, double y,
                                                                double z);

[[nodiscard]] double maxSpeed(const ServerEntity& cart, const Env& env);

void tick(world::WorldState& world, ServerEntity& cart, const Env& env);

void adjustToRails(world::WorldState& world, ServerEntity& cart, const std::array<std::int32_t, 3>& rail, bool instant);

void move(world::WorldState& world, ServerEntity& cart, const Env& env, double dx, double dy, double dz);

[[nodiscard]] std::array<double, 3> intentOf(bool forward, bool back, bool left, bool right, float yaw);

void pushOtherMinecart(ServerEntity& self, ServerEntity& other, double xa, double za, bool experimental);

bool addFuel(ServerEntity& cart, double fromX, double fromZ);

void save(const ServerEntity& cart, wire::nbt::NbtCompound& tag);

void load(ServerEntity& cart, const wire::nbt::NbtCompound& tag);

[[nodiscard]] double speedOf(const ServerEntity& cart);

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
[[nodiscard]] bool poweredProperty(const world::WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);
/// parity: `AbstractMinecart.move` under the new behaviour.
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
