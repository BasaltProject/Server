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

struct SpawnerEntry {
    wire::common::Identifier type;
    std::int32_t weight = 0;
    std::int32_t minCount = 0;
    std::int32_t maxCount = 0;
    /// parity: 26.3's `count` as a `ConstantInt`, whose sample draws nothing from the random.
    bool constantCount = false;

    friend bool operator==(const SpawnerEntry&, const SpawnerEntry&) = default;
};

struct MobSpawnCost {
    double energyBudget = 0.0;
    double charge = 0.0;

    friend bool operator==(const MobSpawnCost&, const MobSpawnCost&) = default;
};

/// parity: `MobSpawnSettings`.
class RealBiomeSpawns {
public:
    static constexpr float kDefaultCreatureProbability = 0.1F;

    struct Settings {
        std::map<std::string, std::vector<SpawnerEntry>, std::less<>> spawners;
        float creatureProbability = kDefaultCreatureProbability;
        std::map<wire::common::Identifier, MobSpawnCost> costs;
    };

    using ReadText = std::function<std::optional<std::string>(std::string_view name)>;

    /// A callback because only this needs the operator's jar and a test has none.
    struct Jar {
        ReadText readText;
    };

    explicit RealBiomeSpawns(Jar jar, wire::common::Logger logger = wire::common::Logger{"BiomeSpawns"});

    [[nodiscard]] std::vector<SpawnerEntry> listFor(const wire::common::Identifier& biome,
                                                    std::string_view category) const;

    [[nodiscard]] float creatureProbabilityFor(const wire::common::Identifier& biome) const;

    /// parity: `MobSpawnSettings.getMobSpawnCost`, absent for almost every biome and entity.
    [[nodiscard]] std::optional<MobSpawnCost> costFor(const wire::common::Identifier& biome,
                                                      const wire::common::Identifier& type) const;

    [[nodiscard]] Settings of(const wire::common::Identifier& biome) const;

    /// A file that does not parse offers nothing rather than half a list.
    [[nodiscard]] static Settings parse(std::string_view json);

private:
    [[nodiscard]] const Settings& settingsOf(const wire::common::Identifier& biome) const;

    Jar jar_;
    wire::common::Logger logger_;
    mutable std::mutex lock_;
    mutable std::map<std::string, Settings, std::less<>> cache_;
};

} // namespace basalt::server::runtime::world
