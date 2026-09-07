#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/world/GrowthSupport.hpp>

#include <charconv>
#include <cstdint>
#include <string>

namespace basalt::server::runtime::world::growthSupport {

void set(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId,
         std::vector<BlockChange>& changes) {
    if (!world.setBlockState(x, y, z, stateId)) {
        return;
    }
    changes.push_back(BlockChange{.x = x, .y = y, .z = z, .stateId = stateId});
}

std::int32_t offsetX(std::string_view facing) {
    if (facing == "west") {
        return -1;
    }
    return facing == "east" ? 1 : 0;
}

std::int32_t offsetY(std::string_view facing) {
    if (facing == "down") {
        return -1;
    }
    return facing == "up" ? 1 : 0;
}

std::int32_t offsetZ(std::string_view facing) {
    if (facing == "north") {
        return -1;
    }
    return facing == "south" ? 1 : 0;
}

std::optional<std::int32_t> ageOf(std::int32_t stateId) {
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    const auto found = properties.find("age");
    if (found == properties.end()) {
        return std::nullopt;
    }
    std::int32_t age = 0;
    const char* first = found->second.data();
    const char* last = first + found->second.size();
    // Whole-string, as everywhere else a property is read as a number: an age is always digits, so
    // this only ever matters for a state written by something that is not this server.
    const std::from_chars_result result = std::from_chars(first, last, age);
    if (result.ec != std::errc{} || result.ptr != last) {
        return std::nullopt;
    }
    return age;
}

void grow(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId, std::int32_t maxAge,
          std::vector<BlockChange>& changes) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    if (block == nullptr) {
        return;
    }
    const std::optional<std::int32_t> age = ageOf(stateId);
    if (!age.has_value() || *age >= maxAge) {
        return;
    }
    const std::optional<std::int32_t> older =
        registry::blockStateDefinitions::stateIdOf(*block, {{"age", std::to_string(*age + 1)}}, stateId);
    if (!older.has_value()) {
        return;
    }
    set(world, x, y, z, *older, changes);
}

} // namespace basalt::server::runtime::world::growthSupport
