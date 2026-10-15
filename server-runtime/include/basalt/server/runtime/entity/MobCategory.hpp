#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace basalt::server::runtime::entity {

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

inline constexpr std::int32_t kNoDespawnDistance = 32;

[[nodiscard]] const MobCategoryFacts& mobCategoryFacts(MobCategory category) noexcept;

[[nodiscard]] std::optional<MobCategory> mobCategoryByName(std::string_view name);

} // namespace basalt::server::runtime::entity
