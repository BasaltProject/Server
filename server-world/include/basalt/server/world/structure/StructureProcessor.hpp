#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/structure/StructureBounds.hpp>

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace basalt::server::world::structure {

/// parity: `Util.shuffle`, Fisher-Yates from the top with java.util.Random's swap order.
template <typename T>
[[nodiscard]] std::vector<T> shuffledCopy(std::vector<T> values, random::RandomSource& random) {
    for (auto index = static_cast<std::int32_t>(values.size()); index > 1; --index) {
        const std::int32_t swapTo = random.nextInt(index);
        std::swap(values[static_cast<std::size_t>(index - 1)], values[static_cast<std::size_t>(swapTo)]);
    }
    return values;
}

/// parity: `RuleTest`, narrowed to the predicate kinds the real processor lists use:
/// `random_block_match` (a fixed block, accepted with a chance), `block_match` (any state of a fixed
/// block), `blockstate_match` (one exact property-bearing state) and `tag_match` (any block of a
/// named tag). `always_true` is the null predicate and is represented as an absent test.
class StructureRuleTest {
public:
    enum class Kind : std::uint8_t { BlockMatch, RandomBlockMatch, BlockStateMatch, TagMatch };

    [[nodiscard]] static StructureRuleTest blockMatch(std::int32_t blockStateId);
    [[nodiscard]] static StructureRuleTest randomBlockMatch(std::int32_t blockStateId, float probability);
    [[nodiscard]] static StructureRuleTest blockStateMatch(std::int32_t stateId);
    [[nodiscard]] static StructureRuleTest tagMatch(const std::set<std::int32_t>& stateIds);

    [[nodiscard]] bool test(std::int32_t stateId, random::RandomSource& random) const;

private:
    Kind kind_ = Kind::BlockMatch;
    std::int32_t stateId_ = 0;
    float probability_ = 0.0F;
    /// The block paths a match accepts, resolved once so the test is a lookup and not a registry
    /// walk.
    std::set<std::string, std::less<>> blocks_;
};

/// parity: `ProcessorRule`. The input test checks the structure's own unplaced block; the location
/// test, when present, checks whatever the world already holds at the target position.
struct StructureProcessorRule {
    StructureRuleTest inputTest;
    std::optional<StructureRuleTest> locationTest;
    std::int32_t outputStateId = 0;
    /// parity: `block_entity_modifier: append_loot`, the loot table the output block gets.
    std::optional<std::string> appendLootTable;

    [[nodiscard]] std::optional<std::int32_t> apply(std::int32_t inputStateId, std::int32_t worldStateId,
                                                    random::RandomSource& random) const;
};

/// parity: `BlockRotProcessor`. Drops a share of the blocks it is allowed to rot, which is what makes
/// a ruin look ruined. It draws only for a block the tag covers, so a wall of something else costs
/// no randomness at all.
struct BlockRot {
    float integrity = 1.0F;
    std::optional<std::set<std::int32_t>> rottable;

    /// `shared` is the placement's own stream when it set one. parity:
    /// `StructurePlaceSettings.getRandom`.
    [[nodiscard]] bool keeps(std::int32_t stateId, StructurePos position, random::RandomSource* shared) const;
};

/// parity: `CappedProcessor`, a second pass converting at most `limit` blocks. It evaluates the
/// piece's whole list rather than the part inside the chunk, and its random is a positional fork of
/// the world seed, so it neither consumes from the feature stream nor depends on it.
struct CappedRules {
    std::vector<StructureProcessorRule> delegate;
    std::int32_t limit = 0;

    /// The indices of `candidates` this converts, in the order the shuffle offers them, stopping at
    /// `limit` actual conversions. parity: `finalizeProcessing`, which counts only the blocks the
    /// delegate really changed, not the ones it looked at.
    [[nodiscard]] std::map<std::int32_t, StructureProcessorRule>
    choose(const std::vector<std::int32_t>& candidates, std::int64_t worldSeed, StructurePos position) const;
};

/// parity: `RuleProcessor`, first matching rule wins, preceded by a `BlockRotProcessor`. Each takes
/// its own random from the same positional seed rather than sharing a stream.
class StructureProcessorList {
public:
    StructureProcessorList() = default;

    explicit StructureProcessorList(std::vector<StructureProcessorRule> rules,
                                    std::optional<BlockRot> rot = std::nullopt,
                                    std::optional<std::set<std::int32_t>> protectedBlocks = std::nullopt,
                                    std::vector<CappedRules> capped = {})
        : rules_(std::move(rules)), rot_(std::move(rot)), protectedBlocks_(std::move(protectedBlocks)),
          capped_(std::move(capped)) {}

    [[nodiscard]] const std::vector<StructureProcessorRule>& rules() const noexcept { return rules_; }

    [[nodiscard]] const std::vector<CappedRules>& capped() const noexcept { return capped_; }

    /// Whether this list does anything at all. A caller must ask this and not whether the rules are
    /// empty: a list can carry only a block rot, which has no rules and still drops most of the
    /// blocks.
    [[nodiscard]] bool empty() const noexcept {
        return rules_.empty() && !rot_.has_value() && !protectedBlocks_.has_value() && capped_.empty();
    }

    [[nodiscard]] std::int32_t process(std::int32_t inputStateId, std::int32_t worldStateId,
                                       random::RandomSource& random) const;

    /// The state to place, or empty when the rot dropped this block.
    ///
    /// parity: `RuleProcessor` creates a fresh legacy random from the positional seed at the
    /// transformed world position for every block. The placement's shared stream is not consumed
    /// unless the caller passes one.
    [[nodiscard]] std::optional<std::int32_t> processAt(std::int32_t inputStateId, std::int32_t worldStateId,
                                                        StructurePos position,
                                                        random::RandomSource* shared = nullptr) const;

    /// The `append_loot` table of the rule that matches here, tested with the same positional random.
    [[nodiscard]] std::optional<std::string> appendedLootAt(std::int32_t inputStateId, std::int32_t worldStateId,
                                                            StructurePos position) const;

    /// parity: `processor_list/mossify_{20,70,10}_percent.json`, verbatim.
    [[nodiscard]] static StructureProcessorList mossify(float probability);

private:
    std::vector<StructureProcessorRule> rules_;
    std::optional<BlockRot> rot_;
    std::optional<std::set<std::int32_t>> protectedBlocks_;
    std::vector<CappedRules> capped_;
};

} // namespace basalt::server::world::structure
