#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace basalt::server::runtime::entity {

/// The eight kinds a mob can be, and what each is worth to the world.
///
/// parity: `MobCategory`, whose numbers decide how many of that kind the world keeps per chunk, how
/// far away one despawns, and whether it counts as friendly. They are declared in code rather than
/// data, so they are declared here too, in the same order and with the same values.
enum class MobCategory : std::uint8_t {
    Monster,
    Creature,
    Ambient,
    Axolotls,
    UndergroundWaterCreature,
    WaterCreature,
    WaterAmbient,
    Misc,
};

struct MobCategoryFacts {
    std::string_view categoryName;
    std::int32_t maxPerChunk = 0;
    bool friendly = false;
    bool persistent = false;
    std::int32_t despawnDistance = 0;
};

/// parity: `MobCategory.noDespawnDistance`, the same 32 for every category.
inline constexpr std::int32_t kNoDespawnDistance = 32;

[[nodiscard]] const MobCategoryFacts& mobCategoryFacts(MobCategory category) noexcept;

[[nodiscard]] std::optional<MobCategory> mobCategoryByName(std::string_view name);

} // namespace basalt::server::runtime::entity
