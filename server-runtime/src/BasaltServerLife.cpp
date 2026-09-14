#include <basalt/server/plugins/PluginEventTypes.hpp>
#include <basalt/server/plugins/PluginHost.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/BasaltServer.hpp>
#include <basalt/server/runtime/BuildInfo.hpp>
#include <basalt/server/runtime/ClientSession.hpp>
#include <basalt/server/runtime/bedrock/BedrockChunks.hpp>
#include <basalt/server/runtime/bedrock/BedrockPlayer.hpp>
#include <basalt/server/runtime/config/ForwardingConfig.hpp>
#include <basalt/server/runtime/player/PlayerBlockBreak.hpp>
#include <basalt/server/runtime/player/PlayerBlockPlace.hpp>
#include <basalt/server/runtime/player/PlayerContainers.hpp>
#include <basalt/server/runtime/player/PlayerDamage.hpp>
#include <basalt/server/runtime/player/PlayerDeath.hpp>
#include <basalt/server/runtime/player/PlayerDigSpeed.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/player/PlayerItemPickup.hpp>
#include <basalt/server/runtime/world/ContainerOpeners.hpp>
#include <basalt/server/runtime/world/Fluids.hpp>
#include <basalt/wire/io/Platform.hpp>
#include <basalt/wire/multiversion/ProtocolVersions.hpp>
#include <basalt/wire/text/Serializers.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <exception>
#include <random>
#include <thread>

