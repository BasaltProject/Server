#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/config/GameplayRules.hpp>
#include <basalt/server/runtime/world/BoneMeal.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <map>
#include <string>
#include <string_view>

namespace basalt::server::runtime::world::boneMeal {
namespace {

/// parity: `CropBlock.getBonemealAgeIncrease`.
constexpr std::int32_t kLeastAges = 2;
constexpr std::int32_t kMostAges = 5;

/// The blocks this grows: the crop family and the two stems. parity: they share `performBonemeal`.
constexpr std::array<std::string_view, 7> kByAge{
    "wheat", "carrots", "potatoes", "beetroots", "torchflower_crop", "melon_stem", "pumpkin_stem"};

constexpr std::string_view kShelfMushroom = "shelf_mushroom";

[[nodiscard]] std::string pathOf(std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block == nullptr ? std::string{} : std::string{block->path()};
}

[[nodiscard]] std::optional<std::int32_t> ageOf(std::int32_t stateId) {
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    const auto found = properties.find("age");
    if (found == properties.end()) {
        return std::nullopt;
    }
    std::int32_t value = 0;
    const char* first = found->second.data();
    const char* last = first + found->second.size();
    const std::from_chars_result result = std::from_chars(first, last, value);
    return result.ec == std::errc{} && result.ptr == last ? std::optional{value} : std::nullopt;
}

[[nodiscard]] std::int32_t maxAgeOf(const wire::common::Identifier& block) {
    const registry::blockStateDefinitions::Definition* definition = registry::blockStateDefinitions::of(block);
    return definition == nullptr ? 0 : static_cast<std::int32_t>(definition->valuesOf("age").size()) - 1;
}

[[nodiscard]] bool ruleAllows(const WorldState& world, const std::string& path) {
    const config::GameplayRules::Blocks& rules = world.gameplay().blocks;
    return (path == "cactus" && rules.cactusAffectedByBonemeal) ||
           (path == "sugar_cane" && rules.sugarCaneAffectedByBonemeal) ||
           (path == "nether_wart" && rules.netherWartAffectedByBonemeal);
}

} // namespace

bool grows(const WorldState& world, const std::int32_t stateId) {
    if (grows(stateId)) {
        return true;
    }
    const std::string path = pathOf(stateId);
    if (!ruleAllows(world, path)) {
        return false;
    }
    return path == "nether_wart" ? ageOf(stateId).value_or(3) < 3 : true;
}

bool grows(const std::int32_t stateId) {
    const std::string path = pathOf(stateId);
    // parity: `ShelfMushroomBlock.isValidBonemealTarget`, any age short of its last.
    if (path == kShelfMushroom) {
        return ageOf(stateId).value_or(1) < 1;
    }
    if (std::ranges::find(kByAge, path) == kByAge.end()) {
        return false;
    }
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    const std::optional<std::int32_t> age = ageOf(stateId);
    // parity: `isValidBonemealTarget` for a crop is `!isMaxAge`.
    return block != nullptr && age.has_value() && *age < maxAgeOf(*block);
}

std::optional<BlockChange> apply(WorldState& world, const std::int32_t x, const std::int32_t y, const std::int32_t z,
                                 random::RandomSource& random) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value() || !grows(world, *stateId)) {
        return std::nullopt;
    }
    if (const std::string path = pathOf(*stateId); path == "cactus" || path == "sugar_cane") {
        // parity: Purpur's bone meal reaches the top of the column and adds one block, up to three tall.
        std::int32_t top = y;
        while (pathOf(world.blockState(x, top + 1, z).value_or(0)) == path) {
            ++top;
        }
        std::int32_t bottom = y;
        while (pathOf(world.blockState(x, bottom - 1, z).value_or(0)) == path) {
            --bottom;
        }
        const std::optional<std::int32_t> above = world.blockState(x, top + 1, z);
        if (top - bottom + 1 >= 3 || !above.has_value() || *above != registry::blocks::air().stateId ||
            !world.setBlockState(x, top + 1, z, *stateId)) {
            return std::nullopt;
        }
        return BlockChange{.x = x, .y = top + 1, .z = z, .stateId = *stateId};
    }
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(*stateId);
    const std::optional<std::int32_t> age = ageOf(*stateId);
    if (block == nullptr || !age.has_value()) {
        return std::nullopt;
    }
    // parity: `ShelfMushroomBlock.performBonemeal` always succeeds and grows one age.
    const std::int32_t grown =
        block->path() == kShelfMushroom || block->path() == "nether_wart"
            ? *age + 1
            : std::min(maxAgeOf(*block), *age + random.nextIntBetweenInclusive(kLeastAges, kMostAges));
    const std::optional<std::int32_t> turned =
        registry::blockStateDefinitions::stateIdOf(*block, {{"age", std::to_string(grown)}}, *stateId);
    // parity: `setBlock(pos, state, 2)`.
    if (!turned.has_value() || !world.setBlockState(x, y, z, *turned, WorldState::SideEffects::kSkip)) {
        return std::nullopt;
    }
    return BlockChange{.x = x, .y = y, .z = z, .stateId = *turned};
}

} // namespace basalt::server::runtime::world::boneMeal
