#pragma once

#include <basalt/metamorph/datafix/DataFixer.hpp>

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::plugins::datafix {

/// Unnamed NBT after a fix, or the reason there is none (unknown type, unreadable NBT, a step this build cannot run).
struct Outcome {
    std::optional<std::vector<std::uint8_t>> nbt;
    std::string error;
};

/// A vanilla step as the native fixer lists it.
struct Step {
    std::int32_t version = 0;
    std::int32_t subVersion = 0;
    std::string name;
};

/// Upgrades unnamed NBT of a data type (a domain name or a saved data file) between world versions.
[[nodiscard]] Outcome upgrade(std::string_view type, std::span<const std::uint8_t> nbt, std::int32_t fromVersion,
                              std::int32_t toVersion);

/// Runs the one vanilla step `name` registered at `versionKey` (`version * 10 + sub`) on the NBT.
[[nodiscard]] Outcome applyStep(std::string_view type, std::string_view name, std::int32_t versionKey,
                                std::span<const std::uint8_t> nbt);

/// The first step in the range that the native fixer cannot run for the type, or empty.
[[nodiscard]] std::string missing(std::string_view type, std::int32_t fromVersion, std::int32_t toVersion);

[[nodiscard]] std::vector<Step> steps(std::string_view type);

[[nodiscard]] bool supports(std::string_view type);

/// The 1.13 name and properties of a pre-1.13 `block << 4 | data` id (air when unmapped).
[[nodiscard]] metamorph::datafix::LegacyBlockState legacyState(std::int32_t id);

[[nodiscard]] std::vector<std::string> types();

} // namespace basalt::server::plugins::datafix
