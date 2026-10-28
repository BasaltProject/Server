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

/// parity: `RuleTest`, narrowed to the predicate kinds the real processor lists use.
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
    std::set<std::string, std::less<>> blocks_;
};

/// parity: `ProcessorRule`.
struct StructureProcessorRule {
    StructureRuleTest inputTest;
    std::optional<StructureRuleTest> locationTest;
    std::int32_t outputStateId = 0;
    /// parity: `block_entity_modifier: append_loot`, the loot table the output block gets.
    std::optional<std::string> appendLootTable;

    [[nodiscard]] std::optional<std::int32_t> apply(std::int32_t inputStateId, std::int32_t worldStateId,
                                                    random::RandomSource& random) const;
};

/// parity: `BlockRotProcessor`.
struct BlockRot {
    float integrity = 1.0F;
    std::optional<std::set<std::int32_t>> rottable;

    /// `shared` is the placement's own stream when it set one.
    [[nodiscard]] bool keeps(std::int32_t stateId, StructurePos position, random::RandomSource* shared) const;
};

/// parity: `CappedProcessor`.
struct CappedRules {
    std::vector<StructureProcessorRule> delegate;
    std::int32_t limit = 0;

    /// Picks the candidates to convert, up to `limit`. parity: `finalizeProcessing`.
    [[nodiscard]] std::map<std::int32_t, StructureProcessorRule>
    choose(const std::vector<std::int32_t>& candidates, std::int64_t worldSeed, StructurePos position) const;
};

/// parity: `RuleProcessor`.
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

    [[nodiscard]] bool empty() const noexcept {
        return rules_.empty() && !rot_.has_value() && !protectedBlocks_.has_value() && capped_.empty();
    }

    [[nodiscard]] std::int32_t process(std::int32_t inputStateId, std::int32_t worldStateId,
                                       random::RandomSource& random) const;

    /// parity: `RuleProcessor` creates a fresh legacy random from the positional seed at the.
    [[nodiscard]] std::optional<std::int32_t> processAt(std::int32_t inputStateId, std::int32_t worldStateId,
                                                        StructurePos position,
                                                        random::RandomSource* shared = nullptr) const;

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
