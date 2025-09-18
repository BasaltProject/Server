#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/OreBlockRules.hpp>

#include <algorithm>

namespace basalt::server::world::terrain::decoration {
namespace {

using BlockSet = std::set<std::string, std::less<>>;

const BlockSet& baseStone() {
    static const BlockSet blocks = {"stone", "granite", "diorite", "andesite", "tuff", "deepslate"};
    return blocks;
}

const BlockSet& stoneReplaceables() {
    static const BlockSet blocks = {"stone", "granite", "diorite", "andesite"};
    return blocks;
}

const BlockSet& deepslateReplaceables() {
    static const BlockSet blocks = {"deepslate", "tuff"};
    return blocks;
}

[[nodiscard]] bool contains(const BlockSet& set, const std::string& path) { return set.find(path) != set.end(); }

[[nodiscard]] bool containsAll(const BlockSet& haystack, const BlockSet& needles) {
    return std::all_of(needles.begin(), needles.end(),
                       [&haystack](const std::string& needle) { return haystack.find(needle) != haystack.end(); });
}

} // namespace

bool oreBlockRules::canReplaceDeferred(GeneratedBlockWritePolicy policy, std::int32_t actualStateId) {
    const std::optional<registry::BlockState> block = registry::blocks::byStateId(actualStateId);
    if (!block.has_value()) {
        return false;
    }
    const std::string path = block->block.path();
    switch (policy) {
    case GeneratedBlockWritePolicy::OreBaseStone:
        return contains(baseStone(), path);
    case GeneratedBlockWritePolicy::OreStone:
        return contains(stoneReplaceables(), path);
    case GeneratedBlockWritePolicy::OreDeepslate:
        return contains(deepslateReplaceables(), path);
    case GeneratedBlockWritePolicy::Exact:
    case GeneratedBlockWritePolicy::Tree:
        break;
    }
    return false;
}

GeneratedBlockWritePolicy oreBlockRules::policyFor(const std::set<std::string, std::less<>>& replaceable) {
    if (replaceable.size() == deepslateReplaceables().size() && containsAll(replaceable, deepslateReplaceables())) {
        return GeneratedBlockWritePolicy::OreDeepslate;
    }
    if (replaceable.size() == stoneReplaceables().size() && containsAll(replaceable, stoneReplaceables())) {
        return GeneratedBlockWritePolicy::OreStone;
    }
    if (containsAll(replaceable, baseStone())) {
        return GeneratedBlockWritePolicy::OreBaseStone;
    }
    // Disks and anything else keep the strict guard they had.
    return GeneratedBlockWritePolicy::Exact;
}

} // namespace basalt::server::world::terrain::decoration
