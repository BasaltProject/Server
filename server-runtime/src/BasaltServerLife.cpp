#include <basalt/server/plugins/PluginEventTypes.hpp>
#include <basalt/server/plugins/PluginHost.hpp>
#include <basalt/server/runtime/BasaltServer.hpp>
#include <basalt/server/runtime/BuildInfo.hpp>
#include <basalt/server/runtime/ClientSession.hpp>
#include <basalt/server/runtime/config/ForwardingConfig.hpp>
#include <basalt/wire/io/Platform.hpp>
#include <basalt/wire/multiversion/ProtocolVersions.hpp>

#include <algorithm>
#include <chrono>
#include <exception>
#include <random>
#include <thread>

namespace basalt::server::runtime {
namespace {

/// How often the watchdog looks, and how long a plugin may be inside the server. Five ticks: long
/// enough that no honest handler reaches it, short enough to be felt once.
constexpr std::int64_t kPluginWatchdogIntervalMillis = 250;
constexpr std::int64_t kPluginStuckMillis = 250;

/// Long enough for an in-flight chunk to land, short enough that a wedged worker cannot hold the
/// shutdown open indefinitely.
constexpr std::int64_t kShutdownDrainTimeoutMillis = 5000;

/// Generous on purpose: saving a large world legitimately takes a while, and cutting a save short is
/// far worse than waiting for one.
constexpr std::int64_t kShutdownWaitTimeoutMillis = 120000;

} // namespace

void BasaltServer::start(const std::chrono::steady_clock::time_point startedAt) {
    // The driver opens lazily, so without a query here a bad path or an unreadable database would
    // not be discovered until the first player joined, which is the worst possible moment to find
    // out. It also creates the file, so the layout is complete before anyone connects.
    logger_.info([this] {
        return "Player database ready: " + std::to_string(playerStore_->players().count()) + " player(s) known.";
    });
    std::expected<wire::io::TcpListener, wire::io::SocketError> bound =
        wire::io::TcpListener::bind(static_cast<std::uint16_t>(config_->port));
    if (!bound.has_value()) {
        // Almost always another server already on that port, which is worth saying rather than
        // leaving the operator with an error number.
        throw std::runtime_error("Could not bind port " + std::to_string(config_->port) +
                                 ". Another server is probably already running on it; stop it or change `port` in "
                                 "config/server.toml.");
    }
    listener_ = std::move(*bound);

    // Said once at startup, because it changes the order of tick and an operator reading a bug
    // report has to know which mode produced it.
    if (worldRegistry_->scheduler().regional()) {
        logger_.warn([] {
            return std::string{"Regions are on, which is NOT VANILLA: two of them have no fixed order of tick "} +
                   "between each other, and what crosses a border is deferred a tick.";
        });
    }

    // Before the loop, while nothing else is running: a plugin's own code is the last thing to come
    // up and the first to go down, so it never races the tick thread over its own state.
    if (config_->pluginsEnabled) {
        static_cast<void>(plugins_->loadAll());
        reportOrphanedPluginData();
        startPluginWatchdog();
    } else {
        logger_.info([] { return "Plugins are off in server.toml."; });
    }

    // The worlds came up before the plugins did, so this says "these are the worlds" rather than
    // announcing something they could have watched happen.
    //
    // On the owner, not on the thread that is still starting the server up: a decider only runs
    // where the plugins do. Queued rather than run, so it lands as the first thing the loop does.
    worldRegistry_->scheduler().global().submit([this] {
        server::plugins::PluginEventBus& bus = plugins_->events();
        for (const auto& [name, world] : worldRegistry_->worlds()) {
            static_cast<void>(
                server::plugins::emit(&bus, &server::plugins::pluginEventTypes::worldLoad(),
                                      [&name](server::plugins::PluginEvent& event) { event.set(0, name); }));
        }
    });
    for (const auto& [name, world] : worldRegistry_->worlds()) {
        world->state().setOnChunkInstalled([this, worldName = name](server::world::Chunk& chunk) {
            static_cast<void>(server::plugins::emit(&plugins_->events(),
                                                    &server::plugins::pluginEventTypes::worldChunkLoad(),
                                                    [&worldName, &chunk](server::plugins::PluginEvent& event) {
                                                        event.set(0, worldName);
                                                        event.set(1, chunk.chunkX());
                                                        event.set(2, chunk.chunkZ());
                                                        event.set(3, true);
                                                    }));
        });
    }
    // After the plugins are up, so the first thing a plugin can be told is that everything is there.
    // Nothing before this point is worth telling anyone about: half a server is not one.
    static_cast<void>(server::plugins::emit(&plugins_->events(), &server::plugins::pluginEventTypes::serverLoad(),
                                            [this](server::plugins::PluginEvent& event) {
                                                event.set(0,
                                                          static_cast<std::int32_t>(worldRegistry_->worlds().size()));
                                                event.set(1, static_cast<std::int32_t>(plugins_->statuses().size()));
                                            }));

    tickLoop_->start();
    const double elapsedSeconds = static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                                          std::chrono::steady_clock::now() - startedAt)
                                                          .count()) /
                                  1000.0;
    logger_.info([this, elapsedSeconds] {
        return "Done (" + std::to_string(elapsedSeconds) + "s)! Listening on port " + std::to_string(config_->port) +
               " (" + (options_.sessionService != nullptr ? "online" : "offline") + "-mode).";
    });
    config::forwardingConfig::announce(config_->forwarding, config_->onlineMode, config_->port, logger_);

    std::set<std::int32_t> translated = protocolGate_.supported();
    translated.erase(wire::multiversion::protocolVersions::kNative);
    if (!translated.empty()) {
        // The range and the count, not the list: forty eight of them is a wall of text nobody reads,
        // and `/version` prints the whole thing for whoever wants it.
        const std::int32_t oldest = *translated.begin();
        const std::int32_t newest = *translated.rbegin();
        logger_.info([&translated, oldest, newest] {
            return "Also accepting " + std::to_string(translated.size()) + " older client versions, " +
                   wire::multiversion::protocolVersions::describe(oldest) + " to " +
                   wire::multiversion::protocolVersions::describe(newest) + " (protocols " + std::to_string(oldest) +
                   " to " + std::to_string(newest) + ").";
        });
    }

    while (accepting_.load()) {
        std::expected<wire::io::TcpConnection, wire::io::SocketError> accepted = listener_->accept();
        if (!accepted.has_value()) {
            break;
        }
        connections_.emplace_back(
            [this, connection = std::move(*accepted)]() mutable { serveConnection(std::move(connection)); });
        // A connection that has finished is joined by its own destructor when the vector is trimmed.
        std::erase_if(connections_, [](const std::jthread& thread) { return !thread.joinable(); });
    }

    awaitShutdown();
}

