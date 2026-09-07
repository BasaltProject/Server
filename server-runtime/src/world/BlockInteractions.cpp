#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/world/BlockInteractions.hpp>

#include <array>
#include <charconv>
#include <cstdint>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace basalt::server::runtime::world::blockInteractions {
namespace {

using Properties = std::map<std::string, std::string>;

/// parity: `BlockSetType.canOpenByHand`, which is false for exactly one family that exists as a
/// block: iron. (`BlockSetType.GOLD` says the same, but no gold door was ever registered.)
constexpr std::string_view kIron = "iron";

/// The button families whose set uses the stone sounds and the shorter press.
constexpr std::array<std::string_view, 5> kStoneButtons{"stone", "polished_blackstone", "copper", "gold", "iron"};

/// parity: the sound sets, which are per wood rather than per block.
constexpr std::array<std::pair<std::string_view, std::string_view>, 6> kSoundSets{{
    {"bamboo", "bamboo_wood"},
    {"cherry", "cherry_wood"},
    {"crimson", "nether_wood"},
    {"warped", "nether_wood"},
    {"copper", "copper"},
    {"iron", "iron"},
}};

/// The weathering prefixes a copper block wears, which its sound set does not.
constexpr std::array<std::string_view, 4> kWeathering{"waxed_", "exposed_", "weathered_", "oxidized_"};

enum class Toggle { Door, Trapdoor, FenceGate, Lever, Button };

[[nodiscard]] std::string_view propertyOf(Toggle kind) {
    return kind == Toggle::Lever || kind == Toggle::Button ? "powered" : "open";
}

[[nodiscard]] std::string valueOf(const Properties& properties, const std::string& key) {
    const auto found = properties.find(key);
    return found == properties.end() ? std::string{} : found->second;
}

/// parity: `RepeaterBlock.DELAY` starts at one, which is what a repeater without a readable delay
/// reads as. `from_chars` leaves the value alone when it fails, so the initializer is that default.
[[nodiscard]] std::int32_t delayOf(const Properties& properties) {
    const std::string text = valueOf(properties, "delay");
    std::int32_t value = 1;
    std::from_chars(text.data(), text.data() + text.size(), value);
    return value;
}

[[nodiscard]] bool startsWithAny(std::string_view path, std::span<const std::string_view> prefixes) {
    for (const std::string_view prefix : prefixes) {
        if (path.starts_with(prefix)) {
            return true;
        }
    }
    return false;
}

/// parity: every weathering stage of a copper door shares `BlockSetType.COPPER`, so an exposed or
/// waxed one sounds like a plain copper one rather than falling back to wood.
[[nodiscard]] std::string_view familyOf(std::string_view path) {
    for (const std::string_view prefix : kWeathering) {
        if (path.starts_with(prefix)) {
            path.remove_prefix(prefix.size());
        }
    }
    return path;
}

[[nodiscard]] std::optional<Toggle> toggleOf(std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    if (block == nullptr) {
        return std::nullopt;
    }
    const std::string_view path = block->path();
    // parity: an iron door or trapdoor only moves for redstone, never for a hand.
    if (path == "lever") {
        return Toggle::Lever;
    }
    if (path.ends_with("_button")) {
        return Toggle::Button;
    }
    if (path.starts_with(kIron)) {
        return std::nullopt;
    }
    if (path.ends_with("_door")) {
        return Toggle::Door;
    }
    if (path.ends_with("_trapdoor")) {
        return Toggle::Trapdoor;
    }
    if (path.ends_with("_fence_gate")) {
        return Toggle::FenceGate;
    }
    return std::nullopt;
}

/// parity: `BlockSetType`'s button sounds, which do not follow its other sounds: a copper, gold or
/// iron button clicks like stone, and only the four wood families have their own.
[[nodiscard]] std::string_view buttonSoundSet(const wire::common::Identifier& block) {
    const std::string_view path = block.path();
    if (startsWithAny(path, kStoneButtons)) {
        return "stone";
    }
    if (path.starts_with("bamboo")) {
        return "bamboo_wood";
    }
    if (path.starts_with("cherry")) {
        return "cherry_wood";
    }
    if (path.starts_with("crimson") || path.starts_with("warped")) {
        return "nether_wood";
    }
    return "wooden";
}

[[nodiscard]] std::string soundFor(const wire::common::Identifier& block, Toggle kind, bool open) {
    if (kind == Toggle::Lever) {
        return "minecraft:block.lever.click";
    }
    if (kind == Toggle::Button) {
        return std::string{"minecraft:block."}.append(buttonSoundSet(block)).append("_button.click_") +
               (open ? "on" : "off");
    }
    const std::string_view action = open ? "open" : "close";
    const std::string_view family = familyOf(block.path());
    std::string_view set;
    for (const auto& [prefix, name] : kSoundSets) {
        if (family.starts_with(prefix)) {
            set = name;
            break;
        }
    }
    if (kind == Toggle::FenceGate) {
        // parity: a plain fence gate has no wood in its sound name, unlike the other two.
        return set.empty() ? std::string{"minecraft:block.fence_gate."}.append(action)
                           : std::string{"minecraft:block."}.append(set).append("_fence_gate.").append(action);
    }
    const std::string_view tail = kind == Toggle::Trapdoor ? "_trapdoor." : "_door.";
    return std::string{"minecraft:block."}.append(set.empty() ? "wooden" : set).append(tail).append(action);
}

} // namespace

