#include <basalt/server/world/terrain/decoration/StoneBlockTags.hpp>

namespace basalt::server::world::terrain::decoration {

const stoneBlockTags::PathSet& stoneBlockTags::baseStoneOverworld() {
    static const PathSet paths = {"stone", "granite", "diorite", "andesite", "tuff", "deepslate"};
    return paths;
}

const stoneBlockTags::PathSet& stoneBlockTags::stoneOreReplaceables() {
    static const PathSet paths = {"stone", "granite", "diorite", "andesite"};
    return paths;
}

const stoneBlockTags::PathSet& stoneBlockTags::deepslateOreReplaceables() {
    static const PathSet paths = {"deepslate"};
    return paths;
}

const stoneBlockTags::PathSet& stoneBlockTags::heightSpecificOreReplaceables() {
    static const PathSet paths = {"tuff"};
    return paths;
}

} // namespace basalt::server::world::terrain::decoration
