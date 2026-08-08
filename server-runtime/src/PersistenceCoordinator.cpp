#include <basalt/server/runtime/PersistenceCoordinator.hpp>
#include <basalt/server/runtime/ServerMetrics.hpp>
#include <basalt/server/runtime/command/CommandStorage.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/storage/PlayerStore.hpp>

#include <algorithm>
#include <cstdint>
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
    // Logged before the work: if saving ever stops completing, the log says where it got to.
    logger_.debug([&] { return "Save starting across " + std::to_string(worlds.size()) + " world(s)."; });
    for (const auto& [name, world] : worlds) {
        world->saveDirty();
        // Clock, weather and game rules no chunk carries: cheap per save, and losing world time to a crash is worse.
        commandStorage_->save();
        world->saveMetadata();
        if (hooks_.worldSaved) {
            hooks_.worldSaved(name);
        }
    }
    if (hooks_.saveShared) {
        hooks_.saveShared();
    }
    persistPlayers();
}

void PersistenceCoordinator::unloadUnreachableChunks() const {
    const std::map<std::string, world::LoadedWorld*> worlds = hooks_.worlds ? hooks_.worlds() : decltype(worlds){};
    const bool watched = hooks_.wantsUnloadEvents && hooks_.chunkUnloaded && hooks_.wantsUnloadEvents();
    std::size_t loaded = 0;
    std::size_t unloaded = 0;
    for (const auto& [name, world] : worlds) {
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
        const config::GameplayRules& chunks = world->settings().rules.gameplay;
        unloaded += world->unloadUnreachable(positions, chunks.keepSpawnLoadedRange, chunks.unloadMarginChunks);
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

    // Reported every sweep: a misbehaving server shows one number climbing without settling. Debug, as a diagnostic.
    logger_.debug([&] {
        return "Chunks: " + std::to_string(loaded) + " loaded across " + std::to_string(worlds.size()) + " world(s), " +
               std::to_string(unloaded) + " unloaded. " + metrics_->memoryReport();
    });
}

} // namespace basalt::server::runtime