void BasaltServer::shutdown() {
    // Runs wherever it was asked from, which is nearly always the tick thread: a signal is noticed
    // inside the tick and `/stop` runs on the owner of a world. That is deliberate rather than
    // tolerated. Saving a world drains its owner, and an owner may only be drained by the thread
    // that owns it, so moving this off the tick thread trades a join for a far worse bug.
    // Once only: `/stop` and a signal can arrive together, and running this twice means two
    // concurrent saves of the same regions.
    bool expected = false;
    if (!shuttingDown_.compare_exchange_strong(expected, true)) {
        return;
    }

    // First of all, so nothing new can arrive while the rest is winding down. Without this a client
    // with auto-reconnect walks straight back in mid-save and starts requesting chunks again, which
    // is exactly the work shutting down is trying to finish.
    accepting_.store(false);
    // Closed, not destroyed: the thread that started the server is inside `accept` on it right now,
    // and taking the object away under it is worse than the error it gets from a closed socket.
    if (listener_.has_value()) {
        listener_->close();
    }

    const std::vector<player::PlayerSession*> players = playerRegistry_.snapshot();
    if (!players.empty()) {
        // Saved here, synchronously, *before* disconnecting them. Disconnecting only closes the
        // socket; each session then saves itself on its own thread, which raced the database being
        // closed a few lines below. Whoever lost that race was not saved at all.
        logger_.info([&players] { return "Saving " + std::to_string(players.size()) + " player(s)."; });
        persistence_->persistPlayers();

        logger_.info([&players] { return "Disconnecting " + std::to_string(players.size()) + " player(s)."; });
        for (player::PlayerSession* player : players) {
            if (player->connection().disconnect) {
                player->connection().disconnect(wire::text::Component::text("Server closed").color(wire::text::kRed));
            }
            playerRegistry_.remove(player->uuid());
        }
    }

    tickLoop_->stop();
    static_cast<void>(server::plugins::emit(
        &plugins_->events(), &server::plugins::pluginEventTypes::serverStop(),
        [&players](server::plugins::PluginEvent& event) { event.set(0, static_cast<std::int32_t>(players.size())); }));
    // After the loop has stopped, for the same reason it loaded before it started.
    plugins_->unloadAll();
    for (const auto& [name, world] : worldRegistry_->worlds()) {
        world->stopGenerating(kShutdownDrainTimeoutMillis);
    }

    logger_.info([this] { return "Saving " + std::to_string(worldRegistry_->worlds().size()) + " world(s)."; });
    for (const auto& [name, world] : worldRegistry_->worlds()) {
        static_cast<void>(server::plugins::emit(&plugins_->events(), &server::plugins::pluginEventTypes::worldSave(),
                                                [&name](server::plugins::PluginEvent& event) { event.set(0, name); }));
        world->saveDirty();
        world->flushRegions();
    }
    // Nothing else runs anywhere: what was queued is cancelled rather than run against a world that
    // is already written out.
    worldRegistry_->scheduler().close();
    // The players were saved before being disconnected, above. This is the checkpoint that leaves
    // players.db as one self-contained file.
    playerStore_.reset();
    logger_.info([this] { return shouldRestart() ? "Restarting the server." : "Stopping the server."; });
    shutdownComplete_.store(true);
}

