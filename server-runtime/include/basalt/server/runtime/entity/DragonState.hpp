#pragma once

#include <basalt/server/random/RandomSource.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace basalt::server::runtime::entity::enderDragon {

/// parity: `EnderDragonPhase`, in registration order, which is the id the client is sent.
enum class Phase : std::int32_t {
    HoldingPattern = 0,
    StrafePlayer = 1,
    LandingApproach = 2,
    Landing = 3,
    Takeoff = 4,
    SittingFlaming = 5,
    SittingScanning = 6,
    SittingAttacking = 7,
    ChargingPlayer = 8,
    Dying = 9,
    Hover = 10,
};

/// parity: `EnderDragonPhase.getById`, which falls back to the holding pattern.
[[nodiscard]] Phase phaseById(std::int32_t id) noexcept;

/// parity: `DragonPhaseInstance.isSitting`: the three sitting phases, and hovering.
[[nodiscard]] bool isSitting(Phase phase) noexcept;

struct Vec3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

/// parity: `DragonFlightHistory`.
class FlightHistory {
public:
    struct Sample {
        double y = 0.0;
        float yRot = 0.0F;
    };

    static constexpr std::int32_t kLength = 64;

    void record(double y, float yRot);

    [[nodiscard]] const Sample& get(std::int32_t delay) const;

private:
    std::array<Sample, kLength> samples_{};
    std::int32_t head_ = -1;
};

/// parity: `pathfinder.Node`.
struct Node {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;
    std::int32_t hash = 0;
    float f = 0.0F;
    float g = 0.0F;
    float h = 0.0F;
    std::int32_t cameFrom = -1;
    bool closed = false;
    std::int32_t heapIdx = -1;
};

/// parity: `Path`, as the dragon uses it: a list of block positions walked one at a time.
struct Path {
    std::vector<std::array<std::int32_t, 3>> nodes;
    std::size_t next = 0;

    [[nodiscard]] bool isDone() const noexcept { return next >= nodes.size(); }

    void advance() noexcept { ++next; }

    [[nodiscard]] const std::array<std::int32_t, 3>& nextNode() const { return nodes[next]; }
};

struct PhaseMemory {
    std::optional<Path> holdingPath;
    std::optional<Vec3> holdingTarget;
    bool holdingClockwise = false;
    std::int32_t fireballCharge = 0;
    std::optional<Path> strafePath;
    std::optional<Vec3> strafeTarget;
    std::optional<std::int32_t> strafeVictim;
    bool strafeClockwise = false;
    std::optional<Path> approachPath;
    std::optional<Vec3> approachTarget;
    std::optional<Vec3> landingTarget;
    bool takeoffFirstTick = false;
    std::optional<Path> takeoffPath;
    std::optional<Vec3> takeoffTarget;
    std::int32_t flameTicks = 0;
    std::int32_t flameCount = 0;
    std::optional<std::int32_t> flameCloud;
    std::int32_t scanningTime = 0;
    std::int32_t attackingTicks = 0;
    std::optional<Vec3> chargeTarget;
    std::int32_t timeSinceCharge = 0;
    std::optional<Vec3> dyingTarget;
    std::int32_t dyingTime = 0;
    std::optional<Vec3> hoverTarget;
};

inline constexpr std::size_t kNodeCount = 24;

enum class Part : std::uint8_t { Head, Neck, Body, Tail1, Tail2, Tail3, Wing1, Wing2 };

inline constexpr std::size_t kParts = 8;

struct PartBox {
    float width = 0.0F;
    float height = 0.0F;
};

/// parity: the sizes `EnderDragon` gives its parts.
inline constexpr std::array<PartBox, kParts> kPartBoxes{
    {{1.0F, 1.0F}, {3.0F, 3.0F}, {5.0F, 3.0F}, {2.0F, 2.0F}, {2.0F, 2.0F}, {2.0F, 2.0F}, {4.0F, 2.0F}, {4.0F, 2.0F}}};

struct DragonState {
    /// parity: `Entity.random`, the dragon's own, seeded from nothing a player can know.
    random::RandomSource random = random::RandomSource::legacy(0);
    Phase phase = Phase::Hover;
    PhaseMemory memory;
    FlightHistory history;
    float yRotA = 0.0F;
    bool inWall = false;
    std::int32_t deathTime = 0;
    float sittingDamageReceived = 0.0F;
    std::optional<std::int32_t> nearestCrystal;
    std::array<Vec3, kParts> parts{};
    std::array<std::int32_t, 3> fightOrigin{0, 0, 0};
    bool nodesReady = false;
    std::array<Node, kNodeCount> nodes{};
    std::array<std::int32_t, kNodeCount> adjacency{};
    std::optional<Part> hitPart;
    std::vector<std::int32_t> staleClouds;
};

} // namespace basalt::server::runtime::entity::enderDragon
