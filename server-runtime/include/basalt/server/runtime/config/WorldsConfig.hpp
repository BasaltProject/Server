#pragma once

#include <basalt/server/runtime/config/ConfigDirectory.hpp>
#include <basalt/server/world/ChunkHeight.hpp>
#include <basalt/wire/common/logging/Logger.hpp>

#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::config {

struct WorldSettings {
    std::string difficulty = "easy";
    std::int32_t viewDistance = 10;
    std::int32_t simulationDistance = 10;
    std::int32_t saveIntervalSeconds = 300;

    bool openEndExitPortal = false;
    bool hardcore = false;

    bool lootRefill = false;
    std::int32_t lootRefillMinSeconds = 43200;
    std::int32_t lootRefillMaxSeconds = 172800;
    std::int32_t lootMaxRefills = -1;
    std::int32_t lootRelootSeconds = -1;

    [[nodiscard]] static WorldSettings vanilla() { return WorldSettings{}; }

    [[nodiscard]] static WorldSettings from(const wire::common::toml::TomlTable* table, const WorldSettings& inherited);

    friend bool operator==(const WorldSettings&, const WorldSettings&) = default;
};

enum class WorldType : std::uint8_t { Normal, Flat, Void, Amplified, LargeBiomes, Debug };

[[nodiscard]] std::string_view worldTypeName(WorldType type) noexcept;

[[nodiscard]] std::optional<WorldType> worldTypeByName(std::string_view name);

enum class WorldDimension : std::uint8_t { Overworld, Nether, End };

struct DimensionFacts {
    std::string_view configName;
    std::string_view registryPath;
    std::int32_t registryIndex = 0;
    std::int32_t seaLevel = 0;
    server::world::ChunkHeight chunkHeight = server::world::kOverworldHeight;
    bool hasSkylight = false;
    bool hasCeiling = false;
    /// parity: `DimensionType.coordinateScale`.
    double coordinateScale = 1.0;
    /// parity: `gameplay/nether_portal_spawns_piglin`.
    bool spawnsPortalPiglins = false;
    /// parity: `EnvironmentAttributes.SKY_LIGHT_LEVEL`.
    float skyLightLevel = 15.0F;
    bool hasDayTimeline = false;
    /// parity: `DimensionType.ambientLight`.
    float ambientLight = 0.0F;
};

[[nodiscard]] const DimensionFacts& dimensionFacts(WorldDimension dimension) noexcept;

[[nodiscard]] std::string_view dimensionName(WorldDimension dimension) noexcept;

[[nodiscard]] std::string dimensionRegistryKey(WorldDimension dimension);

[[nodiscard]] std::optional<WorldDimension> dimensionByName(std::string_view name);

/// parity: `Level.canHaveWeather`, which is why it never rains in the Nether or the End.
[[nodiscard]] bool canHaveWeather(WorldDimension dimension) noexcept;

/// parity: `dimension_type.default_clock`.
[[nodiscard]] constexpr bool hasClock(WorldDimension dimension) noexcept { return dimension != WorldDimension::Nether; }

struct WorldDeclaration {
    std::string name;
    WorldDimension dimension = WorldDimension::Overworld;
    WorldType type = WorldType::Normal;
    std::optional<std::int64_t> seed;
    bool isDefault = false;
    std::optional<std::string> linkedTo;

    friend bool operator==(const WorldDeclaration&, const WorldDeclaration&) = default;
};

class WorldsConfig {
public:
    static constexpr std::string_view kFileName = "worlds.toml";
    static constexpr std::string_view kDefaultWorldsRoot = "worlds";

    WorldsConfig(std::vector<WorldDeclaration> worlds, WorldSettings defaults, std::string worldsRoot)
        : worlds_(std::move(worlds)), defaults_(std::move(defaults)), worldsRoot_(std::move(worldsRoot)) {}

    [[nodiscard]] static std::expected<WorldsConfig, std::string>
    load(const ConfigDirectory& directory = ConfigDirectory{},
         const std::string& worldsRoot = std::string{kDefaultWorldsRoot},
         const wire::common::Logger& logger = wire::common::Logger{"Config"});

    [[nodiscard]] const std::vector<WorldDeclaration>& worlds() const noexcept { return worlds_; }

    [[nodiscard]] const WorldDeclaration& defaultWorld() const;

    [[nodiscard]] const WorldDeclaration& overworldOf(const WorldDeclaration& world) const;

    [[nodiscard]] const WorldDeclaration* portalTarget(const WorldDeclaration& world, WorldDimension dimension) const;

    [[nodiscard]] WorldSettings settingsFor(const WorldDeclaration& world) const;

private:
    std::vector<WorldDeclaration> worlds_;
    WorldSettings defaults_;
    std::string worldsRoot_;
};

} // namespace basalt::server::runtime::config