namespace basalt::server::runtime {
namespace {

/// One source for the numbers nobody checks bit for bit: a drop's scatter, a seed.
[[nodiscard]] std::mt19937_64& scratchRandom() {
    static thread_local std::mt19937_64 random{std::random_device{}()};
    return random;
}

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

/// parity: `Player.drop`, which lifts a thrown stack a tenth of a block and lets it fall in front
/// of the eyes rather than at the feet.
constexpr double kThrowEyeHeight = 1.5;
constexpr double kThrowLift = 0.1;

/// parity: `ItemEntity`'s own delay, so a dropped stack is not picked straight back up.
constexpr std::int32_t kPickupDelay = 40;

} // namespace

void BasaltServer::start(const std::chrono::steady_clock::time_point startedAt) {
    // The driver opens lazily, so without a query here a bad path or an unreadable database would
    // not be discovered until the first player joined, which is the worst possible moment to find
    // out. It also creates the file, so the layout is complete before anyone connects.
    logger_.info([this] {
        return "Player database ready: " + std::to_string(playerStore_->players().count()) + " player(s) known.";
    });
    // A server for one edition only is a real thing to want, so the door that is shut is simply not
    // opened: nothing else in here knows which of the two a player came through.
    if (config_->javaEnabled) {
        std::expected<wire::io::TcpListener, wire::io::SocketError> bound =
            wire::io::TcpListener::bind(static_cast<std::uint16_t>(config_->port));
        if (!bound.has_value()) {
            // Almost always another server already on that port, which is worth saying rather than
            // leaving the operator with an error number.
            throw std::runtime_error("Could not bind port " + std::to_string(config_->port) +
                                     ". Another server is probably already running on it; stop it or change `port` in "
                                     "[network.java] of config/server.toml.");
        }
        listener_ = std::move(*bound);
    }
    startBedrockListener();

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
    std::set<std::int32_t> translated = config_->javaEnabled ? protocolGate_.supported() : std::set<std::int32_t>{};
    translated.erase(wire::multiversion::protocolVersions::kNative);
    // One line, and it starts with the part everybody waits for: whether it is up and how long it
    // took. Where it listens and what it speaks follow in the same breath, because three lines for
    // one fact is three lines to read.
    logger_.info([this, elapsedSeconds, &translated] {
        std::string line = "Done (" + std::to_string(elapsedSeconds) + "s)! ";
        const bool java = config_->javaEnabled && listener_.has_value();
        if (java) {
            line += "Java on TCP port " + std::to_string(config_->port) + " (" +
                    (options_.sessionService != nullptr ? "online" : "offline") + "-mode)";
            if (!translated.empty()) {
                // The range and the count, not the list: forty eight of them is a wall of text
                // nobody reads, and `/version` prints the whole thing for whoever wants it.
                line += " with " + std::to_string(translated.size()) + " older versions down to " +
                        wire::multiversion::protocolVersions::describe(*translated.begin());
            }
        }
        // What is open, and nothing about what is not: the two editions are two doors and neither
        // is the default the other is measured against. Both shut is refused when the config is
        // read, so there is always one to name.
        if (bedrockListener_ != nullptr) {
            line += java ? ", and " : "";
            line += bedrockListener_->welcome();
        }
        return line + ".";
    });
    config::forwardingConfig::announce(config_->forwarding, config_->onlineMode, config_->port, logger_);

    while (accepting_.load() && listener_.has_value()) {
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
    // Only the door: the thread stays up until the players below have been told why they are
    // leaving, because it is that thread that carries the goodbye to a Bedrock client.
    if (bedrockListener_) {
        bedrockListener_->closeDoor();
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
    if (bedrockListener_) {
        bedrockListener_->stop();
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

player::BreakContext BasaltServer::breakContext() {
    player::BreakContext context;
    context.blocks = broadcaster_.get();
    context.registry = &playerRegistry_;
    context.lootTables = &vanillaData_.lootTables;
    context.blockTags = &vanillaData_.blockTags;
    context.enchantments = &vanillaData_.enchantments;
    context.itemTags = &vanillaData_.itemTags;
    context.nextDouble = [] { return std::uniform_real_distribution<double>{0.0, 1.0}(scratchRandom()); };
    context.nextSeed = [] { return static_cast<std::int64_t>(scratchRandom()()); };
    // However that edition draws one slot of a bag, which for Bedrock is the whole window.
    context.slotChanged = [](const player::PlayerSession& player, std::int32_t) {
        if (player.connection().sendInventoryAll) {
            player.connection().sendInventoryAll();
        }
    };
    return context;
}

player::DamageContext BasaltServer::damageContext() {
    player::DamageContext context;
    context.registry = &playerRegistry_;
    context.damageTypeTags = &vanillaData_.damageTypeTags;
    context.enchantments = &vanillaData_.enchantments;
    context.death.registry = &playerRegistry_;
    context.death.logger = logger_;
    context.death.lang = options_.lang;
    return context;
}

/// Tells the world that one more, or one fewer, is looking into that block: the lid of a chest,
/// the state of a barrel, the sound and what a sculk sensor hears. A double chest counts as two,
/// because a lid is per block entity.
void BasaltServer::openersChanged(world::LoadedWorld& world, const bedrock::OpenWindow& window, const bool opened) {
    player::OpenersContext context;
    context.blocks = breakContext();
    context.nextFloat = [] { return std::uniform_real_distribution<float>{0.0F, 1.0F}(scratchRandom()); };
    context.nextSeed = [] { return static_cast<std::int64_t>(scratchRandom()()); };
    const auto tell = [&](const std::int32_t x, const std::int32_t y, const std::int32_t z) {
        const std::int32_t openers = opened ? world::containerOpeners::open(world.name(), x, y, z)
                                            : world::containerOpeners::close(world.name(), x, y, z);
        player::announceOpeners(context, world, x, y, z, window.block, openers);
    };
    tell(window.x, window.y, window.z);
    if (window.second.has_value()) {
        tell((*window.second)[0], (*window.second)[1], (*window.second)[2]);
    }
}

void BasaltServer::startBedrockListener() {
    const config::BedrockSettings& settings = options_.compatibility.bedrock();
    if (!config_->bedrockEnabled) {
        return;
    }

    bedrock::BedrockOptions options;
    options.enabled = true;
    options.port = static_cast<std::uint16_t>(config_->bedrockPort);
    options.prefix = config_->bedrockPrefix;

    // What this build speaks, narrowed to what the operator asked for. A list that names a protocol
    // this build has no codec for is said out loud rather than accepted and then failed at.
    std::set<std::int32_t> speakable = wire::bedrock::BedrockGate::buildSupport();
    if (settings.protocols.has_value()) {
        std::set<std::int32_t> chosen;
        std::vector<std::int32_t> missing;
        for (const std::int32_t protocol : *settings.protocols) {
            if (speakable.contains(protocol)) {
                chosen.insert(protocol);
            } else {
                missing.push_back(protocol);
            }
        }
        if (!missing.empty()) {
            logger_.warn([&missing] {
                std::string named;
                for (const std::int32_t protocol : missing) {
                    if (!named.empty()) {
                        named += ", ";
                    }
                    named += std::to_string(protocol);
                }
                return "server.toml asks for Bedrock protocols this build cannot speak: " + named;
            });
        }
        speakable = std::move(chosen);
    }
    options.gate = wire::bedrock::BedrockGate{std::move(speakable), settings.futureProtocols};

    bedrock::BedrockListener::Hooks hooks;
    // The same answer a Java client gets to its ping, rendered flat: a Bedrock server list has one
    // line and no colours of its own to give it.
    hooks.status = [this] {
        bedrock::BedrockListener::Status status;
        const wire::protocol::status::ServerStatusPayload payload =
            serverList_->statusFor(wire::multiversion::protocolVersions::kNative, "bedrock", std::nullopt);
        status.motd = wire::text::toPlainText(payload.description);
        if (payload.players.has_value()) {
            status.online = payload.players->online;
            status.max = payload.players->max;
        }
        return status;
    };

    // Where this player left off, asked before the world is described to them: `start_game` names a
    // position and the client puts its player there for good. A saved position that arrives after
    // it, as a teleport, is one the client has already decided against.
    hooks.spawn = [this](const wire::common::Uuid& uuid) {
        bedrock::BedrockListener::Spawn spawn;
        if (worldRegistry_ != nullptr) {
            const world::LoadedWorld& world = worldRegistry_->defaultWorld();
            const std::array<double, 3>& at = world.spawn();
            spawn.x = at[0];
            spawn.y = at[1];
            spawn.z = at[2];
        }
        if (playerStore_ == nullptr || !playerStore_->isOpen()) {
            return spawn;
        }
        const std::optional<storage::StoredPlayer> stored = playerStore_->players().load(uuid.toString());
        if (!stored.has_value()) {
            return spawn;
        }
        spawn.gamemode = static_cast<std::int32_t>(stored->gamemode);
        // Only the position is meaningless when the world they were in is gone.
        if (worldRegistry_ != nullptr && stored->world != worldRegistry_->defaultWorld().name()) {
            return spawn;
        }
        spawn.x = stored->x;
        spawn.y = stored->y;
        spawn.z = stored->z;
        spawn.yaw = stored->yaw;
        spawn.pitch = stored->pitch;
        return spawn;
    };

    // What a client that finished the login becomes: the same kind of player a Java login makes,
    // in the same registry, so everything that asks who is online finds them.
    hooks.join =
        [this](const bedrock::BedrockListener::Joining& joining) -> std::unique_ptr<bedrock::BedrockListener::Player> {
        bedrock::BedrockPlayer::Context context;
        context.registry = &playerRegistry_;
        // The same two events the Java door raises, with the same fields: a plugin has no way of
        // telling which door somebody came in by, and that is the point. Queued onto the owner
        // because a decider only runs where the plugins do, carrying copies since the player may be
        // gone by the time the owner gets to it.
        const auto emitOnOwner = [this](const server::plugins::PluginEventType& type,
                                        const player::PlayerSession& player) {
            std::string name = player.name();
            std::string uuid = player.uuid().toString();
            std::string world = player.worldName;
            worldRegistry_->scheduler().global().submit(
                [this, &type, name = std::move(name), uuid = std::move(uuid), world = std::move(world)] {
                    static_cast<void>(
                        server::plugins::emit(&plugins_->events(), &type, [&](server::plugins::PluginEvent& filled) {
                            filled.set(0, name);
                            filled.set(1, uuid);
                            filled.set(2, world);
                        }));
                });
        };
        context.joined = [emitOnOwner](const player::PlayerSession& player) {
            emitOnOwner(server::plugins::pluginEventTypes::playerJoin(), player);
        };
        context.quit = [emitOnOwner](const player::PlayerSession& player) {
            emitOnOwner(server::plugins::pluginEventTypes::playerQuit(), player);
        };
        // The world, built where the world lives. A chunk belongs to its region's owner, so the
        // columns are read and written there and only the finished bytes cross to the Bedrock
        // thread. Queued rather than answered on the spot for the same reason.
        context.sendChunks = [this](std::vector<std::pair<std::int32_t, std::int32_t>> columns, const int protocol,
                                    std::function<void(std::vector<std::uint8_t>)> send, std::function<void()> done) {
            if (worldRegistry_ == nullptr) {
                return;
            }
            worldRegistry_->scheduler().global().submit(
                [this, columns = std::move(columns), protocol, send = std::move(send), done = std::move(done)] {
                    world::LoadedWorld& world = worldRegistry_->defaultWorld();
                    for (const auto& [x, z] : columns) {
                        send(bedrock::writeColumn(world.state().getOrGenerateChunk(x, z), protocol));
                    }
                    if (done) {
                        done();
                    }
                });
        };

        // What players.db remembers, and where what they become goes back. The same store the Java
        // door reads: a player who came in one door and out the other is one row, not two.
        context.stored = [this](const wire::common::Uuid& uuid) -> std::optional<storage::StoredPlayer> {
            if (playerStore_ == nullptr || !playerStore_->isOpen()) {
                return std::nullopt;
            }
            return playerStore_->players().load(uuid.toString());
        };
        context.save = [this](const storage::StoredPlayer& row) {
            // The database is closed when the server is shutting down, and by then everyone has
            // already been written: this is only the door catching up.
            if (playerStore_ != nullptr && playerStore_->isOpen()) {
                playerStore_->players().save(row);
            }
        };

        // Moving between worlds: the same refusal the Java path makes (staying put is not a move),
        // and the world's own dimension turned into the one Bedrock draws.
        context.changeWorld = [](player::PlayerSession& player, const world::LoadedWorld& target, const double x,
                                 const double y,
                                 const double z) -> std::optional<bedrock::BedrockPlayer::Context::Arrival> {
            static_cast<void>(x);
            static_cast<void>(y);
            static_cast<void>(z);
            if (player.worldName == target.name()) {
                return std::nullopt;
            }
            bedrock::BedrockPlayer::Context::Arrival arrival;
            arrival.worldName = target.name();
            switch (target.declaration().dimension) {
            case config::WorldDimension::Nether:
                arrival.dimension = wire::bedrock::BedrockDimension::Nether;
                break;
            case config::WorldDimension::End:
                arrival.dimension = wire::bedrock::BedrockDimension::End;
                break;
            case config::WorldDimension::Overworld:
                arrival.dimension = wire::bedrock::BedrockDimension::Overworld;
                break;
            }
            return arrival;
        };

        // Dying goes through the same code a Java player's death does, queued where the rest of
        // the server runs: it touches the registry and every other player's connection.
        context.killed = [this](player::PlayerSession& player, const std::string_view deathMessageId) {
            const wire::common::Uuid uuid = player.uuid();
            const std::string reason{deathMessageId};
            worldRegistry_->scheduler().global().submit([this, uuid, reason] {
                player::PlayerSession* session = playerRegistry_.get(uuid);
                if (session == nullptr) {
                    return;
                }
                player::DeathContext death;
                death.registry = &playerRegistry_;
                death.logger = logger_;
                death.lang = options_.lang;
                player::killPlayer(*session, reason, std::nullopt, death);
            });
        };

        // Breaking a block is the same everywhere, so both doors call the same thing; what this
        // adds is only getting onto the thread that owns the world.
        context.breakBlock = [this](player::PlayerSession& player, const std::int32_t x, const std::int32_t y,
                                    const std::int32_t z) {
            const wire::common::Uuid uuid = player.uuid();
            worldRegistry_->scheduler().global().submit([this, uuid, x, y, z] {
                player::PlayerSession* session = playerRegistry_.get(uuid);
                if (session == nullptr) {
                    return;
                }
                const std::map<std::string, world::LoadedWorld*> worlds = worldRegistry_->worlds();
                const auto found = worlds.find(session->worldName);
                if (found == worlds.end() || found->second == nullptr) {
                    return;
                }
                // TEMPORARY: whether the world agreed, which is what tells a refused break from a
                // break that never arrived.
                const bool broke = player::breakBlockFor(*session, *found->second, x, y, z, breakContext());
                logger_.debug([broke, x, y, z] {
                    return std::string{"break at "} + std::to_string(x) + ", " + std::to_string(y) + ", " +
                           std::to_string(z) + (broke ? ": yes" : ": refused");
                });
            });
        };

        // What is alive and what is lying around, near enough to be worth drawing. Taken where the
        // world is and left for the door's thread to turn into packets.
        context.lookAround = [this](const player::PlayerSession& player,
                                    std::function<void(std::vector<bedrock::Seen>)> answer) {
            const wire::common::Uuid uuid = player.uuid();
            worldRegistry_->scheduler().global().submit([this, uuid, answer = std::move(answer)] {
                player::PlayerSession* session = playerRegistry_.get(uuid);
                if (session == nullptr) {
                    return;
                }
                const std::map<std::string, world::LoadedWorld*> worlds = worldRegistry_->worlds();
                const auto found = worlds.find(session->worldName);
                if (found == worlds.end() || found->second == nullptr) {
                    return;
                }
                // Near enough to be worth drawing, which is the draw distance a Bedrock join asks
                // for rather than the whole world.
                constexpr double kRange = 64.0;
                const auto near = [session](const double x, const double y, const double z) {
                    const double dx = x - session->x;
                    const double dy = y - session->y;
                    const double dz = z - session->z;
                    return (dx * dx) + (dy * dy) + (dz * dz) <= kRange * kRange;
                };

                std::vector<bedrock::Seen> seen;
                for (const entity::ServerEntity* alive : found->second->state().entities()) {
                    if (alive == nullptr || alive->removed || !near(alive->x, alive->y, alive->z)) {
                        continue;
                    }
                    // Both editions name a cow `minecraft:cow`. One they disagree on is one the
                    // client will not draw, and drawing the wrong animal would be worse.
                    seen.push_back(bedrock::Seen{.entityId = alive->entityId(),
                                                 .type = alive->type().toString(),
                                                 .x = alive->x,
                                                 .y = alive->y,
                                                 .z = alive->z,
                                                 .yaw = alive->yaw,
                                                 .pitch = alive->pitch,
                                                 .width = static_cast<float>(alive->width()),
                                                 .height = static_cast<float>(alive->height()),
                                                 .stack = std::nullopt});
                }
                for (const world::DroppedItem& dropped : found->second->state().items()) {
                    if (!near(dropped.x, dropped.y, dropped.z)) {
                        continue;
                    }
                    seen.push_back(bedrock::Seen{.entityId = dropped.entityId,
                                                 .x = dropped.x,
                                                 .y = dropped.y,
                                                 .z = dropped.z,
                                                 .stack = storage::StoredItemStack{.id = dropped.item.toString(),
                                                                                   .count = dropped.count,
                                                                                   .components = dropped.components}});
                }
                answer(std::move(seen));
            });
        };

        // What they are standing on, gathered where the world is and told to them where they are.
        context.pickupItems = [this](player::PlayerSession& player) {
            const wire::common::Uuid uuid = player.uuid();
            worldRegistry_->scheduler().global().submit([this, uuid] {
                player::PlayerSession* session = playerRegistry_.get(uuid);
                if (session == nullptr) {
                    return;
                }
                const std::map<std::string, world::LoadedWorld*> worlds = worldRegistry_->worlds();
                const auto found = worlds.find(session->worldName);
                if (found == worlds.end() || found->second == nullptr) {
                    return;
                }
                const std::vector<player::PickedItem> taken =
                    player::pickupNearbyItems(*session, found->second->state(), pluginHooks().pickupAllowed);
                if (taken.empty()) {
                    return;
                }
                // The little arc the stack makes on its way in, to everybody who can see it.
                for (const player::PickedItem& row : taken) {
                    for (const player::PlayerSession* watcher : playerRegistry_.snapshot()) {
                        if (watcher->worldName == session->worldName && watcher->connection().sendPickupAnimation) {
                            watcher->connection().sendPickupAnimation(row.item.entityId, session->entityId());
                        }
                    }
                }
                if (session->connection().sendInventoryAll) {
                    session->connection().sendInventoryAll();
                }
            });
        };

        // How fast this player digs, answered where the world is: the block, its hardness and
        // whether the eyes are under water all live over there.
        context.digSpeed = [this](player::PlayerSession& player, const std::int32_t x, const std::int32_t y,
                                  const std::int32_t z, std::function<void(float)> answer) {
            const wire::common::Uuid uuid = player.uuid();
            worldRegistry_->scheduler().global().submit([this, uuid, x, y, z, answer = std::move(answer)] {
                player::PlayerSession* session = playerRegistry_.get(uuid);
                if (session == nullptr) {
                    return;
                }
                const std::map<std::string, world::LoadedWorld*> worlds = worldRegistry_->worlds();
                const auto found = worlds.find(session->worldName);
                if (found == worlds.end() || found->second == nullptr) {
                    return;
                }
                const world::LoadedWorld& world = *found->second;
                const std::optional<std::int32_t> state = world.state().blockState(x, y, z);
                if (!state.has_value()) {
                    return;
                }
                const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(*state);
                if (block == nullptr || registry::blocks::isAir(*state)) {
                    return;
                }
                const double eyeY = session->y + world::fluids::eyeHeight(session->sneaking);
                const std::optional<std::int32_t> around = world.state().blockState(
                    static_cast<std::int32_t>(std::floor(session->x)), static_cast<std::int32_t>(std::floor(eyeY)),
                    static_cast<std::int32_t>(std::floor(session->z)));
                const bool underwater = around.has_value() && world::fluids::of(*around) == world::Submersion::Water;
                answer(player::destroyProgressPerTickFor(*session, *block, registry::blocks::hardnessOf(*block),
                                                         breakContext().blockTags, underwater));
            });
        };

        // Putting a block down, where the world is. What the client says it holds is not asked:
        // the slot is, and what is in it here is what goes down.
        context.useOn =
            [this](player::PlayerSession& player, const player::BlockHit& hit, const std::int32_t hotbarSlot,
                   std::shared_ptr<bedrock::BedrockWindows> windows,
                   std::function<void(const bedrock::OpenWindow&, std::vector<std::optional<storage::StoredItemStack>>)>
                       opened) {
                const wire::common::Uuid uuid = player.uuid();
                worldRegistry_->scheduler().global().submit([this, uuid, hit, hotbarSlot, windows = std::move(windows),
                                                             opened = std::move(opened)] {
                    player::PlayerSession* session = playerRegistry_.get(uuid);
                    if (session == nullptr) {
                        return;
                    }
                    const std::map<std::string, world::LoadedWorld*> worlds = worldRegistry_->worlds();
                    const auto found = worlds.find(session->worldName);
                    if (found == worlds.end() || found->second == nullptr) {
                        return;
                    }
                    // parity: `ServerPlayerGameMode.useItemOn` runs the block's own use before it
                    // considers what is held as something to place, and sneaking overrides that.
                    if (!session->sneaking && windows != nullptr && opened) {
                        if (std::optional<bedrock::OpenWindow> window =
                                bedrock::windowFor(*found->second, hit.x, hit.y, hit.z);
                            window.has_value()) {
                            windows->nextContainerId =
                                windows->nextContainerId >= bedrock::kMaxContainerId ? 1 : windows->nextContainerId + 1;
                            window->containerId = windows->nextContainerId;
                            windows->open = *window;
                            // The next window is a different window: what was said about the last
                            // one says nothing about this one.
                            windows->saidSlots = bedrock::readWindow(*found->second, *window);
                            windows->saidNumbers.reset();
                            windows->sinceRead = 0;
                            opened(*window, windows->saidSlots);
                            openersChanged(*found->second, *window, true);
                            return;
                        }
                    }
                    const std::int32_t slot = player::PlayerInventory::kHotbarStart + hotbarSlot;
                    const std::optional<storage::StoredItemStack> held = session->inventory.at(slot);
                    if (!held.has_value()) {
                        return;
                    }
                    const std::expected<wire::common::Identifier, wire::common::IdentifierError> item =
                        wire::common::Identifier::parse(held->id);
                    if (!item.has_value()) {
                        return;
                    }
                    player::PlaceContext placing;
                    placing.blocks = breakContext();
                    // A sign that has just gone down opens its own editor, the same as on the other
                    // door: without it a Bedrock player places a sign and is never asked for text.
                    placing.openSign = [](const player::PlayerSession& who, const std::int32_t signX,
                                          const std::int32_t signY, const std::int32_t signZ, std::int32_t) {
                        if (who.connection().openSignEditor) {
                            who.connection().openSignEditor(signX, signY, signZ);
                        }
                    };
                    placing.sendInventory = [](const player::PlayerSession& who) {
                        if (who.connection().sendInventoryAll) {
                            who.connection().sendInventoryAll();
                        }
                    };
                    // TEMPORARY: whether the world took it, which is what tells a refused placement
                    // from one that never reached here.
                    const bool placed =
                        player::placeBlockFor(*session, *found->second, hit, *item, slot, *held, placing);
                    logger_.debug([placed, &hit, &held] {
                        return "place " + held->id + " at " + std::to_string(hit.x) + ", " + std::to_string(hit.y) +
                               ", " + std::to_string(hit.z) + (placed ? ": yes" : ": refused");
                    });
                });
            };

        // A window closing, which is the client's decision: whatever was being dragged goes back
        // in the bag rather than staying in a window nobody is looking at.
        context.closeWindow = [this](player::PlayerSession& player, std::shared_ptr<bedrock::BedrockWindows> windows) {
            const wire::common::Uuid uuid = player.uuid();
            worldRegistry_->scheduler().global().submit([this, uuid, windows = std::move(windows)] {
                player::PlayerSession* session = playerRegistry_.get(uuid);
                if (session == nullptr || windows == nullptr) {
                    return;
                }
                const std::map<std::string, world::LoadedWorld*> worlds = worldRegistry_->worlds();
                const auto found = worlds.find(session->worldName);
                if (windows->open.has_value() && found != worlds.end() && found->second != nullptr) {
                    openersChanged(*found->second, *windows->open, false);
                }
                windows->open.reset();
                windows->saidSlots.clear();
                windows->saidNumbers.reset();
                // parity: `AbstractContainerMenu.removed`, which empties the grid into the player
                // rather than leaving it in a table the next person opens.
                bool returned = false;
                for (std::optional<storage::StoredItemStack>& cell : windows->grid) {
                    if (cell.has_value()) {
                        static_cast<void>(session->inventory.add(*cell));
                        cell.reset();
                        returned = true;
                    }
                }
                if (returned && session->connection().sendInventoryAll) {
                    session->connection().sendInventoryAll();
                }
                if (windows->cursor.has_value()) {
                    static_cast<void>(session->inventory.add(*windows->cursor));
                    windows->cursor.reset();
                    if (session->connection().sendInventoryAll) {
                        session->connection().sendInventoryAll();
                    }
                }
            });
        };

        // Tidying a bag, where the world is: a refused request is answered with the whole
        // inventory, which is the same thing a Java click gets.
        context.moveStacks =
            [this](player::PlayerSession& player, wire::bedrock::BedrockStackRequest request,
                   std::shared_ptr<bedrock::BedrockWindows> windows,
                   std::function<void(bool, std::vector<wire::bedrock::BedrockResponseContainer>)> answer) {
                const wire::common::Uuid uuid = player.uuid();
                worldRegistry_->scheduler().global().submit([this, uuid, request = std::move(request),
                                                             windows = std::move(windows), answer = std::move(answer)] {
                    player::PlayerSession* session = playerRegistry_.get(uuid);
                    if (session == nullptr) {
                        return;
                    }
                    const std::map<std::string, world::LoadedWorld*> worlds = worldRegistry_->worlds();
                    const auto found = worlds.find(session->worldName);
                    std::function<void(const storage::StoredItemStack&)> drop;
                    if (found != worlds.end() && found->second != nullptr) {
                        drop = [this, session, world = found->second](const storage::StoredItemStack& stack) {
                            const std::expected<wire::common::Identifier, wire::common::IdentifierError> item =
                                wire::common::Identifier::parse(stack.id);
                            if (!item.has_value()) {
                                return;
                            }
                            player::spawnDroppedItem(breakContext(), *world, *item, stack.count, session->x,
                                                     session->y + kThrowEyeHeight, session->z, 0.0, kThrowLift, 0.0,
                                                     kPickupDelay, stack.components);
                        };
                    }
                    world::LoadedWorld* const where = found == worlds.end() ? nullptr : found->second;
                    const bedrock::StackRequestAnswer applied =
                        bedrock::applyStackRequest(*session, *windows, where, request, drop);
                    answer(applied.accepted, applied.containers);
                    if (!applied.accepted && session->connection().sendInventoryAll) {
                        session->connection().sendInventoryAll();
                    }
                });
            };

        // Hurting and healing are the same arithmetic on both editions, so both doors call the same
        // thing; what this adds is only getting onto the thread that owns the world and the player.
        context.hurt = [this](player::PlayerSession& player, const float amount, const std::string_view deathMessageId,
                              const wire::common::Identifier& damageType,
                              const std::optional<entity::DamageSource>& source) {
            const wire::common::Uuid uuid = player.uuid();
            const std::string reason{deathMessageId};
            const wire::common::Identifier type = damageType;
            worldRegistry_->scheduler().global().submit([this, uuid, amount, reason, type, source] {
                player::PlayerSession* session = playerRegistry_.get(uuid);
                if (session == nullptr) {
                    return;
                }
                player::hurtPlayer(*session, amount, reason, type, source, damageContext());
            });
        };
        context.heal = [this](player::PlayerSession& player, const float amount) {
            const wire::common::Uuid uuid = player.uuid();
            worldRegistry_->scheduler().global().submit([this, uuid, amount] {
                player::PlayerSession* session = playerRegistry_.get(uuid);
                if (session != nullptr) {
                    player::healPlayer(*session, amount, damageContext());
                }
            });
        };

        // Which commands to offer, filtered by permission exactly as the Java tree is: a player who
        // cannot run one is never told it exists. Read straight off the dispatcher rather than
        // queued, because the answer goes back to the caller and the tree only changes when a
        // plugin loads or a group does.
        context.commandList =
            [this](const player::PlayerSession& player) -> std::vector<std::pair<std::string, std::string>> {
            command::CommandDispatcher* dispatcher = wired_.dispatcher.get();
            // The source wants the session the registry owns, not the const view this was handed.
            player::PlayerSession* session = playerRegistry_.get(player.uuid());
            if (dispatcher == nullptr || session == nullptr) {
                return {};
            }
            const command::PlayerCommandSource source{*session, dispatcher->environment().groups()};
            std::vector<std::pair<std::string, std::string>> named;
            for (const command::CommandNode* node : dispatcher->usableCommands(source)) {
                named.emplace_back(node->name(), command::smartUsage(*node, source));
            }
            return named;
        };

        // A command a Bedrock player typed, run where commands are allowed to run and through the
        // same dispatcher and the same plugin event as one typed on Java.
        context.runCommand = [this](const player::PlayerSession& player, std::string line) {
            const wire::common::Uuid uuid = player.uuid();
            const std::string name = player.name();
            const std::string world = player.worldName;
            worldRegistry_->scheduler().global().submit([this, uuid, name, world, line = std::move(line)]() mutable {
                command::CommandDispatcher* dispatcher = wired_.dispatcher.get();
                player::PlayerSession* session = playerRegistry_.get(uuid);
                if (dispatcher == nullptr || session == nullptr) {
                    return;
                }
                const server::plugins::PluginEvent* event =
                    server::plugins::emit(&plugins_->events(), &server::plugins::pluginEventTypes::playerCommand(),
                                          [&](server::plugins::PluginEvent& filled) {
                                              filled.set(0, name);
                                              filled.set(1, uuid.toString());
                                              filled.set(2, world);
                                              filled.set(3, line);
                                          });
                if (event != nullptr) {
                    if (const std::string* edited = std::get_if<std::string>(&event->get(3)); edited != nullptr) {
                        line = *edited;
                    }
                    if (event->cancelled()) {
                        return;
                    }
                }
                logger_.info([&name, &line] { return name + " issued server command: /" + line; });
                command::PlayerCommandSource source{*session, dispatcher->environment().groups()};
                dispatcher->execute(source, line);
            });
        };
        context.chatFilter = [this](const player::PlayerSession& player, std::string message,
                                    std::function<void(bool, std::string)> answer) {
            std::string name = player.name();
            std::string uuid = player.uuid().toString();
            std::string world = player.worldName;
            worldRegistry_->scheduler().global().submit([this, name = std::move(name), uuid = std::move(uuid),
                                                         world = std::move(world), message = std::move(message),
                                                         answer = std::move(answer)]() mutable {
                const server::plugins::PluginEvent* event =
                    server::plugins::emit(&plugins_->events(), &server::plugins::pluginEventTypes::playerChat(),
                                          [&](server::plugins::PluginEvent& filled) {
                                              filled.set(0, name);
                                              filled.set(1, uuid);
                                              filled.set(2, world);
                                              filled.set(3, message);
                                          });
                if (event == nullptr) {
                    answer(true, std::move(message));
                    return;
                }
                if (const std::string* edited = std::get_if<std::string>(&event->get(3)); edited != nullptr) {
                    message = *edited;
                }
                answer(!event->cancelled(), std::move(message));
            });
        };
        context.lang = options_.lang;
        context.logger = logger_;
        // The same files and the same placeholders the other door draws from.
        context.uiConfigs = [this] { return live_->current()->ui; };
        context.placeholders = &placeholders_;
        if (worldRegistry_ != nullptr) {
            const world::LoadedWorld& world = worldRegistry_->defaultWorld();
            const std::array<double, 3>& spawn = world.spawn();
            context.worldName = world.name();
            context.spawnX = spawn[0];
            context.spawnY = spawn[1];
            context.spawnZ = spawn[2];
            // What time it is right now, for a client that has just arrived and would otherwise
            // run its own clock from whatever it last saw.
            context.worldTime = static_cast<std::int32_t>(world.clock().totalTicks());
            context.weather = world.weather();
        }
        return std::make_unique<bedrock::BedrockPlayer>(joining, std::move(context));
    };

    bedrockListener_ = std::make_unique<bedrock::BedrockListener>(std::move(options), std::move(hooks),
                                                                  std::string{bedrock::kBedrockGameVersion});
    if (!bedrockListener_->start()) {
        // It said why in the log already. Dropping it here is what keeps the startup line honest:
        // a port that never opened is not one to announce.
        bedrockListener_.reset();
    }
}

} // namespace basalt::server::runtime
