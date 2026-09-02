#pragma once

#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/common/logging/Logger.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::world {

/// One entry of a biome's spawn list: what can appear there, how likely, and how many at a time.
struct SpawnerEntry {
    wire::common::Identifier type;
    std::int32_t weight = 0;
    std::int32_t minCount = 0;
    std::int32_t maxCount = 0;

    friend bool operator==(const SpawnerEntry&, const SpawnerEntry&) = default;
};

/// How strongly one mob repels another of the same biome-defined cost family.
struct MobSpawnCost {
    double energyBudget = 0.0;
    double charge = 0.0;

    friend bool operator==(const MobSpawnCost&, const MobSpawnCost&) = default;
};

/// What each biome can spawn, out of the operator's own `server.jar`.
///
/// parity: `MobSpawnSettings`, the `spawners` block of every
/// `data/minecraft/worldgen/biome/<name>.json`. Datapack content, so it is read from the download
/// rather than committed, the same as loot tables and recipes. Which of those entries actually
/// appears is `NaturalSpawner`'s business; this only says what the biome offers.
class RealBiomeSpawns {
public:
    /// parity: `MobSpawnSettings.creatureGenerationProbability`'s own default.
    static constexpr float kDefaultCreatureProbability = 0.1F;

    /// One biome file's worth: what it offers, how often, and what each kind costs.
    struct Settings {
        std::map<std::string, std::vector<SpawnerEntry>, std::less<>> spawners;
        float creatureProbability = kDefaultCreatureProbability;
        std::map<wire::common::Identifier, MobSpawnCost> costs;
    };

    /// One jar entry as text, or nothing when the jar has no such entry.
    using ReadText = std::function<std::optional<std::string>(std::string_view name)>;

    /// The jar arrives as a callback rather than as `ServerJarAssets`: this needs the operator's own
    /// copy of the game, and a test has no jar to hand it.
    struct Jar {
        ReadText readText;
    };

    explicit RealBiomeSpawns(Jar jar, wire::common::Logger logger = wire::common::Logger{"BiomeSpawns"});

    /// The spawn list of a biome for one category, empty when the biome offers nothing of that kind.
    [[nodiscard]] std::vector<SpawnerEntry> listFor(const wire::common::Identifier& biome,
                                                    std::string_view category) const;

    [[nodiscard]] float creatureProbabilityFor(const wire::common::Identifier& biome) const;

    /// parity: `MobSpawnSettings.getMobSpawnCost`, absent for almost every biome and entity.
    [[nodiscard]] std::optional<MobSpawnCost> costFor(const wire::common::Identifier& biome,
                                                      const wire::common::Identifier& type) const;

    [[nodiscard]] Settings of(const wire::common::Identifier& biome) const;

    /// One biome file, for a caller that already has the text. A file that does not parse offers
    /// nothing rather than half a list: the other loaders read the jar the same way.
    [[nodiscard]] static Settings parse(std::string_view json);

private:
    [[nodiscard]] const Settings& settingsOf(const wire::common::Identifier& biome) const;

    Jar jar_;
    wire::common::Logger logger_;
    mutable std::mutex lock_;
    mutable std::map<std::string, Settings, std::less<>> cache_;
};

} // namespace basalt::server::runtime::world
