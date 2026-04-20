#pragma once

#include <basalt/wire/common/Identifier.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace basalt::server::registry {
using TagContains = std::function<bool(std::string_view tag, const wire::common::Identifier& block)>;

// parity: `ToolComponent`.
namespace itemTools {
struct Rule {
    std::string_view blockTag;
    bool correctForDrops = false;
    std::optional<float> speed;
};

[[nodiscard]] std::span<const Rule> rulesFor(const wire::common::Identifier& item);

[[nodiscard]] bool isCorrectForDrops(const wire::common::Identifier* item, const wire::common::Identifier& block,
                                     const TagContains& tagContains);

[[nodiscard]] float miningSpeed(const wire::common::Identifier* item, const wire::common::Identifier& block,
                                const TagContains& tagContains);

struct DigConditions {
    std::int32_t efficiencyLevel = 0;
    // parity: `MobEffectUtil.getDigSpeedAmplification`.
    std::int32_t hasteAmplifier = -1;
    // parity: `MobEffects.MINING_FATIGUE`'s amplifier.
    std::int32_t miningFatigueAmplifier = -1;
    bool underwater = false;
    bool inAir = false;
};

// parity: `BlockState.getDestroyProgress` over `Player.getDestroySpeed`.
[[nodiscard]] float destroyProgressPerTick(const wire::common::Identifier* item, const wire::common::Identifier& block,
                                           float hardness, bool requiresCorrectTool, const DigConditions& conditions,
                                           const TagContains& tagContains);
} // namespace itemTools
} // namespace basalt::server::registry