bool isToggleable(std::int32_t stateId) { return toggleOf(stateId).has_value(); }

bool isCycleable(std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    if (block == nullptr) {
        return false;
    }
    const std::string_view path = block->path();
    return path == "repeater" || path == "comparator";
}

std::optional<BlockToggle> toggle(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return std::nullopt;
    }
    const std::optional<Toggle> kind = toggleOf(*stateId);
    if (!kind.has_value()) {
        return std::nullopt;
    }
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(*stateId);
    if (block == nullptr) {
        return std::nullopt;
    }
    const std::string property{propertyOf(*kind)};
    const Properties properties = registry::blockStateDefinitions::propertiesOf(*stateId);
    const bool open = valueOf(properties, property) != "true";
    // parity: `ButtonBlock.useWithoutItem` does nothing to a button already down. Its own scheduled
    // tick is what lets it back up.
    if (*kind == Toggle::Button && !open) {
        return std::nullopt;
    }

    std::vector<BlockChange> changes;
    const auto flip = [&](std::int32_t atX, std::int32_t atY, std::int32_t atZ) {
        const std::optional<std::int32_t> at = world.blockState(atX, atY, atZ);
        if (!at.has_value()) {
            return;
        }
        const wire::common::Identifier* there = registry::blockStateDefinitions::blockOf(*at);
        if (there == nullptr || *there != *block) {
            return;
        }
        const std::optional<std::int32_t> flipped =
            registry::blockStateDefinitions::stateIdOf(*block, Properties{{property, open ? "true" : "false"}}, *at);
        if (!flipped.has_value() || !world.setBlockState(atX, atY, atZ, *flipped)) {
            return;
        }
        changes.push_back(BlockChange{.x = atX, .y = atY, .z = atZ, .stateId = *flipped});
    };

    flip(x, y, z);
    if (*kind == Toggle::Door) {
        // The other half, whichever way round this one is.
        const bool upper = valueOf(properties, "half") == "upper";
        flip(x, upper ? y - 1 : y + 1, z);
    }
    if (changes.empty()) {
        return std::nullopt;
    }
    return BlockToggle{.changes = std::move(changes), .sound = soundFor(*block, *kind, open)};
}

std::optional<BlockCycle> cycle(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return std::nullopt;
    }
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(*stateId);
    if (block == nullptr) {
        return std::nullopt;
    }
    const Properties properties = registry::blockStateDefinitions::propertiesOf(*stateId);
    const std::string_view path = block->path();
    if (path == "repeater") {
        const std::int32_t delay = delayOf(properties);
        const std::int32_t next = delay >= kMaxDelay ? 1 : delay + 1;
        const std::optional<std::int32_t> turned =
            registry::blockStateDefinitions::stateIdOf(*block, Properties{{"delay", std::to_string(next)}}, *stateId);
        if (!turned.has_value() || !world.setBlockState(x, y, z, *turned)) {
            return std::nullopt;
        }
        return BlockCycle{.stateId = *turned, .sound = std::nullopt};
    }
    if (path == "comparator") {
        const bool subtracting = valueOf(properties, "mode") == "subtract";
        const std::optional<std::int32_t> turned = registry::blockStateDefinitions::stateIdOf(
            *block, Properties{{"mode", subtracting ? "compare" : "subtract"}}, *stateId);
        if (!turned.has_value() || !world.setBlockState(x, y, z, *turned)) {
            return std::nullopt;
        }
        // parity: the pitch says which way it went, and it is quieter than a lever.
        return BlockCycle{.stateId = *turned,
                          .sound = std::string{"block.comparator.click"},
                          .pitch = subtracting ? kComparePitch : kSubtractPitch,
                          .volume = kComparatorVolume};
    }
    return std::nullopt;
}

std::optional<std::int32_t> pressedTicks(std::int32_t stateId) {
    if (toggleOf(stateId) != Toggle::Button) {
        return std::nullopt;
    }
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    if (block == nullptr) {
        return std::nullopt;
    }
    return startsWithAny(block->path(), kStoneButtons) ? kStonePressTicks : kWoodPressTicks;
}

std::optional<BlockRelease> release(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value() || toggleOf(*stateId) != Toggle::Button) {
        return std::nullopt;
    }
    const Properties properties = registry::blockStateDefinitions::propertiesOf(*stateId);
    if (valueOf(properties, "powered") != "true") {
        return std::nullopt;
    }
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(*stateId);
    if (block == nullptr) {
        return std::nullopt;
    }
    const std::optional<std::int32_t> released =
        registry::blockStateDefinitions::stateIdOf(*block, Properties{{"powered", "false"}}, *stateId);
    if (!released.has_value() || !world.setBlockState(x, y, z, *released)) {
        return std::nullopt;
    }
    return BlockRelease{.stateId = *released, .sound = soundFor(*block, Toggle::Button, false)};
}

std::optional<std::string> redstoneSound(std::int32_t stateId, bool open) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    if (block == nullptr) {
        return std::nullopt;
    }
    const std::string_view path = block->path();
    Toggle kind = Toggle::Door;
    if (path.ends_with("_trapdoor")) {
        kind = Toggle::Trapdoor;
    } else if (path.ends_with("_fence_gate")) {
        kind = Toggle::FenceGate;
    } else if (!path.ends_with("_door")) {
        return std::nullopt;
    }
    return soundFor(*block, kind, open);
}

} // namespace basalt::server::runtime::world::blockInteractions
