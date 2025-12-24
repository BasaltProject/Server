#pragma once

#include <basalt/server/runtime/config/BasaltConfig.hpp>
#include <basalt/server/runtime/config/WorldsConfig.hpp>
#include <basalt/server/runtime/sched/RegionScheduler.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/VanillaWorldGeneratorFactory.hpp>
#include <basalt/wire/common/logging/Logger.hpp>
#include <basalt/wire/common/timings/Timings.hpp>

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace basalt::server::runtime::player {
class PlayerRegistry;
}

namespace basalt::server::runtime {

/// Every world this server knows about: what builds one, what makes a new one at runtime, the
/// scheduler they all share, and what `/reload` has to redo once the configuration underneath them
/// changes. Split out of `BasaltServer` so the shape of "a world" has one owner.
class WorldRegistry {
public:
    /// What has to be told that something changed, read late: the registry is built before the
    /// command dispatcher and the plugin host exist, and by the time these run, they do.
    struct Hooks {
        /// A world was made at runtime, one turn after the fact so a plugin handler is not re-entered.
        std::function<void(const std::string& name)> worldLoaded;
        /// Builds the command tree again and pushes it to everyone: a command's permission and
        /// aliases are baked in when it is registered, and the client holds its own copy.
        std::function<void()> rebuildCommands;
    };

    struct Options {
        std::string worldsRoot = "worlds";
        wire::common::Timings* timings = nullptr;
        /// One worker while measuring: the generation metrics count without atomics on purpose, so
        /// their numbers are only exact when a single thread is generating.
        bool generationMetrics = false;
    };

    WorldRegistry(const config::BasaltConfig& config, const config::WorldsConfig& worlds,
                  world::VanillaWorldGeneratorFactory* generators, player::PlayerRegistry& players, Hooks hooks,
                  Options options, wire::common::Logger logger = wire::common::Logger{"Worlds"});

    WorldRegistry(const WorldRegistry&) = delete;
    WorldRegistry& operator=(const WorldRegistry&) = delete;
    ~WorldRegistry();

    /// Every world, by the name a player's stored row refers to.
    ///
    /// Answered by value on purpose, and replaced rather than added to: the tick loop walks these
    /// while a plugin can be making one, and inserting into a map somebody is iterating is a crash
    /// on the tick thread. Swapping the map leaves whoever is mid-walk on the old one, which is
    /// exactly right: a world made this turn starts ticking the next.
    [[nodiscard]] std::map<std::string, world::LoadedWorld*> worlds() const;

    [[nodiscard]] world::LoadedWorld& defaultWorld() const;

    [[nodiscard]] std::int64_t worldSeed() const;

    [[nodiscard]] sched::RegionScheduler& scheduler() noexcept { return scheduler_; }

    /// Makes a world that was not in `worlds.toml`, the way a plugin asks for one.
    ///
    /// A world like any other: its own folder, `settings.toml` and seed. Null if the name is taken
    /// or unusable, because a world that half exists is worse than none.
    ///
    /// Its spawn is not generated up front, unlike the declared ones: this runs inside a tick, and
    /// forty-nine chunks there is a freeze every player would feel.
    world::LoadedWorld* createWorld(const std::string& name, config::WorldDimension dimension, config::WorldType type,
                                    std::optional<std::int64_t> seed);

    /// The chunk-set version each world was last split on, so an unchanged world is not walked.
    [[nodiscard]] std::map<std::string, std::int64_t>& lastChunkSetVersion() noexcept { return lastChunkSetVersion_; }

    /// What has to be told that the configuration changed.
    ///
    /// The tab list, scoreboard, boss bar and server list all read their file on every use, so they
    /// need nothing. The command tree does not.
    void onConfigsReloaded();

private:
    [[nodiscard]] std::unique_ptr<world::LoadedWorld> buildWorld(const config::WorldDeclaration& declaration,
                                                                 world::WorldMetadata metadata);

    const config::WorldsConfig* worldsConfig_;
    world::VanillaWorldGeneratorFactory* generators_;
    player::PlayerRegistry* players_;
    Hooks hooks_;
    Options options_;
    wire::common::Logger logger_;

    /// Every world's `level.nbt`, read before any world is built.
    std::map<std::string, world::WorldMetadata> metadata_;
    /// The worlds themselves, each owned here for the life of the server.
    std::vector<std::unique_ptr<world::LoadedWorld>> owned_;
    /// The generators of each world, kept because a world borrows them for its whole life.
    std::map<std::string, world::VanillaWorldGeneratorFactory::Generators> built_;
    std::map<std::string, std::int64_t> lastChunkSetVersion_;
    std::string defaultName_;
    sched::RegionScheduler scheduler_;
};

} // namespace basalt::server::runtime
