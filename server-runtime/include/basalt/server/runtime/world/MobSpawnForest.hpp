#pragma once

#include <basalt/server/world/ChunkTypes.hpp>

#include <cstdint>
#include <deque>
#include <functional>
#include <optional>
#include <vector>

namespace basalt::server::runtime::world {

/// Why a mob is being spawned, which several finalizers branch on. parity: `EntitySpawnReason`.
enum class MobSpawnReason : std::uint8_t {
    Natural,
    Patrol,
    Event,
    ChunkGeneration,
    Structure,
    Jockey,
    Triggered,
    MobSummoned,
    Reinforcement,
    Spawner,
    TrialSpawner,
};

/// Nodes are owned by the builder and referred to by pointer: a finalizer mounts one on another while both still grow.
class MobSpawnNode {
public:
    MobSpawnNode(wire::common::Identifier type, double x, double y, double z, float yaw, float pitch,
                 wire::nbt::NbtCompound data);

    [[nodiscard]] const wire::common::Identifier& type() const noexcept { return type_; }

    [[nodiscard]] double x() const noexcept { return x_; }

    [[nodiscard]] double y() const noexcept { return y_; }

    [[nodiscard]] double z() const noexcept { return z_; }

    [[nodiscard]] float yaw() const noexcept { return yaw_; }

    [[nodiscard]] float pitch() const noexcept { return pitch_; }

    [[nodiscard]] wire::nbt::NbtCompound& data() noexcept { return data_; }

    [[nodiscard]] const wire::nbt::NbtCompound& data() const noexcept { return data_; }

    [[nodiscard]] MobSpawnNode* vehicle() const noexcept { return vehicle_; }

    [[nodiscard]] const std::vector<MobSpawnNode*>& passengers() const noexcept { return passengers_; }

    [[nodiscard]] const std::optional<std::int32_t>& externalVehicleId() const noexcept { return externalVehicleId_; }

    /// Stops a mob riding something that is already riding it.
    [[nodiscard]] bool hasVehicle(const MobSpawnNode* candidate) const;

private:
    friend class MobSpawnForestBuilder;

    wire::common::Identifier type_;
    double x_;
    double y_;
    double z_;
    float yaw_;
    float pitch_;
    wire::nbt::NbtCompound data_;
    MobSpawnNode* vehicle_ = nullptr;
    std::optional<std::int32_t> externalVehicleId_;
    std::vector<MobSpawnNode*> passengers_;
};

struct MobExternalVehicleLink {
    server::world::ChunkGeneratedEntity passenger;
    std::int32_t vehicleId = 0;
};

struct MobSpawnForest {
    server::world::ChunkGeneratedEntity main;
    std::vector<server::world::ChunkGeneratedEntity> roots;
    std::vector<MobExternalVehicleLink> externalVehicleLinks;
};

/// parity: `VillageSiege` places the mob it asked for and leaves what it is riding alone.
[[nodiscard]] MobSpawnForest relocateMain(const MobSpawnForest& forest, double x, double y, double z, float yaw);

/// Callbacks because most of this runs off the tick thread during chunk generation, with no world to ask.
struct MobSpawnEnvironment {
    std::function<std::optional<std::int32_t>(const MobSpawnNode&)> nearbyUnriddenChicken;
    std::function<bool(const wire::common::Identifier&, double, double, double)> canSpawnEntity;
};

class MobSpawnForestBuilder {
public:
    explicit MobSpawnForestBuilder(MobSpawnEnvironment environment = {});

    [[nodiscard]] const MobSpawnEnvironment& environment() const noexcept { return environment_; }

    MobSpawnNode& create(const wire::common::Identifier& type, double x, double y, double z, float yaw,
                         float pitch = 0.0F, wire::nbt::NbtCompound data = {});

    /// parity: `Entity.startRiding`.
    bool mount(MobSpawnNode& passenger, MobSpawnNode& vehicle);

    bool mountExternal(MobSpawnNode& passenger, std::int32_t vehicleId);

    [[nodiscard]] MobSpawnForest freeze(const MobSpawnNode& main) const;

private:
    MobSpawnEnvironment environment_;
    /// A deque because finalizers hold pointers into this while still adding to it.
    std::deque<MobSpawnNode> nodes_;
};

} // namespace basalt::server::runtime::world
