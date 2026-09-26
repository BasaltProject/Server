#pragma once

#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/common/logging/Logger.hpp>

#include <optional>
#include <string>
#include <vector>

namespace basalt::server::runtime::datapack {

class PackRepository;

/// parity: `DataPackConfig`, the pack ids a world keeps enabled and disabled.
struct DataPackConfig {
    std::vector<std::string> enabled{"vanilla"};
    std::vector<std::string> disabled;

    friend bool operator==(const DataPackConfig&, const DataPackConfig&) = default;
};

/// parity: `WorldDataConfiguration`, the packs plus the feature flags they turned on.
struct WorldDataConfiguration {
    DataPackConfig dataPacks;
    std::vector<wire::common::Identifier> enabledFeatures{wire::common::Identifier::vanilla("vanilla")};

    friend bool operator==(const WorldDataConfiguration&, const WorldDataConfiguration&) = default;

    /// The saved selection, from `data_packs.nbt` beside the world's `level.nbt`. Nothing when the
    /// world has never saved one. Written with vanilla's `level.dat` field names.
    [[nodiscard]] static std::optional<WorldDataConfiguration> read(const std::string& worldRoot);

    bool writeTo(const std::string& worldRoot) const;

    [[nodiscard]] static std::string pathFor(const std::string& worldRoot) { return worldRoot + "/data_packs.nbt"; }
};

/// parity: `MinecraftServer.configurePackRepository`. Rescans, then selects what the configuration
/// enables, adds new packs that may add themselves, drops those asking for features the world has
/// not got, and answers the configuration as it now stands. A new world (`initMode`) may pick up
/// any feature; an existing one only those it was created with. `safeMode` loads vanilla alone.
[[nodiscard]] WorldDataConfiguration configurePackRepository(PackRepository& repository,
                                                             const WorldDataConfiguration& initial, bool initMode,
                                                             bool safeMode, const wire::common::Logger& logger);

/// parity: `MinecraftServer.getSelectedPacks`: the selection, and with `disableInactive` everything
/// else as disabled.
[[nodiscard]] DataPackConfig selectedPacks(const PackRepository& repository, bool disableInactive);

/// Whether every flag of `requested` is in `allowed`. parity: `FeatureFlagSet.isSubsetOf`.
[[nodiscard]] bool featuresAllowed(const std::vector<wire::common::Identifier>& requested,
                                   const std::vector<wire::common::Identifier>& allowed);

/// The flags of `requested` not in `allowed`, joined with ", ". parity: `FeatureFlags.printMissingFlags`.
[[nodiscard]] std::string missingFeatures(const std::vector<wire::common::Identifier>& allowed,
                                          const std::vector<wire::common::Identifier>& requested);

} // namespace basalt::server::runtime::datapack
