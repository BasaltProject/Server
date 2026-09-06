#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/OreBlockRules.hpp>
#include <basalt/server/world/terrain/decoration/StoneBlockTags.hpp>

#include <algorithm>

namespace basalt::server::world::terrain::decoration {
namespace {

using BlockSet = std::set<std::string, std::less<>>;

const BlockSet& baseStone() { return stoneBlockTags::baseStoneOverworld(); }

const BlockSet& stoneReplaceables() { return stoneBlockTags::stoneOreReplaceables(); }

const BlockSet& deepslateReplaceables() { return stoneBlockTags::deepslateOreReplaceables(); }

/// parity: block tag `features_cannot_replace`, the only thing a geode refuses to carve through.
const BlockSet& geodeCannotReplace() {
    static const BlockSet blocks = {"bedrock", "spawner",      "chest", "end_portal_frame", "reinforced_deepslate",
                                    "vault",   "trial_spawner"};
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
    case GeneratedBlockWritePolicy::Geode:
        return !contains(geodeCannotReplace(), path);
    case GeneratedBlockWritePolicy::Exact:
    case GeneratedBlockWritePolicy::Tree:
        break;
    }
    return false;
}

bool oreBlockRules::survivesLaterDecoration(GeneratedBlockWritePolicy policy, std::int32_t writeStateId) {
    switch (policy) {
    case GeneratedBlockWritePolicy::OreBaseStone:
    case GeneratedBlockWritePolicy::OreStone:
    case GeneratedBlockWritePolicy::OreDeepslate:
    case GeneratedBlockWritePolicy::Geode:
        break;
    case GeneratedBlockWritePolicy::Exact:
    case GeneratedBlockWritePolicy::Tree:
        return false;
    }
    const std::optional<registry::BlockState> block = registry::blocks::byStateId(writeStateId);
    return block.has_value() && !contains(baseStone(), block->block.path());
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
