#include <basalt/server/registry/ItemTools.hpp>
#include <basalt/server/registry/VanillaEntries.hpp>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <unordered_map>

namespace basalt::server::registry::itemTools {
namespace {
using wire::common::Identifier;

// parity: `MobEffects.MINING_FATIGUE` step multipliers.
[[nodiscard]] float fatigueScale(std::int32_t amplifier) {
    switch (amplifier) {
    case 0:
        return 0.3F;
    case 1:
        return 0.09F;
    case 2:
        return 0.0027F;
    default:
        return 0.00081F;
    }
}

constexpr float kHastePerLevel = 0.2F;
constexpr float kSubmergedMiningSpeed = 0.2F;
constexpr float kInAirDivisor = 5.0F;
constexpr float kCorrectToolDivisor = 30.0F;
constexpr float kWrongToolDivisor = 100.0F;

[[nodiscard]] const std::unordered_map<Identifier, std::vector<Rule>>& table() {
    static const std::unordered_map<Identifier, std::vector<Rule>> built = [] {
        std::unordered_map<Identifier, std::vector<Rule>> table;
        for (const ToolRule& rule : itemToolsTable()) {
            table[Identifier::vanilla(std::string{rule.item})].push_back(
                {rule.blockTag, rule.correctForDrops, rule.speed});
        }
        return table;
    }();
    return built;
}

[[nodiscard]] const Rule* firstMatch(const Identifier* item, const Identifier& block, const TagContains& tagContains) {
    if (item == nullptr) {
        return nullptr;
    }
    const std::span<const Rule> rules = rulesFor(*item);
    const auto found = std::ranges::find_if(rules, [&](const Rule& rule) { return tagContains(rule.blockTag, block); });
    return found == rules.end() ? nullptr : &*found;
}
} // namespace

std::span<const Rule> rulesFor(const Identifier& item) {
    const auto found = table().find(item);
    return found == table().end() ? std::span<const Rule>{} : std::span<const Rule>{found->second};
}

bool isCorrectForDrops(const Identifier* item, const Identifier& block, const TagContains& tagContains) {
    const Rule* rule = firstMatch(item, block, tagContains);
    return rule != nullptr && rule->correctForDrops;
}

float miningSpeed(const Identifier* item, const Identifier& block, const TagContains& tagContains) {
    const Rule* rule = firstMatch(item, block, tagContains);
    return rule == nullptr ? 1.0F : rule->speed.value_or(1.0F);
}

float destroyProgressPerTick(const Identifier* item, const Identifier& block, float hardness, bool requiresCorrectTool,
                             const DigConditions& conditions, const TagContains& tagContains) {
    if (hardness < 0.0F) {
        return 0.0F;
    }
    if (hardness == 0.0F) {
        return std::numeric_limits<float>::max();
    }
    float speed = miningSpeed(item, block, tagContains);
    // parity: the enchantment only helps a tool that is already faster than a bare hand.
    if (speed > 1.0F && conditions.efficiencyLevel > 0) {
        speed += static_cast<float>(conditions.efficiencyLevel * conditions.efficiencyLevel) + 1.0F;
    }
    if (conditions.hasteAmplifier >= 0) {
        speed *= 1.0F + static_cast<float>(conditions.hasteAmplifier + 1) * kHastePerLevel;
    }
    if (conditions.miningFatigueAmplifier >= 0) {
        speed *= fatigueScale(conditions.miningFatigueAmplifier);
    }
    // parity: `Player.getDestroySpeed`.
    if (conditions.underwater) {
        speed *= kSubmergedMiningSpeed;
    }
    if (conditions.inAir) {
        speed /= kInAirDivisor;
    }
    const bool correct = !requiresCorrectTool || isCorrectForDrops(item, block, tagContains);
    return speed / hardness / (correct ? kCorrectToolDivisor : kWrongToolDivisor);
}
} // namespace basalt::server::registry::itemTools
