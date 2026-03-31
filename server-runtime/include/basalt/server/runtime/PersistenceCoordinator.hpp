#pragma once

#include <basalt/wire/common/logging/Logger.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace basalt::server::storage {
class PlayerStore;
}

namespace basalt::server::runtime {
class ServerMetrics;
}

namespace basalt::server::runtime::command {
class CommandStorage;
}

namespace basalt::server::runtime::player {
class PlayerRegistry;
}

namespace basalt::server::runtime::world {
class LoadedWorld;
}

namespace basalt::server::runtime {

/// Everything that writes the server's state to disk on a schedule: players, worlds, and the chunks
/// nobody can reach any more. Split out of `BasaltServer`; the tick loop and `SaveControl` still
/// decide *when* this runs, this only decides what running means.
class PersistenceCoordinator {
public:
    /// A chunk that has just been unloaded, for whoever wants to hear about it.
    ///
    /// A callback rather than the plugin bus itself: this module has no business knowing what a
    /// plugin is, and `wantsUnloadEvents` is what keeps the before-and-after comparison from being
    /// paid for when nobody is listening.
    struct Hooks {
        /// Read live rather than captured once: a plugin can add a world after this is built, and it
        /// must be saved on the next pass.
        std::function<std::map<std::string, world::LoadedWorld*>()> worlds;
        std::function<bool()> wantsUnloadEvents;
        std::function<void(const std::string& worldName, std::int32_t chunkX, std::int32_t chunkZ)> chunkUnloaded;
        /// What belongs to the server rather than to one world, like its maps.
        std::function<void()> saveShared;
    };

    struct Options {
        std::int32_t spawnRadiusChunks = 0;
        std::int32_t retainMarginChunks = 0;
    };

    PersistenceCoordinator(Hooks hooks, player::PlayerRegistry& players, storage::PlayerStore& store,
                           command::CommandStorage& commandStorage, const ServerMetrics& metrics, Options options,
                           wire::common::Logger logger = wire::common::Logger{"Save"});

    /// Writes every online player as one transaction, on the same interval the worlds save on.
    void persistPlayers() const;

    /// parity: `MinecraftServer.saveEverything`, the whole autosave: every world, then the players.
    void persistEverything() const;

    /// Chunks nobody can reach go, whatever changed in them written on the way out.
    void unloadUnreachableChunks() const;

private:
    Hooks hooks_;
    player::PlayerRegistry* players_;
    storage::PlayerStore* store_;
    command::CommandStorage* commandStorage_;
    const ServerMetrics* metrics_;
    Options options_;
    wire::common::Logger logger_;
};

} // namespace basalt::server::runtime
