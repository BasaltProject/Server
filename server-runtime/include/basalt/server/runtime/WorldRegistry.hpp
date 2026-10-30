#pragma once

#include <basalt/server/runtime/config/BasaltConfig.hpp>
#include <basalt/server/runtime/config/WorldsConfig.hpp>
#include <basalt/server/runtime/sched/RegionScheduler.hpp>
#include <basalt/server/runtime/world/Jukeboxes.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/VanillaWorldGeneratorFactory.hpp>
#include <basalt/wire/common/logging/Logger.hpp>
#include <basalt/wire/common/timings/Timings.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <atomic>
#include <mutex>
#include <set>
#include <string>
#include <vector>

namespace basalt::server::runtime::player {
class PlayerRegistry;
}

namespace basalt::server::runtime::world {
class AdvancementTriggers;
class RealLootTables;
} // namespace basalt::server::runtime::world

namespace basalt::server::runtime::plugins {
class EventSink;
}

namespace basalt::server::runtime {

/// Split out of `BasaltServer` so a world has one owner.
class WorldRegistry {
public:
    /// Read late: the registry is built before the command dispatcher and plugin host exist.
    struct Hooks {
        /// One turn after the fact so a plugin handler is not re-entered.
        std::function<void(const std::string& name)> worldLoaded;
        /// A command's permission and aliases are baked in at registration and the client holds its own copy.
        std::function<void()> rebuildCommands;
    };

    struct Options {
        std::string worldsRoot = "worlds";
        wire::common::Timings* timings = nullptr;
        bool generationMetrics = false;
        /// Null in a test, and then every jukebox reads zero.
        world::jukeboxes::SongOutputs* jukeboxes = nullptr;
        /// Null in a test, and then such a chest stays empty.
        world::RealLootTables* lootTables = nullptr;
        world::AdvancementTriggers* triggers = nullptr;
        /// Null in a test, and then no plugin hears the world.
        plugins::EventSink* events = nullptr;
    };

    WorldRegistry(const config::BasaltConfig& config, const config::WorldsConfig& worlds,
                  world::VanillaWorldGeneratorFactory* generators, player::PlayerRegistry& players, Hooks hooks,
                  Options options, wire::common::Logger logger = wire::common::Logger{"Worlds"});

    WorldRegistry(const WorldRegistry&) = delete;
    WorldRegistry& operator=(const WorldRegistry&) = delete;
    ~WorldRegistry();

    [[nodiscard]] std::map<std::string, world::LoadedWorld*> worlds() const;

    /// Short-lived login leases keep a snapshot valid until player registration.
    [[nodiscard]] std::map<std::string, std::shared_ptr<world::LoadedWorld>> worldsForLogin() const;

    [[nodiscard]] world::LoadedWorld& defaultWorld() const;

    [[nodiscard]] std::int64_t worldSeed() const;

    [[nodiscard]] sched::RegionScheduler& scheduler() noexcept { return *scheduler_; }

    /// Null if the name is taken or unusable.
    world::LoadedWorld* createWorld(const std::string& name, config::WorldDimension dimension, config::WorldType type,
                                    std::optional<std::int64_t> seed);

    /// Takes an empty world out of play and closes storage; a later create reloads it from disk.
    bool unloadWorld(const std::string& name, bool save);

    /// Only a registered, unloaded non-default world inside managed storage. Never a raw path.
    bool deleteWorld(const std::string& name);

    [[nodiscard]] std::map<std::string, std::int64_t>& lastChunkSetVersion() noexcept { return lastChunkSetVersion_; }

    /// The tab list, scoreboard, boss bar and server list read their file on every use; the command tree does not.
    void onConfigsReloaded();

private:
    [[nodiscard]] std::shared_ptr<world::LoadedWorld> buildWorld(const config::WorldDeclaration& declaration,
                                                                 world::WorldMetadata metadata);

    const config::WorldsConfig* worldsConfig_;
    world::VanillaWorldGeneratorFactory* generators_;
    player::PlayerRegistry* players_;
    Hooks hooks_;
    Options options_;
    wire::common::Logger logger_;

    mutable std::recursive_mutex worldsMutex_;
    mutable std::map<std::string, std::shared_ptr<std::atomic<std::size_t>>> loginPins_;
    std::map<std::string, world::WorldMetadata> metadata_;
    std::vector<std::shared_ptr<world::LoadedWorld>> owned_;
    std::map<std::string, std::string> storagePaths_;
    std::set<std::string> unloaded_;
    std::set<std::string> unloading_;
    std::map<std::string, std::int64_t> lastChunkSetVersion_;
    std::string defaultName_;
    std::shared_ptr<sched::RegionScheduler> scheduler_;
};

} // namespace basalt::server::runtime
