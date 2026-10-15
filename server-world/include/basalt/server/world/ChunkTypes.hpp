#pragma once

#include <basalt/server/world/ChunkHeight.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>
#include <optional>
#include <vector>

namespace basalt::server::world {

enum class GeneratedBlockWritePolicy : std::uint8_t {
    Exact,
    Tree,
    OreBaseStone,
    OreStone,
    OreDeepslate,
    Geode,
};

/// How far a chunk got through generation. parity: `ChunkStatus`, reduced to the three stages a chunk
enum class ChunkStage : std::uint8_t { Full, Terrain, Decorated };

class ChunkBlockEntity {
public:
    ChunkBlockEntity(std::int32_t localX, std::int32_t y, std::int32_t localZ, const wire::common::Identifier& type,
                     wire::nbt::NbtCompound data);

    [[nodiscard]] std::int32_t localX() const noexcept { return localX_; }

    [[nodiscard]] std::int32_t y() const noexcept { return y_; }

    [[nodiscard]] std::int32_t localZ() const noexcept { return localZ_; }

    [[nodiscard]] const wire::common::Identifier& type() const noexcept { return type_; }

    [[nodiscard]] const wire::nbt::NbtCompound& data() const noexcept { return data_; }

    [[nodiscard]] wire::nbt::NbtCompound& data() noexcept { return data_; }

    [[nodiscard]] ChunkBlockEntity withData(wire::nbt::NbtCompound data) const;

    friend bool operator==(const ChunkBlockEntity&, const ChunkBlockEntity&) = default;

private:
    std::int32_t localX_;
    std::int32_t y_;
    std::int32_t localZ_;
    wire::common::Identifier type_;
    wire::nbt::NbtCompound data_;
};

/// parity: `SavedTick`.
struct ChunkScheduledTick {
    wire::common::Identifier type;
    std::int32_t localX = 0;
    std::int32_t y = 0;
    std::int32_t localZ = 0;
    std::int32_t delay = 0;
    /// parity: `TickPriority`, which runs from -3 to 3.
    std::int32_t priority = 0;
    /// parity: `ScheduledTick.subTickOrder`.
    std::int64_t order = 0;

    friend bool operator==(const ChunkScheduledTick&, const ChunkScheduledTick&) = default;
};

struct ChunkGeneratedEntity {
    wire::common::Identifier type{"minecraft", "marker"};
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    float yaw = 0.0F;
    float pitch = 0.0F;
    wire::nbt::NbtCompound data;
    std::vector<ChunkGeneratedEntity> passengers;

    friend bool operator==(const ChunkGeneratedEntity&, const ChunkGeneratedEntity&) = default;
};

/// they exist. parity: `StructurePiece.placeBlock` marks these with `markPosForPostProcessing`.
struct ChunkShapeUpdate {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;

    friend bool operator==(const ChunkShapeUpdate&, const ChunkShapeUpdate&) = default;
};

struct ChunkGeneratedBlockWrite {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;
    std::int32_t expectedStateId = 0;
    std::int32_t stateId = 0;
    bool replaceAnyState = false;
    GeneratedBlockWritePolicy policy = GeneratedBlockWritePolicy::Exact;

    friend bool operator==(const ChunkGeneratedBlockWrite&, const ChunkGeneratedBlockWrite&) = default;
};

struct ChunkGeneratedBlockEntityWrite {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;
    std::int32_t expectedStateId = 0;
    wire::common::Identifier type{"minecraft", "chest"};
    wire::nbt::NbtCompound data;

    friend bool operator==(const ChunkGeneratedBlockEntityWrite&, const ChunkGeneratedBlockEntityWrite&) = default;
};

struct ChunkGeneratedBlockTick {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;
    std::int32_t expectedStateId = 0;
    wire::common::Identifier type{"minecraft", "water"};
    std::int32_t delay = 0;
    std::int32_t priority = 0;

    friend bool operator==(const ChunkGeneratedBlockTick&, const ChunkGeneratedBlockTick&) = default;
};

[[nodiscard]] bool isInsideChunk(std::int32_t localX, std::int32_t y, std::int32_t localZ, ChunkHeight bounds);

} // namespace basalt::server::world
