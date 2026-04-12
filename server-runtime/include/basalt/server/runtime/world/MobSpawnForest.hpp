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

/// One mob while the tree of vehicles and passengers is still being assembled.
///
/// Nodes are owned by the builder and referred to by pointer: a finalizer mounts one on another
/// while both are still growing, which a value tree cannot express without copying half of it.
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

    /// The NBT this mob is being written into, which every finalizer adds to.
    [[nodiscard]] wire::nbt::NbtCompound& data() noexcept { return data_; }

    [[nodiscard]] const wire::nbt::NbtCompound& data() const noexcept { return data_; }

    [[nodiscard]] MobSpawnNode* vehicle() const noexcept { return vehicle_; }

    [[nodiscard]] const std::vector<MobSpawnNode*>& passengers() const noexcept { return passengers_; }

    [[nodiscard]] const std::optional<std::int32_t>& externalVehicleId() const noexcept { return externalVehicleId_; }

    /// Whether `candidate` is this node's vehicle, or its vehicle's, and so on: what stops a mob
    /// riding something that is already riding it.
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

/// A passenger whose vehicle is an entity that already exists, rather than one being spawned here.
struct MobExternalVehicleLink {
    server::world::ChunkGeneratedEntity passenger;
    std::int32_t vehicleId = 0;
};

/// The finished tree: the mob that was asked for, every root of the forest, and the mounts onto
/// entities outside it.
struct MobSpawnForest {
    server::world::ChunkGeneratedEntity main;
    std::vector<server::world::ChunkGeneratedEntity> roots;
    std::vector<MobExternalVehicleLink> externalVehicleLinks;
};

/// Moves only the requested entity after finalization, keeping vehicle positions.
/// parity: `VillageSiege`, which places the mob it asked for and leaves what it is riding alone.
[[nodiscard]] MobSpawnForest relocateMain(const MobSpawnForest& forest, double x, double y, double z, float yaw);

/// What a finalizer may ask about the world it is spawning into.
///
/// Callbacks rather than the world itself: most of this runs off the tick thread while a chunk is
/// generating, where there is no world to ask.
struct MobSpawnEnvironment {
    /// The entity id of a chicken nobody is riding near this node, for a chicken jockey.
    std::function<std::optional<std::int32_t>(const MobSpawnNode&)> nearbyUnriddenChicken;
    /// Whether this type fits where it is about to go.
    std::function<bool(const wire::common::Identifier&, double, double, double)> canSpawnEntity;
};

/// Mutable vehicle and passenger topology assembled while concrete mob finalizers recurse.
class MobSpawnForestBuilder {
public:
    explicit MobSpawnForestBuilder(MobSpawnEnvironment environment = {});

    [[nodiscard]] const MobSpawnEnvironment& environment() const noexcept { return environment_; }

    MobSpawnNode& create(const wire::common::Identifier& type, double x, double y, double z, float yaw,
                         float pitch = 0.0F, wire::nbt::NbtCompound data = {});

    /// parity: `Entity.startRiding`, including forced reparenting after recursive finalization.
    bool mount(MobSpawnNode& passenger, MobSpawnNode& vehicle);

    /// The same, onto an entity that is not part of this forest.
    bool mountExternal(MobSpawnNode& passenger, std::int32_t vehicleId);

    [[nodiscard]] MobSpawnForest freeze(const MobSpawnNode& main) const;

private:
    MobSpawnEnvironment environment_;
    /// A deque because finalizers hold pointers into this while still adding to it.
    std::deque<MobSpawnNode> nodes_;
};

} // namespace basalt::server::runtime::world
