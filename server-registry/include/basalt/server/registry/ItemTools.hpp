#pragma once

#include <basalt/wire/common/Identifier.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace basalt::server::registry {

/// Whether a block tag holds a block.
///
/// Tag membership is not known here (this module holds no datapack content), so it is asked for: the
/// server resolves the real block tags out of the operator's own jar and passes a lookup in.
using TagContains = std::function<bool(std::string_view tag, const wire::common::Identifier& block)>;

/// Which blocks each tool mines properly, and how fast.
///
/// parity: `ToolComponent`. Both answers come from the **first** rule whose tag contains the block,
/// which is what makes a diamond pickaxe correct for stone but not for the handful of blocks its
/// `incorrect_for_diamond_tool` tag excludes: that exclusion is listed first and therefore wins.
namespace itemTools {

/// One rule of an item's `minecraft:tool` component.
struct Rule {
    std::string_view blockTag;
    bool correctForDrops = false;
    /// Absent when the rule only excludes rather than saying a speed.
    std::optional<float> speed;
};

/// This item's mining rules, in order, or empty if it is not a tool at all.
[[nodiscard]] std::span<const Rule> rulesFor(const wire::common::Identifier& item);

/// Whether holding `item` means `block` drops what it should.
///
/// A block that does not require a tool at all drops regardless, so callers check that first; this
/// answers only the tool half. An item with no rules (a bare hand, a stick, a diamond) is never
/// correct, which is exactly why stone gives nothing when punched.
[[nodiscard]] bool isCorrectForDrops(const wire::common::Identifier* item, const wire::common::Identifier& block,
                                     const TagContains& tagContains);

/// How fast `item` mines `block`, as a multiplier on the bare-hand speed of 1.
///
/// The dig itself is timed by the client, which knows this from its own copy of the same data. The
/// server needs it anyway to decide whether a block comes apart in one go: that is the one moment
/// vanilla asks the question server-side too.
[[nodiscard]] float miningSpeed(const wire::common::Identifier* item, const wire::common::Identifier& block,
                                const TagContains& tagContains);

/// Everything that scales a dig besides the tool itself. Absent amplifiers mean nobody hasted and
/// nobody slowed.
struct DigConditions {
    std::int32_t efficiencyLevel = 0;
    /// parity: `MobEffectUtil.getDigSpeedAmplification`.
    std::int32_t hasteAmplifier = -1;
    /// parity: `MobEffects.MINING_FATIGUE`'s amplifier.
    std::int32_t miningFatigueAmplifier = -1;
    bool underwater = false;
    bool inAir = false;
};

/// How much of `block` one tick of mining takes off, 1 or more meaning it breaks at once.
///
/// parity: `BlockState.getDestroyProgress` over `Player.getDestroySpeed`. Attributes are not
/// modelled, so this is the tool, the enchantment, the two effects, and the 30 or 100 divisor that
/// makes the wrong tool slow rather than useless.
[[nodiscard]] float destroyProgressPerTick(const wire::common::Identifier* item, const wire::common::Identifier& block,
                                           float hardness, bool requiresCorrectTool, const DigConditions& conditions,
                                           const TagContains& tagContains);

} // namespace itemTools

} // namespace basalt::server::registry
