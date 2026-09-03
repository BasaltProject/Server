#pragma once

#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::world {

/// parity: `Panda.Gene`, whose two recessive genes are what makes a brown or weak panda rare.
enum class PandaGene : std::uint8_t { Normal, Lazy, Worried, Playful, Brown, Weak, Aggressive };

[[nodiscard]] std::string_view pandaGeneName(PandaGene gene) noexcept;

[[nodiscard]] std::int32_t pandaGeneId(PandaGene gene) noexcept;

/// parity: only `brown` and `weak` are recessive, which is why a panda needs both parents to show
/// one.
[[nodiscard]] bool pandaGeneRecessive(PandaGene gene) noexcept;

[[nodiscard]] std::optional<PandaGene> pandaGeneByName(std::string_view name);

/// Immutable lookup tables used by natural mob spawn finalizers.
namespace mobSpawnVariantData {

[[nodiscard]] const std::vector<std::string>& chickenSoundVariants();
[[nodiscard]] const std::vector<std::string>& cowSoundVariants();
[[nodiscard]] const std::vector<std::string>& pigSoundVariants();
[[nodiscard]] const std::vector<std::string>& wolfVariants();
[[nodiscard]] const std::vector<std::string>& wolfSoundVariants();

/// parity: `TropicalFish.COMMON_VARIANTS`, packed the way the entity's own variant field is.
[[nodiscard]] const std::vector<std::int32_t>& tropicalCommonVariants();

[[nodiscard]] const std::set<std::string, std::less<>>& whiteRabbitBiomes();
[[nodiscard]] const std::set<std::string, std::less<>>& warmFarmBiomes();
[[nodiscard]] const std::set<std::string, std::less<>>& coldFarmBiomes();
[[nodiscard]] const std::set<std::string, std::less<>>& warmFrogBiomes();
[[nodiscard]] const std::set<std::string, std::less<>>& coldFrogBiomes();
[[nodiscard]] const std::set<std::string, std::less<>>& savannaBiomes();
[[nodiscard]] const std::set<std::string, std::less<>>& jungleBiomes();
[[nodiscard]] const std::set<std::string, std::less<>>& badlandsBiomes();

} // namespace mobSpawnVariantData

} // namespace basalt::server::runtime::world
