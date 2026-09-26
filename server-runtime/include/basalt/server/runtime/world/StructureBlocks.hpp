#pragma once

#include <basalt/wire/nbt/NbtTag.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace basalt::server::runtime::world {

class WorldState;

/// A structure block's saved settings, the corners it looks for and the structure it captures.
///
/// parity: `StructureBlockEntity` and `StructureTemplate.fillFromWorld` over `save`.
namespace structureBlocks {

/// parity: `StructureMode`, in its declared order.
enum class Mode : std::uint8_t { Save, Load, Corner, Data };

/// parity: `StructureBlockEntity.MAX_OFFSET_PER_AXIS`, `MAX_SIZE_PER_AXIS` and the corner search.
inline constexpr std::int32_t kMaxOffset = 48;
inline constexpr std::int32_t kMaxSize = 48;
inline constexpr std::int32_t kCornerRange = 80;

/// parity: the fields `StructureBlockEntity.saveAdditional` writes.
struct Settings {
    std::string name;
    std::string author;
    std::string metadata;
    std::array<std::int32_t, 3> offset{0, 1, 0};
    std::array<std::int32_t, 3> size{0, 0, 0};
    /// Quarter turns clockwise, and none, left-right or front-back.
    std::int32_t rotation = 0;
    std::int32_t mirror = 0;
    Mode mode = Mode::Data;
    bool ignoreEntities = true;
    bool strict = false;
    bool powered = false;
    bool showAir = false;
    bool showBoundingBox = true;
    float integrity = 1.0F;
    std::int64_t seed = 0;
};

/// parity: `loadAdditional`, the offset and size clamped as the jar clamps them.
[[nodiscard]] Settings read(const wire::nbt::NbtCompound& data);

/// parity: `saveAdditional`, over what else the tag holds.
void write(const Settings& settings, wire::nbt::NbtCompound& data);

[[nodiscard]] std::string_view modeName(Mode mode);

/// The lower-case form the block state's `mode` property takes.
[[nodiscard]] std::string_view modeProperty(Mode mode);

/// parity: `hasStructureName` over `Identifier.tryParse`, and the name it resolves to.
[[nodiscard]] std::optional<std::string> resolvedName(std::string_view name);

/// An inclusive box of blocks.
struct Box {
    std::array<std::int32_t, 3> min{};
    std::array<std::int32_t, 3> max{};
};

/// parity: `getEnclosingBoundingBox`: the corner blocks of the same name within eighty across,
/// taking the block itself in when only one is found.
[[nodiscard]] std::optional<Box> enclosingBox(WorldState& world, const std::array<std::int32_t, 3>& self,
                                              const std::string& name);

/// parity: `StructureTemplate.fillFromWorld` then `save`: the blocks of the box less structure void,
/// full ones first, then the rest, then those with block entities, each sorted by y, x and z; and the
/// entities in it when asked.
[[nodiscard]] wire::nbt::NbtCompound capture(WorldState& world, const std::array<std::int32_t, 3>& origin,
                                             const std::array<std::int32_t, 3>& size, bool withEntities);

} // namespace structureBlocks

} // namespace basalt::server::runtime::world
