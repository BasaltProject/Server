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

/// The settings that apply to one world, after inheritance has been resolved.
struct WorldSettings {
    std::string difficulty = "easy";
    std::int32_t viewDistance = 10;
    std::int32_t saveIntervalSeconds = 300;

    /// Whether the End's exit portal is open from the start.
    ///
    /// A divergence, and off by default: vanilla only opens it when the dragon dies, and there is
    /// no dragon in this build yet, so an End nobody can leave is what parity actually means today.
    bool openEndExitPortal = false;

    /// The vanilla defaults, which is what a world with no settings of its own gets.
    [[nodiscard]] static WorldSettings vanilla() { return WorldSettings{}; }

    /// Reads whichever of the keys `table` declares, leaving the rest at `inherited`.
    [[nodiscard]] static WorldSettings from(const wire::common::toml::TomlTable* table, const WorldSettings& inherited);

    friend bool operator==(const WorldSettings&, const WorldSettings&) = default;
};

/// How a world's terrain is made.
///
/// `Amplified` is declared but not implemented: vanilla builds it from a second set of density
/// functions under `overworld_amplified` whose splines this build does not port, and this project's
/// own rule is that a parity gap is declared rather than approximated. Selecting it fails at
/// startup with that explanation instead of quietly generating normal terrain under its name.
enum class WorldType : std::uint8_t { Normal, Flat, Void, Amplified };

[[nodiscard]] std::string_view worldTypeName(WorldType type) noexcept;

[[nodiscard]] std::optional<WorldType> worldTypeByName(std::string_view name);

[[nodiscard]] constexpr bool isImplemented(WorldType type) noexcept { return type != WorldType::Amplified; }

/// The vanilla dimension whose rules and generator a declared world uses.
enum class WorldDimension : std::uint8_t { Overworld, Nether, End };

/// What a dimension is, as the registry and the generator need it.
struct DimensionFacts {
    std::string_view configName;
    std::string_view registryPath;
    std::int32_t registryIndex = 0;
    std::int32_t seaLevel = 0;
    world::ChunkHeight chunkHeight = world::kOverworldHeight;
    bool hasSkylight = false;
    bool hasCeiling = false;
    /// parity: `DimensionType.coordinateScale`, the 1:8 that makes a Nether portal land where it
    /// does.
    double coordinateScale = 1.0;
    /// parity: `gameplay/nether_portal_spawns_piglin`, which the overworld's own dimension type
    /// turns on and every other one leaves off.
    bool spawnsPortalPiglins = false;
};

[[nodiscard]] const DimensionFacts& dimensionFacts(WorldDimension dimension) noexcept;

/// The name a config file uses, which is the short one.
[[nodiscard]] std::string_view dimensionName(WorldDimension dimension) noexcept;

/// `minecraft:the_nether` and friends.
[[nodiscard]] std::string dimensionRegistryKey(WorldDimension dimension);

/// Accepts the config name, the registry path and the full registry key, in any case.
[[nodiscard]] std::optional<WorldDimension> dimensionByName(std::string_view name);

/// parity: `Level.canHaveWeather`, which is why it never rains in the Nether or the End.
[[nodiscard]] bool canHaveWeather(WorldDimension dimension) noexcept;

/// Whether time runs there at all.
///
/// parity: `dimension_type.default_clock`. The Overworld and the End name a world clock in 26.2 and
/// the Nether names none, which is why its sky never moves however long anyone stays in it.
[[nodiscard]] constexpr bool hasClock(WorldDimension dimension) noexcept { return dimension != WorldDimension::Nether; }

/// One world as `worlds.toml` declares it. The seed is absent when the operator left it to chance.
///
/// `linkedTo` is what makes several sets of dimensions possible: a Nether or an End names the
/// Overworld whose portals lead to it, so `skyblock_nether` belongs to `skyblock` and not to
/// whichever Nether happened to be declared first. Absent on an Overworld, and on a secondary
/// dimension that left it out, which then belongs to the default world.
struct WorldDeclaration {
    std::string name;
    WorldDimension dimension = WorldDimension::Overworld;
    WorldType type = WorldType::Normal;
    std::optional<std::int64_t> seed;
    bool isDefault = false;
    std::optional<std::string> linkedTo;

    friend bool operator==(const WorldDeclaration&, const WorldDeclaration&) = default;
};

/// `config/worlds.toml`: which worlds to load, plus the settings they inherit.
///
/// Resolution order for a setting, strongest first: `worlds/<name>/settings.toml`, then
/// `[defaults]` here, then the vanilla values.
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

    /// The overworld a world belongs to: itself, what it names, or the default world.
    [[nodiscard]] const WorldDeclaration& overworldOf(const WorldDeclaration& world) const;

    /// The world a portal out of `world` leads to, or nothing when none is declared for it.
    ///
    /// Travel is always between an overworld and one of its own secondary dimensions, which is why
    /// a portal in a Nether comes back to the overworld that Nether belongs to rather than to
    /// whichever one is the default.
    [[nodiscard]] const WorldDeclaration* portalTarget(const WorldDeclaration& world, WorldDimension dimension) const;

    /// The settings for `world`, generating its `settings.toml` if the world directory already
    /// exists and does not have one yet.
    ///
    /// Not generated for a world that has never been created: writing a settings file into an
    /// empty directory would make the directory exist, and an existing directory is how "this
    /// world was already created" is decided elsewhere.
    [[nodiscard]] WorldSettings settingsFor(const WorldDeclaration& world) const;

private:
    std::vector<WorldDeclaration> worlds_;
    WorldSettings defaults_;
    std::string worldsRoot_;
};

} // namespace basalt::server::runtime::config
