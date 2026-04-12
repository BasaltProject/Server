#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/wire/common/Identifier.hpp>

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::world {

/// Where a mob is being spawned, as a variant's `spawn_conditions` ask about it.
struct VariantSpawnFacts {
    std::optional<wire::common::Identifier> biome;
    double moonBrightness = 0.0;
    /// Whether the spot is inside a structure of this holder set (`#tag` or id).
    std::function<bool(std::string_view structures)> inStructure;
};

/// The `spawn_conditions` of every mob variant registry, read from the selected packs at startup.
///
/// parity: `PriorityProvider.pick` over the registry in id order: each selector unpacked, the
/// highest priority whose condition holds kept, one of those drawn with `nextInt`.
namespace variantSpawnConditions {

/// Reads the packs. Before this, `pick` answers nothing and callers keep their built-in tables.
void install(const std::function<std::vector<std::string>(std::string_view prefix)>& entriesUnder,
             const std::function<std::optional<std::string>(std::string_view name)>& readText);

/// Where in `entries(registry)` the variant a spawn draws sits, or nothing when the registry was
/// not installed or no entry matched (vanilla then keeps the default variant).
[[nodiscard]] std::optional<std::size_t> pick(std::string_view registry, const VariantSpawnFacts& facts,
                                              random::RandomSource& random);

/// A registry's entries in id order, the variant and sound variant ones; empty before `install`.
[[nodiscard]] const std::vector<wire::common::Identifier>& entries(std::string_view registry);

/// parity: `Registry.getRandom`, what the sound variants are drawn with.
[[nodiscard]] std::optional<wire::common::Identifier> randomEntry(std::string_view registry,
                                                                  random::RandomSource& random);

/// Whether `pick` has data to answer from.
[[nodiscard]] bool installed();

} // namespace variantSpawnConditions

} // namespace basalt::server::runtime::world
