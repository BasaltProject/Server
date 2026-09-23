#include <basalt/server/random/PositionalHashing.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/structure/StructureProcessor.hpp>

#include <algorithm>
#include <cstdint>
#include <numeric>

namespace basalt::server::world::structure {
namespace {

[[nodiscard]] std::string pathOf(std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block == nullptr ? std::string{} : block->path();
}

[[nodiscard]] random::RandomSource positionalRandom(StructurePos position) {
    return random::RandomSource::worldgen(
        random::RandomSource::legacy(random::positionalSeed(position.x, position.y, position.z)));
}

} // namespace

StructureRuleTest StructureRuleTest::blockMatch(const std::int32_t blockStateId) {
    StructureRuleTest test;
    test.kind_ = Kind::BlockMatch;
    test.stateId_ = blockStateId;
    const std::string path = pathOf(blockStateId);
    if (!path.empty()) {
        test.blocks_.insert(path);
    }
    return test;
}

StructureRuleTest StructureRuleTest::randomBlockMatch(const std::int32_t blockStateId, const float probability) {
    StructureRuleTest test = blockMatch(blockStateId);
    test.kind_ = Kind::RandomBlockMatch;
    test.probability_ = probability;
    return test;
}

StructureRuleTest StructureRuleTest::blockStateMatch(const std::int32_t stateId) {
    StructureRuleTest test;
    test.kind_ = Kind::BlockStateMatch;
    test.stateId_ = stateId;
    return test;
}

StructureRuleTest StructureRuleTest::tagMatch(const std::set<std::int32_t>& stateIds) {
    StructureRuleTest test;
    test.kind_ = Kind::TagMatch;
    for (const std::int32_t stateId : stateIds) {
        const std::string path = pathOf(stateId);
        if (!path.empty()) {
            test.blocks_.insert(path);
        }
    }
    return test;
}

bool StructureRuleTest::test(const std::int32_t stateId, random::RandomSource& random) const {
    switch (kind_) {
    case Kind::BlockStateMatch:
        return stateId == stateId_;
    case Kind::RandomBlockMatch:
        return !blocks_.empty() && blocks_.contains(pathOf(stateId)) && random.nextFloat() < probability_;
    case Kind::TagMatch:
        return blocks_.contains(pathOf(stateId));
    case Kind::BlockMatch:
    default:
        return !blocks_.empty() && blocks_.contains(pathOf(stateId));
    }
}

std::optional<std::int32_t> StructureProcessorRule::apply(const std::int32_t inputStateId,
                                                          const std::int32_t worldStateId,
                                                          random::RandomSource& random) const {
    if (!inputTest.test(inputStateId, random)) {
        return std::nullopt;
    }
    if (locationTest.has_value() && !locationTest->test(worldStateId, random)) {
        return std::nullopt;
    }
    return outputStateId;
}

bool BlockRot::keeps(const std::int32_t stateId, const StructurePos position, random::RandomSource* shared) const {
    if (rottable.has_value() && !rottable->contains(stateId)) {
        return true;
    }
    if (shared != nullptr) {
        return shared->nextFloat() <= integrity;
    }
    random::RandomSource random = positionalRandom(position);
    return random.nextFloat() <= integrity;
}

std::map<std::int32_t, StructureProcessorRule> CappedRules::choose(const std::vector<std::int32_t>& candidates,
                                                                   const std::int64_t worldSeed,
                                                                   const StructurePos position) const {
    if (limit == 0 || candidates.empty()) {
        return {};
    }
    random::RandomSource seedSource = random::RandomSource::legacy(worldSeed);
    const std::int64_t factorySeed = seedSource.nextLong();
    random::RandomSource random = random::RandomSource::worldgen(
        random::RandomSource::legacy(random::positionalSeed(position.x, position.y, position.z) ^ factorySeed));
    const auto maxToReplace = std::min<std::size_t>(static_cast<std::size_t>(limit), candidates.size());
    std::vector<std::int32_t> indices(candidates.size());
    std::iota(indices.begin(), indices.end(), 0);
    const std::vector<std::int32_t> order = shuffledCopy(std::move(indices), random);
    std::map<std::int32_t, StructureProcessorRule> chosen;
    for (const std::int32_t index : order) {
        if (chosen.size() >= maxToReplace) {
            break;
        }
        const std::int32_t stateId = candidates[static_cast<std::size_t>(index)];
        for (const StructureProcessorRule& rule : delegate) {
            if (rule.inputTest.test(stateId, random) && rule.outputStateId != stateId) {
                chosen.emplace(index, rule);
                break;
            }
        }
    }
    return chosen;
}

std::int32_t StructureProcessorList::process(const std::int32_t inputStateId, const std::int32_t worldStateId,
                                             random::RandomSource& random) const {
    for (const StructureProcessorRule& rule : rules_) {
        const std::optional<std::int32_t> result = rule.apply(inputStateId, worldStateId, random);
        if (result.has_value()) {
            return *result;
        }
    }
    return inputStateId;
}

std::optional<std::int32_t> StructureProcessorList::processAt(const std::int32_t inputStateId,
                                                              const std::int32_t worldStateId,
                                                              const StructurePos position,
                                                              random::RandomSource* shared) const {
    if (rot_.has_value() && !rot_->keeps(inputStateId, position, shared)) {
        return std::nullopt;
    }
    if (protectedBlocks_.has_value() && protectedBlocks_->contains(worldStateId)) {
        return std::nullopt;
    }
    if (shared != nullptr) {
        return process(inputStateId, worldStateId, *shared);
    }
    random::RandomSource random = positionalRandom(position);
    return process(inputStateId, worldStateId, random);
}

std::optional<std::string> StructureProcessorList::appendedLootAt(const std::int32_t inputStateId,
                                                                  const std::int32_t worldStateId,
                                                                  const StructurePos position) const {
    random::RandomSource random = positionalRandom(position);
    for (const StructureProcessorRule& rule : rules_) {
        if (rule.apply(inputStateId, worldStateId, random).has_value()) {
            return rule.appendLootTable;
        }
    }
    return std::nullopt;
}

StructureProcessorList StructureProcessorList::mossify(const float probability) {
    const auto blockId = [](std::string_view path) {
        const registry::BlockState* found = registry::blocks::named(path);
        return found == nullptr ? registry::blocks::air().stateId : found->stateId;
    };
    return StructureProcessorList{
        {StructureProcessorRule{.inputTest = StructureRuleTest::randomBlockMatch(blockId("cobblestone"), probability),
                                .locationTest = std::nullopt,
                                .outputStateId = blockId("mossy_cobblestone")}}};
}

} // namespace basalt::server::world::structure
