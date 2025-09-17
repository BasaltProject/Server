#pragma once

#include <basalt/server/world/ChunkHeight.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>
#include <optional>
#include <vector>

namespace basalt::server::world {

/// How a generated write decides whether it is still allowed to land.
///
/// Decoration is placed after the terrain it grew from, and by then a neighbouring chunk may have
/// written over the spot. Each policy is the guard vanilla's own feature uses.
enum class GeneratedBlockWritePolicy : std::uint8_t {
    Exact,
    Tree,
    /// An ore or stone blob, guarded by its own target predicate rather than by one exact block.
    OreBaseStone,
    OreStone,
    OreDeepslate,
};

/// One block entity of a chunk: where it sits, what kind it is, and the NBT it carries.
///
/// The type is the *block entity type*, not the block. Callers may hand in either, since a block is
/// what they have: a block that owns a differently named type is translated on construction rather
/// than at each of the twenty places that build one. Getting that wrong left a copper chest without
/// a protocol id, so it never reached the client and the block rendered as nothing.
class ChunkBlockEntity {
public:
    ChunkBlockEntity(std::int32_t localX, std::int32_t y, std::int32_t localZ, const wire::common::Identifier& type,
                     wire::nbt::NbtCompound data);

    [[nodiscard]] std::int32_t localX() const noexcept { return localX_; }

    [[nodiscard]] std::int32_t y() const noexcept { return y_; }

    [[nodiscard]] std::int32_t localZ() const noexcept { return localZ_; }

    [[nodiscard]] const wire::common::Identifier& type() const noexcept { return type_; }

    [[nodiscard]] const wire::nbt::NbtCompound& data() const noexcept { return data_; }

    /// The same block entity carrying different NBT, which is what a `/data` write leaves behind.
    [[nodiscard]] ChunkBlockEntity withData(wire::nbt::NbtCompound data) const;

    friend bool operator==(const ChunkBlockEntity&, const ChunkBlockEntity&) = default;

private:
    std::int32_t localX_;
    std::int32_t y_;
    std::int32_t localZ_;
    wire::common::Identifier type_;
    wire::nbt::NbtCompound data_;
};

/// parity: `SavedTick`, owned by one chunk; x and z are local until it is written to disk.
struct ChunkScheduledTick {
    wire::common::Identifier type;
    std::int32_t localX = 0;
    std::int32_t y = 0;
    std::int32_t localZ = 0;
    std::int32_t delay = 0;
    /// parity: `TickPriority`, which runs from -3 to 3.
    std::int32_t priority = 0;

    friend bool operator==(const ChunkScheduledTick&, const ChunkScheduledTick&) = default;
};

/// Entity emitted by world generation and waiting for the runtime entity layer to materialize it.
struct ChunkGeneratedEntity {
    wire::common::Identifier type;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    float yaw = 0.0F;
    float pitch = 0.0F;
    wire::nbt::NbtCompound data;
    std::vector<ChunkGeneratedEntity> passengers;

    friend bool operator==(const ChunkGeneratedEntity&, const ChunkGeneratedEntity&) = default;
};

/// A generated decoration write whose target lies outside the chunk that produced it.
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

/// Whether these coordinates sit inside a chunk of those bounds. A block entity or a tick outside it
/// is a caller bug, and the chunk refuses it rather than storing something it cannot write back.
[[nodiscard]] bool isInsideChunk(std::int32_t localX, std::int32_t y, std::int32_t localZ, ChunkHeight bounds);

} // namespace basalt::server::world
