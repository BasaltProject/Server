#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/wire/common/Identifier.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::world {

struct VariantSpawnFacts {
    std::optional<wire::common::Identifier> biome;
    double moonBrightness = 0.0;
    /// parity: `StructureManager.getStructureWithPieceAt`.
    std::function<bool(const wire::common::Identifier& structure)> structureAt;
};

struct MobSpawnContext;

[[nodiscard]] VariantSpawnFacts spawnFactsOf(const MobSpawnContext& context, std::int32_t blockX, std::int32_t blockY,
                                             std::int32_t blockZ);

/// parity: `PriorityProvider.pick`.
namespace variantSpawnConditions {

/// Before this, `pick` answers nothing and callers keep their built-in tables.
void install(const std::function<std::vector<std::string>(std::string_view prefix)>& entriesUnder,
             const std::function<std::optional<std::string>(std::string_view name)>& readText);

/// Nothing when the registry was not installed or no entry matched (vanilla then keeps the default variant).
[[nodiscard]] std::optional<std::size_t> pick(std::string_view registry, const VariantSpawnFacts& facts,
                                              random::RandomSource& random);

[[nodiscard]] const std::vector<wire::common::Identifier>& entries(std::string_view registry);

/// parity: `Registry.getRandom`, what the sound variants are drawn with.
[[nodiscard]] std::optional<wire::common::Identifier> randomEntry(std::string_view registry,
                                                                  random::RandomSource& random);

[[nodiscard]] bool installed();

} // namespace variantSpawnConditions

} // namespace basalt::server::runtime::world
