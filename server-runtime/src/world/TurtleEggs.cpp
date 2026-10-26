#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/TurtleEggs.hpp>

#include <map>
#include <string>

namespace basalt::server::runtime::world::turtleEggs {

std::optional<BlockChange> trample(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                   std::int32_t randomness, random::RandomSource& random) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    const wire::common::Identifier* block =
        stateId.has_value() ? registry::blockStateDefinitions::blockOf(*stateId) : nullptr;
    if (block == nullptr || block->path() != "turtle_egg" || random.nextInt(randomness) != 0) {
        return std::nullopt;
    }
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(*stateId);
    const auto eggs = properties.find("eggs");
    const std::int32_t count = eggs == properties.end() ? 1 : std::stoi(eggs->second);
    std::int32_t next = registry::blocks::air().stateId;
    if (count > 1) {
        const std::optional<std::int32_t> fewer =
            registry::blockStateDefinitions::stateIdOf(*block, {{"eggs", std::to_string(count - 1)}}, *stateId);
        if (!fewer.has_value()) {
            return std::nullopt;
        }
        next = *fewer;
    }
    if (!world.setBlockState(x, y, z, next)) {
        return std::nullopt;
    }
    return BlockChange{.x = x, .y = y, .z = z, .stateId = next};
}

} // namespace basalt::server::runtime::world::turtleEggs
