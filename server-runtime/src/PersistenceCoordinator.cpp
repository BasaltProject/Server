#include <basalt/server/runtime/PersistenceCoordinator.hpp>
#include <basalt/server/runtime/ServerMetrics.hpp>
#include <basalt/server/runtime/command/CommandStorage.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/storage/PlayerStore.hpp>

#include <algorithm>
#include <set>
#include <utility>

namespace basalt::server::runtime {

PersistenceCoordinator::PersistenceCoordinator(Hooks hooks, player::PlayerRegistry& players,
                                               storage::PlayerStore& store, command::CommandStorage& commandStorage,
                                               const ServerMetrics& metrics, Options options,
                                               wire::common::Logger logger)
    : hooks_(std::move(hooks)), players_(&players), store_(&store), commandStorage_(&commandStorage),
      metrics_(&metrics), options_(options), logger_(std::move(logger)) {}

void PersistenceCoordinator::persistPlayers() const {
    std::vector<storage::StoredPlayer> snapshots;
    for (const player::PlayerSession* player : players_->snapshot()) {
        if (!player->connection().storedSnapshot) {
            continue;
        }
        if (std::optional<storage::StoredPlayer> stored = player->connection().storedSnapshot(); stored.has_value()) {
            snapshots.push_back(std::move(*stored));
        }
    }
    if (snapshots.empty()) {
        return;
    }
    store_->players().saveAll(snapshots);
}

void PersistenceCoordinator::persistEverything() const {
    const std::map<std::string, world::LoadedWorld*> worlds = hooks_.worlds ? hooks_.worlds() : decltype(worlds){};
    // Logged before the work, not after: if this ever stops completing, the last thing in the log is
    // where it got to, instead of nothing at all.
    logger_.debug([&] { return "Save starting across " + std::to_string(worlds.size()) + " world(s)."; });
    for (const auto& [name, world] : worlds) {
        world->saveDirty();
        // Clock, weather and game rules, which no chunk carries: cheap enough to write on every save,
        // and losing an hour of world time to a crash is worse than one small file write.
        commandStorage_->save();
        world->saveMetadata();
    }
    persistPlayers();
}

void PersistenceCoordinator::unloadUnreachableChunks() const {
    const std::map<std::string, world::LoadedWorld*> worlds = hooks_.worlds ? hooks_.worlds() : decltype(worlds){};
    const bool watched = hooks_.wantsUnloadEvents && hooks_.chunkUnloaded && hooks_.wantsUnloadEvents();
    std::size_t loaded = 0;
    std::size_t unloaded = 0;
    for (const auto& [name, world] : worlds) {
        // Only the players actually in this world keep its chunks resident.
        std::vector<std::pair<std::int32_t, std::int32_t>> positions;
        for (const player::PlayerSession* player : players_->snapshot()) {
            if (player->worldName == world->name()) {
                positions.emplace_back(static_cast<std::int32_t>(player->x) >> 4,
                                       static_cast<std::int32_t>(player->z) >> 4);
            }
        }
        std::vector<std::int64_t> before;
        if (watched) {
            before = world->state().loadedChunkKeys();
        }
        unloaded += world->unloadUnreachable(positions, options_.spawnRadiusChunks, options_.retainMarginChunks);
        if (watched) {
            const std::vector<std::int64_t> keys = world->state().loadedChunkKeys();
            const std::set<std::int64_t> after{keys.begin(), keys.end()};
            for (const std::int64_t key : before) {
                if (!after.contains(key)) {
                    hooks_.chunkUnloaded(world->name(), static_cast<std::int32_t>(key >> 32),
                                         static_cast<std::int32_t>(key));
                }
            }
        }
        loaded += world->state().loadedChunkCount();
    }

    // Reported every sweep, not only when something happens: when this server misbehaves it is
    // because one of these numbers climbs without settling, and knowing which one is the whole
    // diagnosis. At debug level because it is a diagnostic, not operator news.
    logger_.debug([&] {
        return "Chunks: " + std::to_string(loaded) + " loaded across " + std::to_string(worlds.size()) + " world(s), " +
               std::to_string(unloaded) + " unloaded. " + metrics_->memoryReport();
    });
}

} // namespace basalt::server::runtime
