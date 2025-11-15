#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/world/SnowyBlocks.hpp>

#include <algorithm>
#include <array>
#include <string>

namespace basalt::server::runtime::world::snowyBlocks {
namespace {

constexpr std::array kSnow = {std::string_view{"snow"}, std::string_view{"snow_block"},
                              std::string_view{"powder_snow"}};

} // namespace

bool isSnow(std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block != nullptr && block->namespaceName() == wire::common::Identifier::kVanillaNamespace &&
           std::ranges::find(kSnow, block->path()) != kSnow.end();
}

bool wearsSnow(std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    if (block == nullptr) {
        return false;
    }
    const registry::blockStateDefinitions::Definition* definition = registry::blockStateDefinitions::of(*block);
    return definition != nullptr && !definition->valuesOf("snowy").empty();
}

std::optional<std::int32_t> updated(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    // The `wearsSnow` half is a fast path: a block without the property answers with itself below
    // and is dropped by the comparison there. It is here because most blocks are that block.
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value() || !wearsSnow(*stateId)) {
        return std::nullopt;
    }
    const std::optional<std::int32_t> above = world.blockState(x, y + 1, z);
    if (!above.has_value()) {
        return std::nullopt;
    }
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(*stateId);
    if (block == nullptr) {
        return std::nullopt;
    }
    const std::optional<std::int32_t> wanted =
        registry::blockStateDefinitions::stateIdOf(*block, {{"snowy", isSnow(*above) ? "true" : "false"}}, *stateId);
    if (!wanted.has_value() || *wanted == *stateId) {
        return std::nullopt;
    }
    return wanted;
}

} // namespace basalt::server::runtime::world::snowyBlocks