void BasaltServer::requestRestart() {
    restarting_.store(true);
    shutdown();
}

void BasaltServer::shutdownIfSignalled() {
    if (!wire::io::shutdownSignal::isRequested()) {
        return;
    }
    logger_.info([] { return "Termination signal received, shutting down."; });
    shutdown();
}

void BasaltServer::awaitShutdown() {
    if (!shuttingDown_.load()) {
        return;
    }
    const std::chrono::steady_clock::time_point deadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds{kShutdownWaitTimeoutMillis};
    while (std::chrono::steady_clock::now() < deadline) {
        if (shutdownComplete_.load()) {
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{20});
    }
    logger_.error([] {
        return "Shutdown did not finish within " + std::to_string(kShutdownWaitTimeoutMillis) +
               "ms, carrying on anyway.";
    });
}

void BasaltServer::startPluginWatchdog() {
    // Its own thread, because a watchdog that runs on the thread it is watching is not a watchdog.
    // The instruction ceiling stops a plugin spinning in Lua on its own; this is for the other half,
    // a plugin stuck inside something the host is doing for it, which no counter sees.
    watchdog_ = std::jthread{[this](const std::stop_token& stop) {
        while (!stop.stop_requested() && accepting_.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds{kPluginWatchdogIntervalMillis});
            static_cast<void>(plugins_->interruptIfStuck(kPluginStuckMillis));
        }
    }};
}

void BasaltServer::reportOrphanedPluginData() {
    // Nothing is deleted: a plugin taken out for an afternoon has to find its data when it comes
    // back. The operator is told it is there and how to get rid of it.
    if (config_->orphanData != "warn") {
        return;
    }
    const std::map<std::string, std::int64_t> orphans = plugins_->orphanedData();
    if (orphans.empty()) {
        return;
    }
    logger_.warn([&orphans] {
        std::vector<std::pair<std::string, std::int64_t>> byRows{orphans.begin(), orphans.end()};
        std::ranges::sort(byRows, [](const auto& left, const auto& right) { return left.second > right.second; });
        std::string listed;
        for (const auto& [id, rows] : byRows) {
            if (!listed.empty()) {
                listed += ", ";
            }
            listed += id + " (" + std::to_string(rows) + " rows)";
        }
        return "Data is here for " + std::to_string(orphans.size()) + " plugin(s) that are not installed: " + listed +
               ". Nothing was deleted. /plugins orphans lists them, /plugins purge <id> removes one.";
    });
}

} // namespace basalt::server::runtime
