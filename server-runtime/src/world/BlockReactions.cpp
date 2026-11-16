#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/world/BlockReactions.hpp>

#include <algorithm>
#include <array>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::world::blockReactions {
namespace {

[[nodiscard]] bool isInertBlock(std::string_view path) {
    static constexpr std::array<std::string_view, 30> kExact{"air",
                                                             "cave_air",
                                                             "void_air",
                                                             "stone",
                                                             "granite",
                                                             "polished_granite",
                                                             "diorite",
                                                             "polished_diorite",
                                                             "andesite",
                                                             "polished_andesite",
                                                             "cobblestone",
                                                             "mossy_cobblestone",
                                                             "stone_bricks",
                                                             "mossy_stone_bricks",
                                                             "cracked_stone_bricks",
                                                             "chiseled_stone_bricks",
                                                             "smooth_stone",
                                                             "bricks",
                                                             "deepslate",
                                                             "cobbled_deepslate",
                                                             "polished_deepslate",
                                                             "deepslate_bricks",
                                                             "tuff",
                                                             "dirt",
                                                             "coarse_dirt",
                                                             "netherrack",
                                                             "terracotta",
                                                             "sandstone",
                                                             "red_sandstone",
                                                             "bedrock"};
    if (std::ranges::find(kExact, path) != kExact.end()) {
        return true;
    }
    return path.ends_with("_planks") || path.ends_with("_wool") || path.ends_with("_concrete") ||
           (path.ends_with("_terracotta") && !path.ends_with("_glazed_terracotta"));
}

[[nodiscard]] Kind classify(std::string_view path) {
    if (isInertBlock(path)) {
        return Kind::kInert;
    }
    if (path == "water" || path == "lava") {
        return Kind::kLiquid;
    }
    if (path == "sand" || path == "red_sand" || path == "gravel") {
        return Kind::kFalling;
    }
    return Kind::kFull;
}

} // namespace

Kind kindOf(const std::int32_t stateId) {
    static const std::vector<Kind> table = [] {
        std::vector<Kind> built(static_cast<std::size_t>(registry::blockStateDefinitions::stateIdBound()), Kind::kFull);
        for (std::size_t id = 0; id < built.size(); ++id) {
            const wire::common::Identifier* block =
                registry::blockStateDefinitions::blockOf(static_cast<std::int32_t>(id));
            if (block != nullptr) {
                built[id] = classify(block->path());
            }
        }
        return built;
    }();
    return stateId >= 0 && static_cast<std::size_t>(stateId) < table.size() ? table[static_cast<std::size_t>(stateId)]
                                                                            : Kind::kFull;
}

} // namespace basalt::server::runtime::world::blockReactions
