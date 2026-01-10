#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/structure/MineshaftBlocks.hpp>

#include <array>
#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>

namespace basalt::server::world::structure {
namespace {

constexpr std::array<std::string_view, 4> kFenceSides{"north", "south", "east", "west"};

[[nodiscard]] std::int32_t state(const std::string& path) {
    const registry::BlockState* found = registry::blocks::named(path);
    if (found == nullptr) {
        throw std::runtime_error{"unknown block '" + path + "'"};
    }
    return found->stateId;
}

[[nodiscard]] std::int32_t fenceWith(const std::string& wood, std::string_view side, std::int32_t fallback) {
    std::map<std::string, std::string> properties{{"waterlogged", "false"}};
    for (const std::string_view name : kFenceSides) {
        properties.emplace(std::string{name}, name == side ? "true" : "false");
    }
    return registry::blocks::stateIdOf(wire::common::Identifier::vanilla(wood + "_fence"), properties)
        .value_or(fallback);
}

} // namespace

MineshaftBlocks::MineshaftBlocks(const MineshaftType type) {
    const std::string wood = type == MineshaftType::Mesa ? "dark_oak" : "oak";
    log_ = state(wood + "_log");
    planks_ = state(wood + "_planks");
    fence_ = state(wood + "_fence");
    fenceWest_ = fenceWith(wood, "west", fence_);
    fenceEast_ = fenceWith(wood, "east", fence_);
}

} // namespace basalt::server::world::structure
