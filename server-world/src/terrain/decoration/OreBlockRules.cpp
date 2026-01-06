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

const BlockSet& oreFeatureBlocks() {
    static const BlockSet blocks = {"andesite",
                                    "clay",
                                    "coal_ore",
                                    "copper_ore",
                                    "deepslate_coal_ore",
                                    "deepslate_copper_ore",
                                    "deepslate_diamond_ore",
                                    "deepslate_emerald_ore",
                                    "deepslate_gold_ore",
                                    "deepslate_iron_ore",
                                    "deepslate_lapis_ore",
                                    "deepslate_redstone_ore",
                                    "diamond_ore",
                                    "diorite",
                                    "dirt",
                                    "emerald_ore",
                                    "gold_ore",
                                    "granite",
                                    "gravel",
                                    "iron_ore",
                                    "lapis_ore",
                                    "redstone_ore",
                                    "tuff"};
    return blocks;
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

bool oreBlockRules::outranksOreDecoration(std::int32_t writeStateId, std::int32_t currentStateId) {
    const std::optional<registry::BlockState> write = registry::blocks::byStateId(writeStateId);
    const std::optional<registry::BlockState> current = registry::blocks::byStateId(currentStateId);
    if (!write.has_value() || !current.has_value()) {
        return false;
    }
    // parity: a stone variant targets `base_stone_overworld`, which holds the other variants, so it
    // replaces them the same whether the cell was decorated or not.
    if (contains(baseStone(), write->block.path()) && contains(baseStone(), current->block.path())) {
        return true;
    }
    // Only an ore knows it could not have replaced this. Anything else that decorated the cell may
    // well have been allowed to.
    return !contains(baseStone(), write->block.path()) && contains(oreFeatureBlocks(), current->block.path());
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
