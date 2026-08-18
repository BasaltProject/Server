#include <basalt/server/runtime/config/ConfigDefaults.hpp>
#include <basalt/server/runtime/config/GameplayRules.hpp>
#include <basalt/server/runtime/config/WorldsConfig.hpp>
#include <basalt/wire/common/placeholder/Placeholders.hpp>
#include <basalt/wire/io/FileIO.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <utility>

namespace basalt::server::runtime::config {
namespace {

using wire::common::placeholder::lowercaseAscii;
using wire::common::toml::TomlTable;

constexpr std::array<WorldType, 6> kWorldTypes{WorldType::Normal,    WorldType::Flat,        WorldType::Void,
                                               WorldType::Amplified, WorldType::LargeBiomes, WorldType::Debug};
constexpr std::array<WorldDimension, 3> kDimensions{WorldDimension::Overworld, WorldDimension::Nether,
                                                    WorldDimension::End};

[[nodiscard]] std::string everyWorldTypeName() {
    std::string names;
    for (const WorldType type : kWorldTypes) {
        if (!names.empty()) {
            names += ", ";
        }
        names += worldTypeName(type);
    }
    return names;
}

[[nodiscard]] std::string everyDimensionName() {
    std::string names;
    for (const WorldDimension dimension : kDimensions) {
        if (!names.empty()) {
            names += ", ";
        }
        names += dimensionName(dimension);
    }
    return names;
}

} // namespace

WorldSettings WorldSettings::from(const TomlTable* table, const WorldSettings& inherited) {
    if (table == nullptr) {
        return inherited;
    }
    WorldSettings settings = inherited;
    settings.difficulty = std::string{table->string("difficulty").value_or(inherited.difficulty)};
    settings.viewDistance = static_cast<std::int32_t>(table->integer("view-distance").value_or(inherited.viewDistance));
    // parity: `PlayerList.setSimulationDistance`, clamped as `setViewDistance` is.
    settings.simulationDistance = static_cast<std::int32_t>(
        std::clamp<std::int64_t>(table->integer("simulation-distance").value_or(inherited.simulationDistance), 2, 32));
    settings.saveIntervalSeconds = static_cast<std::int32_t>(
        std::max<std::int64_t>(table->integer("save-interval-seconds").value_or(inherited.saveIntervalSeconds), 0));
    settings.openEndExitPortal = table->boolean("open-end-exit-portal").value_or(inherited.openEndExitPortal);
    settings.hardcore = table->boolean("hardcore").value_or(inherited.hardcore);
    settings.lootRefill = table->boolean("loot-refill").value_or(inherited.lootRefill);
    const auto whole = [table](std::string_view key, std::int32_t fallback, std::int64_t least) {
        return static_cast<std::int32_t>(std::max<std::int64_t>(table->integer(key).value_or(fallback), least));
    };
    settings.lootRefillMinSeconds = whole("loot-refill-min-seconds", inherited.lootRefillMinSeconds, 0);
    settings.lootRefillMaxSeconds =
        std::max(whole("loot-refill-max-seconds", inherited.lootRefillMaxSeconds, 0), settings.lootRefillMinSeconds);
    settings.lootMaxRefills = whole("loot-max-refills", inherited.lootMaxRefills, -1);
    settings.lootRelootSeconds = whole("loot-reloot-seconds", inherited.lootRelootSeconds, -1);
    settings.rules = WorldRules::from(table, inherited.rules);
    const GameplayRules inheritedRules = inherited.gameplay.ptr != nullptr ? *inherited.gameplay.ptr : GameplayRules{};
    settings.gameplay =
        std::make_shared<const GameplayRules>(GameplayRules::from(table->table("gameplay"), inheritedRules));
    return settings;
}

std::string_view worldTypeName(WorldType type) noexcept {
    switch (type) {
    case WorldType::Flat:
        return "flat";
    case WorldType::Void:
        return "void";
    case WorldType::Amplified:
        return "amplified";
    case WorldType::LargeBiomes:
        return "large_biomes";
    case WorldType::Debug:
        return "debug";
    case WorldType::Normal:
    default:
        return "normal";
    }
}

std::optional<WorldType> worldTypeByName(std::string_view name) {
    const std::string lowercase = lowercaseAscii(name);
    for (const WorldType type : kWorldTypes) {
        if (lowercase == worldTypeName(type)) {
            return type;
        }
    }
    return std::nullopt;
}

const DimensionFacts& dimensionFacts(WorldDimension dimension) noexcept {
    static constexpr DimensionFacts kOverworld{.configName = "overworld",
                                               .registryPath = "overworld",
                                               .registryIndex = 0,
                                               .seaLevel = 63,
                                               .chunkHeight = world::kOverworldHeight,
                                               .hasSkylight = true,
                                               .hasCeiling = false,
                                               .coordinateScale = 1.0,
                                               .spawnsPortalPiglins = true,
                                               .skyLightLevel = 15.0F,
                                               .hasDayTimeline = true};
    static constexpr DimensionFacts kNether{.configName = "nether",
                                            .registryPath = "the_nether",
                                            .registryIndex = 1,
                                            .seaLevel = 32,
                                            .chunkHeight = world::kNetherHeight,
                                            .hasSkylight = false,
                                            .hasCeiling = true,
                                            .coordinateScale = 8.0,
                                            .spawnsPortalPiglins = false,
                                            .skyLightLevel = 4.0F,
                                            .hasDayTimeline = false,
                                            .ambientLight = 0.1F};
    static constexpr DimensionFacts kEnd{.configName = "end",
                                         .registryPath = "the_end",
                                         .registryIndex = 2,
                                         .seaLevel = 0,
                                         .chunkHeight = world::kEndHeight,
                                         .hasSkylight = true,
                                         .hasCeiling = false,
                                         .coordinateScale = 1.0,
                                         .spawnsPortalPiglins = false,
                                         .skyLightLevel = 15.0F,
                                         .hasDayTimeline = false,
                                         .ambientLight = 0.25F};
    switch (dimension) {
    case WorldDimension::Nether:
        return kNether;
    case WorldDimension::End:
        return kEnd;
    case WorldDimension::Overworld:
    default:
        return kOverworld;
    }
}

std::string_view dimensionName(WorldDimension dimension) noexcept { return dimensionFacts(dimension).configName; }

std::string dimensionRegistryKey(WorldDimension dimension) {
    return "minecraft:" + std::string{dimensionFacts(dimension).registryPath};
}

std::optional<WorldDimension> dimensionByName(std::string_view name) {
    const std::string lowercase = lowercaseAscii(name);
    for (const WorldDimension dimension : kDimensions) {
        const DimensionFacts& facts = dimensionFacts(dimension);
        if (lowercase == facts.configName || lowercase == facts.registryPath ||
            lowercase == dimensionRegistryKey(dimension)) {
            return dimension;
        }
    }
    return std::nullopt;
}

bool canHaveWeather(WorldDimension dimension) noexcept {
    const DimensionFacts& facts = dimensionFacts(dimension);
    return facts.hasSkylight && !facts.hasCeiling && dimension != WorldDimension::End;
}

const WorldDeclaration& WorldsConfig::defaultWorld() const {
    for (const WorldDeclaration& world : worlds_) {
        if (world.isDefault) {
            return world;
        }
    }
    return worlds_.front();
}

const WorldDeclaration& WorldsConfig::overworldOf(const WorldDeclaration& world) const {
    if (world.dimension == WorldDimension::Overworld) {
        return world;
    }
    const std::string& name = world.linkedTo.has_value() ? *world.linkedTo : defaultWorld().name;
    for (const WorldDeclaration& candidate : worlds_) {
        if (candidate.name == name) {
            return candidate;
        }
    }
    // `load` refuses links to undeclared worlds, so this covers a configuration built in code.
    return defaultWorld();
}

const WorldDeclaration* WorldsConfig::portalTarget(const WorldDeclaration& world, WorldDimension dimension) const {
    const WorldDeclaration& overworld = overworldOf(world);
    if (dimension == WorldDimension::Overworld) {
        return &overworld;
    }
    for (const WorldDeclaration& candidate : worlds_) {
        const std::string& linked = candidate.linkedTo.has_value() ? *candidate.linkedTo : defaultWorld().name;
        if (candidate.dimension == dimension && linked == overworld.name) {
            return &candidate;
        }
    }
    return nullptr;
}

WorldSettings WorldsConfig::settingsFor(const WorldDeclaration& world) const {
    const std::string directory = worldsRoot_ + "/" + world.name;
    const std::string path = directory + "/settings.toml";
    const std::optional<std::string> text = wire::io::fileIO::readText(path);
    WorldSettings settings = defaults_;
    settings.mobs = loadWorldMobs(directory, defaults_.mobs);
    if (!text.has_value()) {
        // Asked rather than left to the write failing: what is being decided is whether the world exists.
        if (wire::io::fileIO::isDirectory(directory)) {
            wire::io::fileIO::writeText(path, std::string{kWorldSettingsDefaultToml} + worldRulesTemplate(false) +
                                                  std::string{kGameplayWorldSettingsDefaultToml});
        }
        return settings;
    }
    const std::expected<TomlTable, wire::common::toml::TomlError> parsed = wire::common::toml::parse(*text);
    // An unparsable settings file loses one world's preferences, not the server's configuration.
    return parsed.has_value() ? WorldSettings::from(&*parsed, settings) : settings;
}

namespace {

// Refuses links that cannot mean one thing, instead of silently sending every portal to one of two Nethers.
[[nodiscard]] std::optional<std::string> validateLinks(const std::vector<WorldDeclaration>& worlds,
                                                       const std::string& path) {
    const auto byName = [&worlds](std::string_view name) -> const WorldDeclaration* {
        for (const WorldDeclaration& world : worlds) {
            if (world.name == name) {
                return &world;
            }
        }
        return nullptr;
    };
    const WorldDeclaration* defaultWorld = &worlds.front();
    for (const WorldDeclaration& world : worlds) {
        if (world.isDefault) {
            defaultWorld = &world;
            break;
        }
    }

    std::vector<std::pair<std::pair<std::string, WorldDimension>, std::string>> claimed;
    for (const WorldDeclaration& world : worlds) {
        if (world.dimension == WorldDimension::Overworld) {
            continue;
        }
        const std::string targetName = world.linkedTo.value_or(defaultWorld->name);
        const WorldDeclaration* target = byName(targetName);
        if (target == nullptr) {
            return "World '" + world.name + "' is linked to '" + targetName + "', which " + path + " does not declare.";
        }
        if (target->dimension != WorldDimension::Overworld) {
            return "World '" + world.name + "' is linked to '" + targetName + "', which is a " +
                   std::string{dimensionName(target->dimension)} + " rather than an overworld.";
        }
        const std::pair<std::string, WorldDimension> claim{targetName, world.dimension};
        const auto already =
            std::ranges::find_if(claimed, [&claim](const auto& entry) { return entry.first == claim; });
        if (already != claimed.end()) {
            return "Worlds '" + already->second + "' and '" + world.name + "' are both the " +
                   std::string{dimensionName(world.dimension)} + " of '" + targetName + "'. A world has one of each.";
        }
        claimed.emplace_back(claim, world.name);
    }
    return std::nullopt;
}

} // namespace

std::expected<WorldsConfig, std::string> WorldsConfig::load(const ConfigDirectory& directory,
                                                            const std::string& worldsRoot,
                                                            const wire::common::Logger& logger) {
    const std::string path = directory.pathOf(kFileName);
    const std::expected<TomlTable, std::string> root =
        directory.load(kFileName, std::string{kWorldsDefaultToml} + worldRulesTemplate(true) +
                                      std::string{kGameplayWorldsDefaultToml});
    if (!root.has_value()) {
        return std::unexpected(root.error());
    }
    WorldSettings defaults = WorldSettings::from(root->table("defaults"), WorldSettings::vanilla());
    std::expected<MobSettings, std::string> mobs = loadMobDefaults(directory);
    if (!mobs.has_value()) {
        return std::unexpected(mobs.error());
    }
    defaults.mobs = *std::move(mobs);
    const TomlTable* declared = root->table("worlds");

    std::vector<WorldDeclaration> worlds;
    if (declared != nullptr) {
        for (const std::string& name : declared->keys()) {
            const TomlTable* table = declared->table(name);
            if (table == nullptr) {
                continue;
            }
            const std::string typeName{table->string("type").value_or("normal")};
            const std::optional<WorldType> type = worldTypeByName(typeName);
            if (!type.has_value()) {
                return std::unexpected("World '" + name + "' declares an unknown type '" + typeName +
                                       "'. Known types: " + everyWorldTypeName() + ".");
            }
            const std::string dimensionSpelling{table->string("dimension").value_or("overworld")};
            const std::optional<WorldDimension> dimension = dimensionByName(dimensionSpelling);
            if (!dimension.has_value()) {
                return std::unexpected("World '" + name + "' declares an unknown dimension '" + dimensionSpelling +
                                       "'. Known dimensions: " + everyDimensionName() + ".");
            }
            std::optional<std::string> linkedTo;
            if (const std::optional<std::string_view> linked = table->string("linked_to"); linked.has_value()) {
                linkedTo = std::string{*linked};
            }
            if (linkedTo.has_value() && *dimension == WorldDimension::Overworld) {
                return std::unexpected("World '" + name + "' is an overworld and declares linked_to = \"" + *linkedTo +
                                       "\". Only a nether or an end names the overworld its portals lead to.");
            }
            worlds.push_back(WorldDeclaration{.name = name,
                                              .dimension = *dimension,
                                              .type = *type,
                                              .seed = table->integer("seed"),
                                              .isDefault = table->boolean("default").value_or(false),
                                              .linkedTo = std::move(linkedTo)});
        }
    }

    if (worlds.empty()) {
        return std::unexpected(path + " declares no worlds: a server needs at least one.");
    }
    const std::size_t defaults_count = static_cast<std::size_t>(
        std::ranges::count_if(worlds, [](const WorldDeclaration& world) { return world.isDefault; }));
    if (defaults_count > 1) {
        return std::unexpected(path + " marks more than one world as the default.");
    }
    if (defaults_count == 0) {
        logger.warn([&path, &worlds] {
            return "No world is marked as the default in " + path + "; using '" + worlds.front().name + "'.";
        });
    }
    if (const std::optional<std::string> broken = validateLinks(worlds, path); broken.has_value()) {
        return std::unexpected(*broken);
    }
    return WorldsConfig{std::move(worlds), defaults, worldsRoot};
}

} // namespace basalt::server::runtime::config
