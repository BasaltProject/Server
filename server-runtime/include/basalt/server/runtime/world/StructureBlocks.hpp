#pragma once

#include <basalt/wire/nbt/NbtTag.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace basalt::server::runtime::world {

class WorldState;

/// parity: `StructureBlockEntity` and `StructureTemplate.fillFromWorld` over `save`.
namespace structureBlocks {

/// parity: `StructureMode`, in its declared order.
enum class Mode : std::uint8_t { Save, Load, Corner, Data };

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

[[nodiscard]] std::string_view modeProperty(Mode mode);

/// parity: `hasStructureName` over `Identifier.tryParse`, and the name it resolves to.
[[nodiscard]] std::optional<std::string> resolvedName(std::string_view name);

struct Box {
    std::array<std::int32_t, 3> min{};
    std::array<std::int32_t, 3> max{};
};

/// parity: `getEnclosingBoundingBox`.
[[nodiscard]] std::optional<Box> enclosingBox(WorldState& world, const std::array<std::int32_t, 3>& self,
                                              const std::string& name);

/// parity: `fillFromWorld`.
[[nodiscard]] wire::nbt::NbtCompound capture(WorldState& world, const std::array<std::int32_t, 3>& origin,
                                             const std::array<std::int32_t, 3>& size, bool withEntities);

} // namespace structureBlocks

} // namespace basalt::server::runtime::world
