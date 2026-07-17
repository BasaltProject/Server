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

class PersistenceCoordinator {
public:
    struct Hooks {
        std::function<std::map<std::string, world::LoadedWorld*>()> worlds;
        std::function<bool()> wantsUnloadEvents;
        std::function<void(const std::string& worldName, std::int32_t chunkX, std::int32_t chunkZ)> chunkUnloaded;
        std::function<void()> saveShared;
        std::function<void(const std::string& worldName)> worldSaved;
    };

    struct Options {
        std::int32_t spawnRadiusChunks = 0;
        std::int32_t retainMarginChunks = 0;
    };

    PersistenceCoordinator(Hooks hooks, player::PlayerRegistry& players, storage::PlayerStore& store,
                           command::CommandStorage& commandStorage, const ServerMetrics& metrics, Options options,
                           wire::common::Logger logger = wire::common::Logger{"Save"});

    void persistPlayers() const;

    void persistEverything() const;

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
