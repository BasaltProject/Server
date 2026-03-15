#include <basalt/server/plugins/PluginEventTypes.hpp>
#include <basalt/server/plugins/PluginHost.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/registry/Items.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/BasaltServer.hpp>
#include <basalt/server/runtime/BuildInfo.hpp>
#include <basalt/server/runtime/ClientSession.hpp>
#include <basalt/server/runtime/ExplosionSystem.hpp>
#include <basalt/server/runtime/bedrock/BedrockChunks.hpp>
#include <basalt/server/runtime/bedrock/BedrockPlayer.hpp>
#include <basalt/server/runtime/bedrock/BedrockRecipes.hpp>
#include <basalt/server/runtime/bedrock/BedrockSeen.hpp>
#include <basalt/server/runtime/config/ForwardingConfig.hpp>
#include <basalt/server/runtime/entity/Breeding.hpp>
#include <basalt/server/runtime/entity/EntityCombat.hpp>
#include <basalt/server/runtime/entity/MobRules.hpp>
#include <basalt/server/runtime/entity/Villagers.hpp>
#include <basalt/server/runtime/item/ItemDamageEffects.hpp>
#include <basalt/server/runtime/item/ItemWear.hpp>
#include <basalt/server/runtime/player/Combat.hpp>
#include <basalt/server/runtime/player/PlayerAnvil.hpp>
#include <basalt/server/runtime/player/PlayerBlockBreak.hpp>
#include <basalt/server/runtime/player/PlayerBlockItemUse.hpp>
#include <basalt/server/runtime/player/PlayerBlockPlace.hpp>
#include <basalt/server/runtime/player/PlayerBlockSlots.hpp>
#include <basalt/server/runtime/player/PlayerBlockToggle.hpp>
#include <basalt/server/runtime/player/PlayerBooks.hpp>
#include <basalt/server/runtime/player/PlayerBuckets.hpp>
#include <basalt/server/runtime/player/PlayerContainers.hpp>
#include <basalt/server/runtime/player/PlayerCooldowns.hpp>
#include <basalt/server/runtime/player/PlayerDamage.hpp>
#include <basalt/server/runtime/player/PlayerDeath.hpp>
#include <basalt/server/runtime/player/PlayerDigSpeed.hpp>
#include <basalt/server/runtime/player/PlayerEating.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/player/PlayerItemPickup.hpp>
#include <basalt/server/runtime/player/PlayerPick.hpp>
#include <basalt/server/runtime/player/PlayerSigns.hpp>
#include <basalt/server/runtime/player/PlayerSleep.hpp>
#include <basalt/server/runtime/player/PlayerSounds.hpp>
#include <basalt/server/runtime/player/PlayerSpawnItems.hpp>
#include <basalt/server/runtime/player/PlayerSweep.hpp>
#include <basalt/server/runtime/player/PlayerThrown.hpp>
#include <basalt/server/runtime/world/AnvilEnchantments.hpp>
#include <basalt/server/runtime/world/Beacons.hpp>
#include <basalt/server/runtime/world/Beds.hpp>
#include <basalt/server/runtime/world/ChiseledBookshelves.hpp>
#include <basalt/server/runtime/world/CommandBlocks.hpp>
#include <basalt/server/runtime/world/ComparatorReading.hpp>
#include <basalt/server/runtime/world/ContainerOpeners.hpp>
#include <basalt/server/runtime/world/Crafters.hpp>
#include <basalt/server/runtime/world/DecoratedPots.hpp>
#include <basalt/server/runtime/world/DetectorRails.hpp>
#include <basalt/server/runtime/world/Enchanting.hpp>
#include <basalt/server/runtime/world/EnchantmentBooks.hpp>
#include <basalt/server/runtime/world/Fluids.hpp>
#include <basalt/server/runtime/world/Grindstones.hpp>
#include <basalt/server/runtime/world/Lecterns.hpp>
#include <basalt/server/runtime/world/Looms.hpp>
#include <basalt/server/runtime/world/RealEnchantments.hpp>
#include <basalt/server/runtime/world/RedstoneComponents.hpp>
#include <basalt/server/runtime/world/Shelves.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>
#include <basalt/server/runtime/world/WeaponDamage.hpp>
#include <basalt/wire/bedrock/BedrockEntityIdentifiers.hpp>
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

const bedrock::BedrockRecipeList& BasaltServer::bedrockRecipeBook() {
    if (!bedrockRecipes_.has_value()) {
        bedrockRecipes_ = bedrock::bedrockRecipes(vanillaData_.recipes);
        logger_.debug([this] {
            return "Bedrock recipes: " + std::to_string(bedrockRecipes_->recipes.size()) + " sent, " +
                   std::to_string(bedrockRecipes_->unnamed) + " left out for want of a name over there.";
        });
    }
    return *bedrockRecipes_;
}

std::optional<storage::StoredItemStack> BasaltServer::bedrockRecipeResult(const std::int32_t netId) {
    const bedrock::BedrockRecipeList& book = bedrockRecipeBook();
    const auto found = book.results.find(netId);
    return found == book.results.end() ? std::nullopt : std::optional{found->second};
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
    context.awardExperience = [this](world::LoadedWorld& world, double x, double y, double z, std::int32_t amount) {
        if (orbs_ != nullptr) {
            orbs_->award(world, x, y, z, amount);
        }
    };
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
    context.slotChanged = [](const player::PlayerSession& player, std::int32_t) {
        if (player.connection().sendInventoryAll) {
            player.connection().sendInventoryAll();
        }
    };
    context.disableBlocking = [this](player::PlayerSession& player, std::int32_t ticks) {
        player.blockingSlot.reset();
        player.blockingTicks = 0;
        const std::map<std::string, world::LoadedWorld*> worlds = worldRegistry_->worlds();
        const auto found = worlds.find(player.worldName);
        player::startCooldownFor(player, wire::common::Identifier::vanilla("shield"),
                                 found == worlds.end() || found->second == nullptr ? 0 : found->second->gameTime(),
                                 ticks);
    };
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

void BasaltServer::rollBedrockEnchantment(player::PlayerSession& session, world::LoadedWorld& world,
                                          bedrock::BedrockWindows& windows) {
    if (!windows.open.has_value() || windows.open->type != wire::bedrock::BedrockContainerType::Enchantment) {
        windows.enchanting.reset();
        return;
    }
    if (session.enchantmentSeed == 0) {
        session.enchantmentSeed = static_cast<std::int32_t>(scratchRandom()());
    }
    bedrock::OpenEnchantment rolled;
    const std::optional<storage::StoredItemStack>& input = windows.grid[0];
    if (input.has_value()) {
        const world::realBlockTags::TagMembers* tags = &vanillaData_.blockTags;
        const std::int32_t shelves = world::enchanting::bookshelvesAround(
            [&world](const std::int32_t x, const std::int32_t y,
                     const std::int32_t z) -> std::optional<wire::common::Identifier> {
                const std::optional<std::int32_t> state = world.state().blockState(x, y, z);
                const wire::common::Identifier* block =
                    state.has_value() ? server::registry::blockStateDefinitions::blockOf(*state) : nullptr;
                return block == nullptr ? std::nullopt : std::optional{*block};
            },
            [tags](const std::string_view tag, const wire::common::Identifier& block) {
                return world::realBlockTags::contains(*tags, tag, block);
            },
            windows.open->x, windows.open->y, windows.open->z);
        const world::EnchantmentBooks books{.enchantments = &vanillaData_.enchantments,
                                            .itemTags = &vanillaData_.itemTags,
                                            .enchantmentTags = &vanillaData_.enchantmentTags};
        const std::array<world::enchanting::Row, 3> rows =
            world::enchanting::roll(books, *input, session.enchantmentSeed, shelves);
        for (std::size_t row = 0; row < rows.size(); ++row) {
            rolled.rows[row].cost = rows[row].cost;
            rolled.rows[row].offers = rows[row].offers;
            rolled.rows[row].netId = ++windows.nextEnchantNetId;
        }
    }
    windows.enchanting = std::move(rolled);
    if (session.connection().sendEnchantOptions) {
        session.connection().sendEnchantOptions();
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

    // The recipe book, built once and handed to every client that joins: on that door the list the
    // server sends is the only crafting a client knows.
    hooks.recipes = [this] { return bedrockRecipeBook().recipes; };

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
                    if (alive != nullptr && !alive->removed && near(alive->x, alive->y, alive->z)) {
                        seen.push_back(bedrock::seenOf(*alive));
                    }
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

        // A swing at something alive, settled where the world is. Every number here comes from the
        // same place the other door's swing does: the charge, the reach and what a hit is worth are
        // all in `player::combat`, so the two editions cannot drift apart.
        context.attackEntity = [this](player::PlayerSession& player, const std::int32_t entityId, const bool falling) {
            const wire::common::Uuid uuid = player.uuid();
            worldRegistry_->scheduler().global().submit([this, uuid, entityId, falling] {
                player::PlayerSession* session = playerRegistry_.get(uuid);
                if (session == nullptr || session->dead ||
                    session->gamemode == wire::protocol::play::Gamemode::Spectator) {
                    return;
                }
                const std::map<std::string, world::LoadedWorld*> worlds = worldRegistry_->worlds();
                const auto found = worlds.find(session->worldName);
                if (found == worlds.end() || found->second == nullptr) {
                    return;
                }
                world::LoadedWorld& world = *found->second;
                entity::ServerEntity* target = world.state().entity(entityId);
                if (target == nullptr || target->removed || !registry::entityAttributes::isLiving(target->type())) {
                    return;
                }
                const bool reached =
                    player::combat::withinAttackReach(session->x, session->y, session->z, target->x, target->y,
                                                      target->z, target->width(), target->height());
                // TEMPORARY: whether the swing reached, which tells a miss from a hit never arrived.
                logger_.debug([reached, session, target] {
                    return std::string{reached ? "attack: within reach" : "attack: out of reach"} + " (player " +
                           std::to_string(session->x) + "," + std::to_string(session->z) + " target " +
                           std::to_string(target->x) + "," + std::to_string(target->z) + ")";
                });
                if (!reached) {
                    return;
                }

                const std::optional<storage::StoredItemStack>& held = session->inventory.heldItem();
                std::optional<wire::common::Identifier> weapon;
                if (held.has_value()) {
                    if (const auto parsed = wire::common::Identifier::parse(held->id); parsed.has_value()) {
                        weapon = *parsed;
                    }
                }
                const registry::WeaponStats* stats = weapon.has_value() ? registry::itemWeapons::of(*weapon) : nullptr;
                const float strength = player::combat::attackStrengthScale(
                    session->ticksSinceSwing,
                    registry::itemWeapons::kBaseAttackSpeed + (stats == nullptr ? 0.0 : stats->attackSpeed));
                session->ticksSinceSwing = 0;
                const auto base = static_cast<float>(registry::itemWeapons::kBaseAttackDamage +
                                                     (stats == nullptr ? 0.0 : stats->attackDamage));
                // parity: what the enchantments add rides on the swing but never on the critical, so
                // it is worked out here and handed to the same arithmetic the other door uses.
                world::RealEnchantments* enchantments = &vanillaData_.enchantments;
                const float enchanted =
                    world::weaponDamage::of(held, base, [enchantments](const wire::common::Identifier& id) {
                        return enchantments->definitionOf(id);
                    });
                const player::combat::Swing swing =
                    player::combat::swingOf(strength, base, enchanted, falling, session->onGround, session->sprinting);
                if (swing.damage <= 0.0F) {
                    return;
                }

                const entity::entityCombat::Result result = entity::entityCombat::hurt(
                    *target, swing.damage, session->x, session->z, {}, "attack", session->name());
                if (!result.landed) {
                    return;
                }
                if (swing.knockbackAttack) {
                    entity::entityCombat::knockback(*target, 0.5, session->x - target->x, session->z - target->z);
                }
                // parity: `isSweepAttack`: a sword, on the ground, a swing that is neither critical
                // nor a sprinting one. The sweep itself is the same code the other door runs.
                if (swing.fullStrength && !swing.critical && !swing.knockbackAttack && session->onGround &&
                    weapon.has_value() && weapon->path().ends_with("_sword")) {
                    const std::vector<player::SweepHit> caught =
                        player::sweepAround(*session, world, *target, strength);
                    for (player::PlayerSession* watcher : playerRegistry_.snapshot()) {
                        if (watcher->worldName != session->worldName || !watcher->connection().playSound) {
                            continue;
                        }
                        watcher->connection().playSound("entity.player.attack.sweep", session->x, session->y,
                                                        session->z, 1.0F, 1.0F);
                        // Everything the sweep caught sounds for itself, the same as what was aimed
                        // at: a silent sweep looks like it missed.
                        for (const player::SweepHit& hit : caught) {
                            const entity::ServerEntity* struck = world.state().entity(hit.entityId);
                            if (struck == nullptr) {
                                continue;
                            }
                            watcher->connection().playSound(hit.died ? entity::entityCombat::deathSound(struck->type())
                                                                     : entity::entityCombat::hurtSound(struck->type()),
                                                            struck->x, struck->y, struck->z, 1.0F, 1.0F);
                        }
                    }
                }
                // parity: `HurtByTargetGoal` and `PanicGoal` both read the last thing that hurt it.
                target->hurtBy = session->entityId();
                entity::entityCombat::rememberPlayer(*target, session->uuid());
                // parity: `Player.attack` wears the weapon by one point per hit, and unbreaking
                // gets its say before the wear lands rather than repairing it afterwards.
                if (session->gamemode != wire::protocol::play::Gamemode::Creative && held.has_value() &&
                    item::itemWear::isDamageable(*held)) {
                    item::ItemDamageEffects effects{
                        [enchantments](const wire::common::Identifier& id) { return enchantments->definitionOf(id); }};
                    random::RandomSource wear =
                        random::RandomSource::legacy(static_cast<std::int64_t>(scratchRandom()()));
                    const std::optional<storage::StoredItemStack> worn = item::itemWear::hurt(
                        *held, item::itemWear::costPerAttack(*held), wear,
                        [&effects](const storage::StoredItemStack& stack, const std::int32_t amount,
                                   random::RandomSource& source) { return effects.process(stack, amount, source); });
                    if (worn != held) {
                        session->inventory.set(player::PlayerInventory::kHotbarStart + session->inventory.heldSlot(),
                                               worn);
                        if (session->connection().sendInventoryAll) {
                            session->connection().sendInventoryAll();
                        }
                    }
                }
                world::vibrations::emit(world.state(), "entity_damage", target->x, target->y, target->z,
                                        world.gameTime());
                for (player::PlayerSession* watcher : playerRegistry_.snapshot()) {
                    if (watcher->worldName != session->worldName) {
                        continue;
                    }
                    player::playSoundFor(*watcher,
                                         result.died ? entity::entityCombat::deathSound(target->type())
                                                     : entity::entityCombat::hurtSound(target->type()),
                                         wire::protocol::play::SoundSource::Hostile, target->x, target->y, target->z,
                                         1.0F, 1.0F, 0);
                    // parity: `ServerPlayer.crit` and the knockback sound, which is what tells the
                    // difference between a good hit and an ordinary one from across the field.
                    if (swing.critical) {
                        player::playSoundFor(*watcher, "entity.player.attack.crit",
                                             wire::protocol::play::SoundSource::Players, session->x, session->y,
                                             session->z, 1.0F, 1.0F, 0);
                        if (watcher->connection().showCriticalHit) {
                            watcher->connection().showCriticalHit(target->entityId());
                        }
                    } else if (swing.knockbackAttack) {
                        player::playSoundFor(*watcher, "entity.player.attack.knockback",
                                             wire::protocol::play::SoundSource::Players, session->x, session->y,
                                             session->z, 1.0F, 1.0F, 0);
                    }
                }
            });
        };

        // What a villager offers, read where the world is. A mob that is no merchant offers nothing
        // and the click goes on to whatever else it was.
        context.trades = [this](player::PlayerSession& player,
                                const std::int32_t entityId) -> std::optional<bedrock::OpenTrades> {
            const std::map<std::string, world::LoadedWorld*> worlds = worldRegistry_->worlds();
            const auto found = worlds.find(player.worldName);
            if (found == worlds.end() || found->second == nullptr) {
                return std::nullopt;
            }
            entity::ServerEntity* target = found->second->state().entity(entityId);
            if (target == nullptr || target->removed || target->type() != entity::villagers::type()) {
                return std::nullopt;
            }
            std::vector<world::trades::Offer> offers =
                entity::villagers::offersFor(vanillaData_.villagerTrades, target->profession, target->villagerLevel);
            if (offers.empty()) {
                return std::nullopt;
            }
            return bedrock::OpenTrades{
                .entityId = entityId, .tier = std::max(0, target->villagerLevel - 1), .offers = std::move(offers)};
        };

        context.useEntity = [this](player::PlayerSession& player, const std::int32_t entityId) {
            const wire::common::Uuid uuid = player.uuid();
            worldRegistry_->scheduler().global().submit([this, uuid, entityId] {
                player::PlayerSession* session = playerRegistry_.get(uuid);
                if (session == nullptr || session->gamemode == wire::protocol::play::Gamemode::Spectator) {
                    return;
                }
                const std::map<std::string, world::LoadedWorld*> worlds = worldRegistry_->worlds();
                const auto found = worlds.find(session->worldName);
                if (found == worlds.end() || found->second == nullptr) {
                    return;
                }
                entity::ServerEntity* target = found->second->state().entity(entityId);
                if (target == nullptr || target->removed) {
                    return;
                }
                // parity: `AbstractMinecart.interact`, which seats whoever clicked it.
                if (world::detectorRails::isMinecart(target->type())) {
                    if (session->vehicleId.has_value()) {
                        return;
                    }
                    session->vehicleId = target->entityId();
                    target->passengerIds.push_back(session->entityId());
                    for (player::PlayerSession* watcher : playerRegistry_.snapshot()) {
                        if (watcher->worldName == session->worldName && watcher->connection().sendPassengers) {
                            watcher->connection().sendPassengers(target->entityId(), target->passengerIds);
                        }
                    }
                    return;
                }
                // parity: `Animal.mobInteract`, which only takes the food an animal of that kind eats.
                const std::optional<storage::StoredItemStack>& held = session->inventory.heldItem();
                if (!held.has_value()) {
                    return;
                }
                const std::expected<wire::common::Identifier, wire::common::IdentifierError> item =
                    wire::common::Identifier::parse(held->id);
                const std::int32_t heldSlot = player::PlayerInventory::kHotbarStart + session->inventory.heldSlot();
                // parity: `ZombieVillager.mobInteract`, where the apple is taken only when the cure starts.
                if (item.has_value() && item->path() == "golden_apple" && target->type().path() == "zombie_villager") {
                    if (mobActions_->startCure(*found->second, *target, session->uuid()) &&
                        session->gamemode != wire::protocol::play::Gamemode::Creative) {
                        storage::StoredItemStack left = *held;
                        left.count = held->count - 1;
                        session->inventory.set(heldSlot, left.count <= 0 ? std::nullopt : std::optional{left});
                    }
                    return;
                }
                if (!item.has_value() || !entity::breeding::isFood(target->type(), *item) ||
                    !entity::breeding::canFallInLove(*target)) {
                    return;
                }
                entity::breeding::feed(*target);
                if (session->gamemode == wire::protocol::play::Gamemode::Creative) {
                    return;
                }
                const std::int32_t slot = player::PlayerInventory::kHotbarStart + session->inventory.heldSlot();
                if (held->count <= 1) {
                    session->inventory.set(slot, std::nullopt);
                } else {
                    storage::StoredItemStack left = *held;
                    left.count = held->count - 1;
                    session->inventory.set(slot, left);
                }
                if (session->connection().sendInventoryAll) {
                    session->connection().sendInventoryAll();
                }
            });
        };

        // What they are standing on, gathered where the world is and told to them where they are.
        // A block a command changed, heard by everyone in that world. The broadcaster owns both the
        // list and the sending, so this only has to find the world.
        context.blockEffects = [this](const std::string& worldName, const std::int32_t x, const std::int32_t y,
                                      const std::int32_t z, const std::int32_t stateId,
                                      const std::vector<world::BlockEffect>& effects) {
            std::vector<player::PlayerSession*> watchers;
            for (player::PlayerSession* other : playerRegistry_.snapshot()) {
                if (other->worldName == worldName) {
                    watchers.push_back(other);
                }
            }
            broadcaster_->sendEffects(watchers, x, y, z, stateId, effects);
        };

        // An arm swung at nothing: everybody else has to be told, and the swinger draws its own.
        context.swing = [this](player::PlayerSession& player) {
            const std::int32_t who = player.entityId();
            const std::string worldName = player.worldName;
            for (player::PlayerSession* other : playerRegistry_.snapshot()) {
                if (other->entityId() == who || other->worldName != worldName) {
                    continue;
                }
                if (other->connection().sendSwing) {
                    other->connection().sendSwing(who, false);
                    continue;
                }
                if (other->connection().sendPlayPacket) {
                    const wire::protocol::play::AnimatePacket packet{
                        .entityId = who, .action = wire::protocol::play::AnimatePacket::kSwingMainHand};
                    other->connection().sendPlayPacket(
                        wire::protocol::play::AnimatePacket::kId,
                        [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
                }
            }
        };

        // The middle click, answered where the world is: what block is there decides what is handed
        // over, and only creative may be handed one it is not already carrying.
        context.pickBlock = [this](player::PlayerSession& player, const std::int32_t x, const std::int32_t y,
                                   const std::int32_t z, const std::int32_t hotbarSlot) {
            static_cast<void>(hotbarSlot);
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
                const std::optional<std::int32_t> stateId = found->second->state().blockState(x, y, z);
                const std::optional<server::registry::BlockState> block =
                    stateId.has_value() ? server::registry::blocks::byStateId(*stateId) : std::nullopt;
                if (!block.has_value()) {
                    return;
                }
                // parity: `Block.asItem`, which is not the block's own name for dust, crops or cocoa.
                const server::registry::Item* item =
                    server::registry::items::registry().byName(server::registry::itemBlocks::itemOf(block->block));
                if (item == nullptr || !player::pickItemFor(*session, item->id.toString())) {
                    return;
                }
                if (session->connection().setHeldSlot) {
                    session->connection().setHeldSlot(session->inventory.heldSlot());
                }
                if (session->connection().sendInventoryAll) {
                    session->connection().sendInventoryAll();
                }
            });
        };

        // The same click on something alive: a mob answers with its own spawn egg and most other
        // things answer with nothing at all.
        context.pickEntity = [this](player::PlayerSession& player, const std::int32_t entityId) {
            const wire::common::Uuid uuid = player.uuid();
            worldRegistry_->scheduler().global().submit([this, uuid, entityId] {
                player::PlayerSession* session = playerRegistry_.get(uuid);
                if (session == nullptr) {
                    return;
                }
                const std::map<std::string, world::LoadedWorld*> worlds = worldRegistry_->worlds();
                const auto found = worlds.find(session->worldName);
                if (found == worlds.end() || found->second == nullptr) {
                    return;
                }
                const entity::ServerEntity* target = found->second->state().entity(entityId);
                if (target == nullptr || target->removed) {
                    return;
                }
                // parity: `Entity.getPickResult`: a mob answers with its own spawn egg, a boat or a
                // minecart with what it was placed from, and everything else with nothing.
                const std::optional<wire::common::Identifier> item = entity::entityPickResults::itemFor(target->type());
                if (!item.has_value() || !player::pickItemFor(*session, item->toString())) {
                    return;
                }
                if (session->connection().setHeldSlot) {
                    session->connection().setHeldSlot(session->inventory.heldSlot());
                }
                if (session->connection().sendInventoryAll) {
                    session->connection().sendInventoryAll();
                }
            });
        };

        // One of a crafter's nine switched off, where the world is. The block keeps it, and a
        // comparator counts a slot that is off as a full one.
        context.toggleCrafterSlot = [this](player::PlayerSession& player, const std::int32_t x, const std::int32_t y,
                                           const std::int32_t z, const std::int32_t slot, const bool disabled) {
            const wire::common::Uuid uuid = player.uuid();
            worldRegistry_->scheduler().global().submit([this, uuid, x, y, z, slot, disabled] {
                player::PlayerSession* session = playerRegistry_.get(uuid);
                if (session == nullptr) {
                    return;
                }
                const std::map<std::string, world::LoadedWorld*> worlds = worldRegistry_->worlds();
                const auto found = worlds.find(session->worldName);
                if (found == worlds.end() || found->second == nullptr) {
                    return;
                }
                const std::optional<std::int32_t> stateId = found->second->state().blockState(x, y, z);
                const wire::common::Identifier* block =
                    stateId.has_value() ? server::registry::blockStateDefinitions::blockOf(*stateId) : nullptr;
                // parity: the packet reaches `CrafterBlockEntity` and nothing else.
                if (block == nullptr || *block != world::crafters::block()) {
                    return;
                }
                if (!world::crafters::setSlotState(found->second->state(), x, y, z, slot, !disabled)) {
                    return;
                }
                world::redstoneComponents::scheduleComparatorsAround(
                    found->second->state(), found->second->name(), x, y, z,
                    world::comparatorReading::of(found->second->state()));
            });
        };

        // An emote. The other edition has none, so it goes to the clients that can draw one and
        // nowhere else.
        context.emote = [this](player::PlayerSession& player, const std::string& emoteId, const std::int32_t duration,
                               const std::uint8_t flags) {
            const std::int32_t who = player.entityId();
            const std::string worldName = player.worldName;
            for (player::PlayerSession* other : playerRegistry_.snapshot()) {
                // The one who made it is already drawing it; this is for everybody else.
                if (other->entityId() == who || other->worldName != worldName || !other->connection().showEmote) {
                    continue;
                }
                other->connection().showEmote(who, emoteId, duration, flags);
            }
        };

        // A command block being programmed. The permission check is the other door's own, because it
        // is the same question: creative, and operator through their own command source.
        context.programCommandBlock = [this](player::PlayerSession& player, const std::int32_t x, const std::int32_t y,
                                             const std::int32_t z, const std::int32_t mode, const std::string& command,
                                             const bool trackOutput, const bool conditional, const bool needsRedstone) {
            const wire::common::Uuid uuid = player.uuid();
            worldRegistry_->scheduler().global().submit([this, uuid, x, y, z, mode, command, trackOutput, conditional,
                                                         needsRedstone] {
                player::PlayerSession* session = playerRegistry_.get(uuid);
                command::CommandDispatcher* dispatcher = wired_.dispatcher.get();
                if (session == nullptr || dispatcher == nullptr) {
                    return;
                }
                // parity: `Player.canUseGameMasterBlocks`, which is permission two and creative.
                const command::PlayerCommandSource source{*session, dispatcher->environment().groups()};
                if (session->gamemode != wire::protocol::play::Gamemode::Creative ||
                    source.permissionLevel() < command::permissionLevel::kGamemasters) {
                    session->sendSystemMessage(wire::text::Component::translatable("advMode.notAllowed"));
                    return;
                }
                const std::map<std::string, world::LoadedWorld*> worlds = worldRegistry_->worlds();
                const auto found = worlds.find(session->worldName);
                if (found == worlds.end() || found->second == nullptr) {
                    return;
                }
                // That edition numbers the three the same way this server does: impulse, then
                // repeating, then chain.
                const world::commandBlocks::Mode kind = mode == 1   ? world::commandBlocks::Mode::Repeating
                                                        : mode == 2 ? world::commandBlocks::Mode::Chain
                                                                    : world::commandBlocks::Mode::Impulse;
                const std::optional<world::BlockChange> moved = world::commandBlocks::program(
                    found->second->state(), x, y, z, command, kind, trackOutput, conditional, !needsRedstone);
                if (moved.has_value()) {
                    player::announceBlock(breakContext(), session->worldName, moved->x, moved->y, moved->z,
                                          moved->stateId);
                }
                if (command.empty()) {
                    return;
                }
                const bool works = found->second->gameRules().get(world::gameRules::kCommandBlocksWork);
                session->sendSystemMessage(wire::text::Component::translatable(works ? "advMode.setCommand.success"
                                                                                     : "advMode.setCommand.disabled",
                                                                               {wire::text::Component::text(command)}));
            });
        };

        // A page turned on a lectern. That edition sends the page it wants rather than a direction,
        // which is the only difference: the block keeps the page either way.
        context.turnLecternPage = [this](player::PlayerSession& player, const std::int32_t x, const std::int32_t y,
                                         const std::int32_t z, const std::int32_t page) {
            const wire::common::Uuid uuid = player.uuid();
            worldRegistry_->scheduler().global().submit([this, uuid, x, y, z, page] {
                player::PlayerSession* session = playerRegistry_.get(uuid);
                if (session == nullptr || page < 0) {
                    return;
                }
                const std::map<std::string, world::LoadedWorld*> worlds = worldRegistry_->worlds();
                const auto found = worlds.find(session->worldName);
                if (found == worlds.end() || found->second == nullptr) {
                    return;
                }
                const std::optional<world::BlockChange> change =
                    world::lecterns::turnPage(found->second->state(), x, y, z, page);
                if (!change.has_value()) {
                    return;
                }
                const player::BreakContext blocks = breakContext();
                player::announceBlock(blocks, session->worldName, change->x, change->y, change->z, change->stateId);
                player::notifyAround(blocks, *found->second, x, y, z, std::nullopt);
            });
        };

        // What a client typed into a sign's editor, written where the world is.
        // The book is the player's own, so both of these only need the thread that owns them.
        context.writeBookPage = [this](player::PlayerSession& player, const std::int32_t slot,
                                       const world::books::PageEdit what, const std::int32_t page,
                                       const std::int32_t other, const std::string& text) {
            const wire::common::Uuid uuid = player.uuid();
            worldRegistry_->scheduler().global().submit([this, uuid, slot, what, page, other, text] {
                player::PlayerSession* session = playerRegistry_.get(uuid);
                if (session != nullptr && player::editBookPageFor(*session, slot, what, page, other, text) &&
                    session->connection().sendInventoryAll) {
                    session->connection().sendInventoryAll();
                }
            });
        };
        context.signBook = [this](player::PlayerSession& player, const std::int32_t slot, const std::string& title) {
            const wire::common::Uuid uuid = player.uuid();
            worldRegistry_->scheduler().global().submit([this, uuid, slot, title] {
                player::PlayerSession* session = playerRegistry_.get(uuid);
                if (session == nullptr) {
                    return;
                }
                // parity: `signBook` keeps whatever pages the book already carries.
                const std::optional<std::int32_t> where =
                    slot >= 0 && slot < player::PlayerInventory::kHotbarSize
                        ? std::optional{player::PlayerInventory::kHotbarStart + slot}
                        : std::nullopt;
                const std::optional<storage::StoredItemStack>* held =
                    where.has_value() ? &session->inventory.at(*where) : nullptr;
                if (held == nullptr || !held->has_value()) {
                    return;
                }
                if (player::editBookFor(*session, slot, world::books::pagesOf(**held), title) &&
                    session->connection().sendInventoryAll) {
                    session->connection().sendInventoryAll();
                }
            });
        };
        context.writeSign = [this](player::PlayerSession& player, const std::int32_t x, const std::int32_t y,
                                   const std::int32_t z, const std::string& text) {
            const wire::common::Uuid uuid = player.uuid();
            worldRegistry_->scheduler().global().submit([this, uuid, x, y, z, text] {
                player::PlayerSession* session = playerRegistry_.get(uuid);
                if (session == nullptr) {
                    return;
                }
                const std::map<std::string, world::LoadedWorld*> worlds = worldRegistry_->worlds();
                const auto found = worlds.find(session->worldName);
                if (found == worlds.end() || found->second == nullptr) {
                    return;
                }
                // parity: that edition only ever edits the front here, which is the side this
                // server's own editor is opened on.
                static_cast<void>(player::writeSignFor(breakContext(), *found->second, *session, x, y, z, true,
                                                       player::signLinesOf(text)));
            });
        };

        // A bed whose sleeper left: this arrives from the door's own thread, so the world is
        // reached the way everything else from there is.
        context.freeBed = [this](const std::string& worldName, const std::int32_t x, const std::int32_t y,
                                 const std::int32_t z) {
            worldRegistry_->scheduler().global().submit([this, worldName, x, y, z] {
                const std::map<std::string, world::LoadedWorld*> worlds = worldRegistry_->worlds();
                const auto found = worlds.find(worldName);
                if (found == worlds.end() || found->second == nullptr) {
                    return;
                }
                player::freeBedAt(breakContext(), *found->second, x, y, z);
            });
        };

        // Morning reaches every connection on the world's own thread, so this runs where it is
        // called rather than going round the scheduler.
        context.wakeUp = [this](player::PlayerSession& player) {
            const std::map<std::string, world::LoadedWorld*> worlds = worldRegistry_->worlds();
            const auto found = worlds.find(player.worldName);
            player::wakeUpFor(breakContext(), player, found == worlds.end() ? nullptr : found->second);
        };

        // A click on nothing: what leaves the hand does so here, and a bow only starts bending.
        context.useInAir = [this](player::PlayerSession& player, const std::int32_t slot) {
            const wire::common::Uuid uuid = player.uuid();
            worldRegistry_->scheduler().global().submit([this, uuid, slot] {
                player::PlayerSession* session = playerRegistry_.get(uuid);
                const std::map<std::string, world::LoadedWorld*> worlds = worldRegistry_->worlds();
                const auto found = session == nullptr ? worlds.end() : worlds.find(session->worldName);
                if (found == worlds.end() || found->second == nullptr) {
                    return;
                }
                const std::optional<storage::StoredItemStack> held = session->inventory.at(slot);
                if (!held.has_value()) {
                    return;
                }
                const auto item = wire::common::Identifier::parse(held->id);
                // parity: `ItemCooldowns` gates the throw itself, not only the icon: a pearl that is
                // still cooling down goes nowhere on either door.
                if (!item.has_value() || player::onCooldownFor(*session, *item, found->second->gameTime())) {
                    return;
                }
                if (fishing_->useRod(breakContext(), *session, *found->second, slot)) {
                    return;
                }
                if (player::throwItemFor(breakContext(), *session, *found->second, *item, slot)) {
                    player::startCooldownFor(*session, *item, found->second->gameTime());
                }
            });
        };

        // Letting go of it, which for a bow is the shot and its arrow.
        context.releaseItem = [this](player::PlayerSession& player, const std::int32_t slot,
                                     const std::int32_t drawnTicks) {
            const wire::common::Uuid uuid = player.uuid();
            worldRegistry_->scheduler().global().submit([this, uuid, slot, drawnTicks] {
                player::PlayerSession* session = playerRegistry_.get(uuid);
                const std::map<std::string, world::LoadedWorld*> worlds = worldRegistry_->worlds();
                const auto found = session == nullptr ? worlds.end() : worlds.find(session->worldName);
                if (found == worlds.end() || found->second == nullptr) {
                    return;
                }
                player::shootBowFor(breakContext(), *session, *found->second, slot, drawnTicks, {});
            });
        };

        context.eat = [this](player::PlayerSession& player, const std::int32_t slot) {
            const wire::common::Uuid uuid = player.uuid();
            worldRegistry_->scheduler().global().submit([this, uuid, slot] {
                player::PlayerSession* session = playerRegistry_.get(uuid);
                if (session == nullptr) {
                    return;
                }
                const std::map<std::string, world::LoadedWorld*> worlds = worldRegistry_->worlds();
                const auto found = worlds.find(session->worldName);
                if (found == worlds.end() || found->second == nullptr) {
                    return;
                }
                static_cast<void>(player::eatHeldItem(breakContext(), *session, *found->second, slot, [] {
                    return std::uniform_real_distribution<float>{0.0F, 1.0F}(scratchRandom());
                }));
            });
        };

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
                                  const std::int32_t z, const std::int32_t ticks, std::function<void(float)> answer) {
            const wire::common::Uuid uuid = player.uuid();
            worldRegistry_->scheduler().global().submit([this, uuid, x, y, z, ticks, answer = std::move(answer)] {
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
                const float perTick = player::destroyProgressPerTickFor(
                    *session, *block, registry::blocks::hardnessOf(*block), breakContext().blockTags, underwater);
                answer(perTick);
                // parity: `ServerPlayerGameMode.destroyProgress` tells everybody else how far along
                // it is. The step is worked out from the turns because this edition never says.
                constexpr std::int32_t kStages = 10;
                const auto stage = std::min(kStages - 1, static_cast<std::int32_t>(static_cast<float>(ticks) * perTick *
                                                                                   static_cast<float>(kStages)));
                for (player::PlayerSession* other : player::playersIn(breakContext(), session->worldName)) {
                    if (other->entityId() == session->entityId() || !other->connection().sendBlockDestruction) {
                        continue;
                    }
                    other->connection().sendBlockDestruction(session->entityId(), x, y, z, stage, perTick);
                }
            });
        };

        context.digStopped = [this](player::PlayerSession& player, const std::int32_t x, const std::int32_t y,
                                    const std::int32_t z) {
            const wire::common::Uuid uuid = player.uuid();
            worldRegistry_->scheduler().global().submit([this, uuid, x, y, z] {
                player::PlayerSession* session = playerRegistry_.get(uuid);
                if (session == nullptr) {
                    return;
                }
                constexpr std::int32_t kNoCracks = 10;
                for (player::PlayerSession* other : player::playersIn(breakContext(), session->worldName)) {
                    if (other->entityId() == session->entityId() || !other->connection().sendBlockDestruction) {
                        continue;
                    }
                    other->connection().sendBlockDestruction(session->entityId(), x, y, z, kNoCracks, 0.0F);
                }
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
                            windows->trades.reset();
                            // The next window is a different window: what was said about the last
                            // one says nothing about this one.
                            windows->saidSlots = bedrock::readWindow(*found->second, *window, session);
                            windows->saidNumbers.reset();
                            windows->sinceRead = 0;
                            opened(*window, windows->saidSlots);
                            rollBedrockEnchantment(*session, *found->second, *windows);
                            openersChanged(*found->second, *window, true);
                            return;
                        }
                    }
                    // parity: `ServerPlayerGameMode.useItemOn` runs the block's own use first. A
                    // door, a gate, a lever, a button, a repeater or a comparator answers the hand
                    // rather than letting whatever is held be placed against it.
                    if (!session->sneaking) {
                        const player::BreakContext blocks = breakContext();
                        // parity: `BedBlock.useWithoutItem`. The spawn point and the plugin's say
                        // come from this side, the same as they do for the other door.
                        if (const std::optional<std::int32_t> clickedState =
                                found->second->state().blockState(hit.x, hit.y, hit.z);
                            clickedState.has_value() && world::beds::isBed(*clickedState)) {
                            player::SleepContext sleeping;
                            sleeping.blocks = blocks;
                            sleeping.setSpawn = [this](player::PlayerSession& who, const std::int32_t bedX,
                                                       const std::int32_t bedY, const std::int32_t bedZ) {
                                if (playerStore_ == nullptr || !playerStore_->isOpen()) {
                                    return;
                                }
                                playerStore_->access().setSpawnPoint(
                                    storage::StoredSpawnPoint{.uuid = who.uuid().toString(),
                                                              .world = who.worldName,
                                                              .x = bedX + 0.5,
                                                              .y = static_cast<double>(bedY),
                                                              .z = bedZ + 0.5,
                                                              .yaw = who.yaw});
                                if (who.connection().sendActionBar) {
                                    who.connection().sendActionBar(
                                        wire::text::Component::translatable("block.minecraft.set_spawn"));
                                }
                            };
                            sleeping.explode = [this](world::LoadedWorld& where, const double bx, const double by,
                                                      const double bz, const std::int32_t byEntityId) {
                                if (explosions_ == nullptr) {
                                    return;
                                }
                                std::vector<player::PlayerSession*> watchers;
                                for (player::PlayerSession* other : playerRegistry_.snapshot()) {
                                    if (other->worldName == where.name()) {
                                        watchers.push_back(other);
                                    }
                                }
                                explosions_->blowUp(where, watchers, bx, by, bz, world::explosions::kBedPower, "bed",
                                                    byEntityId);
                            };
                            static_cast<void>(player::sleepInBedFor(sleeping, *session, *found->second, hit.x, hit.y,
                                                                    hit.z, *clickedState));
                            return;
                        }
                        if (player::ringBellFor(blocks, *found->second, hit.x, hit.y, hit.z, hit.direction,
                                                hit.clickY) ||
                            player::tuneNoteBlockFor(blocks, *found->second, hit.x, hit.y, hit.z) ||
                            player::toggleBlockFor(blocks, *found->second, hit.x, hit.y, hit.z, session->yaw) ||
                            player::cycleBlockFor(blocks, *found->second, hit.x, hit.y, hit.z)) {
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
                    // parity: `ServerPlayerGameMode.useItemOn` with something in hand: these are
                    // the item's own use, which happens sneaking or not, unlike the block's.
                    const player::BreakContext blocks = breakContext();
                    // parity: `BucketItem.use` looks where the player is looking rather than at the
                    // block that was clicked, so this hands it the eye and not the hit.
                    // parity: `ThrowableProjectileItem.use`, which leaves the hand whatever is in
                    // front of it. This door hears it as a click on whatever the arc started at.
                    if (player::throwItemFor(blocks, *session, *found->second, *item, slot)) {
                        return;
                    }

                    // parity: `BucketItem.use` traces where the player is looking; this door is told
                    // the block and the face outright, so it goes straight to the same fill or pour.
                    if (player::useBucketOnFor(blocks, *session, *found->second, *item, slot, *held, hit.x, hit.y,
                                               hit.z, hit.direction, {})) {
                        return;
                    }

                    // parity: `TntBlock.useItemOn` runs before anything a flint and steel would do
                    // to the square beside it, and a fire charge is spent where the lighter is worn.
                    const bool lighter = *item == wire::common::Identifier::vanilla("flint_and_steel");
                    const bool charge = *item == wire::common::Identifier::vanilla("fire_charge");
                    const wire::common::Identifier* clicked = registry::blockStateDefinitions::blockOf(
                        found->second->state().blockState(hit.x, hit.y, hit.z).value_or(0));
                    if ((lighter || charge) && clicked != nullptr && clicked->path() == "tnt") {
                        player::primeTntFor(blocks, *session, *found->second, hit.x, hit.y, hit.z, slot, *held,
                                            lighter);
                        return;
                    }

                    if (lighter) {
                        // parity: `FlintAndSteelItem.useOn` lights the square beside the face that
                        // was clicked, not the block itself.
                        const wire::protocol::play::BlockPos beside =
                            wire::protocol::play::BlockPos{hit.x, hit.y, hit.z}.offset(hit.direction);
                        player::igniteFor(blocks, *session, *found->second, beside.x, beside.y, beside.z, slot, *held);
                        return;
                    }
                    // parity: `ChiseledBookShelfBlock.useItemOn` and `DecoratedPotBlock.useItemOn`,
                    // both of which move one item by hand and neither of which opens a screen.
                    if (!session->sneaking && clicked != nullptr) {
                        const player::FaceHit face{
                            .face = hit.direction, .x = hit.clickX, .y = hit.clickY, .z = hit.clickZ};
                        if (world::chiseledBookshelves::isBookshelf(clicked->path())) {
                            player::useBookshelfFor(blocks, *session, *found->second, hit.x, hit.y, hit.z, face, slot,
                                                    held, &vanillaData_.itemTags);
                            return;
                        }
                        if (world::decoratedPots::isPot(clicked->path())) {
                            player::usePotFor(blocks, *session, *found->second, hit.x, hit.y, hit.z, slot, held);
                            return;
                        }
                        // parity: `JukeboxBlock` and `LecternBlock` both answer the hand without a
                        // screen. A lectern that already holds a book opens one, which this door
                        // does not draw yet, so it falls through to placing instead.
                        if (clicked->path() == "jukebox" &&
                            player::useJukeboxFor(blocks, *session, *found->second, hit.x, hit.y, hit.z, slot, held)) {
                            return;
                        }
                        if (clicked->path() == "lectern" &&
                            player::placeBookOnLecternFor(blocks, *session, *found->second, hit.x, hit.y, hit.z, slot,
                                                          held)) {
                            return;
                        }
                        if (world::shelves::isShelf(clicked->path())) {
                            player::useShelfFor(blocks, *session, *found->second, hit.x, hit.y, hit.z, face, slot,
                                                held);
                            return;
                        }
                    }

                    // parity: a composter and a lit campfire take what is offered without a screen,
                    // and answer the click whether or not it changed anything.
                    if (!session->sneaking &&
                        (player::useComposterFor(blocks, *session, *found->second, hit.x, hit.y, hit.z, slot, held) ||
                         player::placeOnCampfireFor(blocks, *session, *found->second, hit.x, hit.y, hit.z, slot, held,
                                                    &vanillaData_.recipes))) {
                        return;
                    }
                    const bool answered =
                        (*item == wire::common::Identifier::vanilla("bone_meal") &&
                         player::useBoneMealFor(blocks, *session, *found->second, hit.x, hit.y, hit.z, slot, *held)) ||
                        (*item == wire::common::Identifier::vanilla("honeycomb") &&
                         player::useHoneycombFor(blocks, *session, *found->second, hit.x, hit.y, hit.z, slot, *held)) ||
                        (item->path().ends_with("_axe") &&
                         player::useAxeFor(blocks, *session, *found->second, hit.x, hit.y, hit.z, slot, *held));
                    if (answered) {
                        return;
                    }

                    // parity: `MinecartItem.useOn`, `BoatItem.use` and `SpawnEggItem.useOn`, the
                    // three items that put something alive down rather than a block.
                    const std::string path{item->path()};
                    const bool boat = path.ends_with("_boat") || path == "bamboo_raft" || path == "bamboo_chest_raft";
                    // parity: `HangingEntityItem.useOn`, which hangs it on the face that was clicked
                    // and answers nothing at all where it will not hold.
                    if ((path == "item_frame" || path == "glow_item_frame" || path == "painting") &&
                        player::hangFrameFor(blocks, *session, *found->second, *item, slot, *held, hit)) {
                        return;
                    }
                    if ((path.ends_with("minecart") &&
                         player::placeMinecartFor(blocks, *session, *found->second, *item, slot, *held, hit)) ||
                        (boat && player::placeBoatFor(blocks, *session, *found->second, *item, slot, *held, hit)) ||
                        (path.ends_with("_spawn_egg") &&
                         player::useSpawnEggFor(blocks, *session, *found->second, *item, slot, *held, hit))) {
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
                    placing.placed = [this](world::LoadedWorld& at, std::int32_t x, std::int32_t y, std::int32_t z) {
                        withers_->placed(at, x, y, z);
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
                windows->trades.reset();
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
        context.moveStacks = [this](player::PlayerSession& player, wire::bedrock::BedrockStackRequest request,
                                    std::shared_ptr<bedrock::BedrockWindows> windows,
                                    std::function<void(bool, std::vector<wire::bedrock::BedrockResponseContainer>)>
                                        answer) {
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
                const bool free = session->gamemode == wire::protocol::play::Gamemode::Creative;
                const bedrock::StackRequestAnswer applied = bedrock::applyStackRequest(
                    *session, *windows, where, request,
                    bedrock::StackRequestContext{
                        .drop = drop,
                        .resultOf = [this](const std::int32_t netId) { return bedrockRecipeResult(netId); },
                        .anvilResult =
                            [this](const storage::StoredItemStack* input, const storage::StoredItemStack* addition,
                                   const std::optional<std::string>& name) {
                                return world::anvils::combine(input, addition, name, world::anvils::isRepairMaterialFor,
                                                              world::anvils::enchantmentMerge(
                                                                  {.enchantments = &vanillaData_.enchantments,
                                                                   .itemTags = &vanillaData_.itemTags,
                                                                   .enchantmentTags = &vanillaData_.enchantmentTags}));
                            },
                        .canPay = [free, session](
                                      const std::int32_t cost) { return free || session->experience.level() >= cost; },
                        .grindstoneResult =
                            [this](const storage::StoredItemStack* input, const storage::StoredItemStack* addition) {
                                const world::EnchantmentBooks books{.enchantments = &vanillaData_.enchantments,
                                                                    .itemTags = &vanillaData_.itemTags,
                                                                    .enchantmentTags = &vanillaData_.enchantmentTags};
                                return world::grindstones::combine(
                                    input, addition,
                                    [books](const wire::common::Identifier& id) { return books.isCurse(id); },
                                    [books](const wire::common::Identifier& id, const std::int32_t level) {
                                        return books.minCost(id, level);
                                    });
                            },
                        .smithingResult =
                            [this](
                                const storage::StoredItemStack* templateStack, const storage::StoredItemStack* base,
                                const storage::StoredItemStack* addition) -> std::optional<storage::StoredItemStack> {
                            const auto asCrafting = [](const storage::StoredItemStack* stack)
                                -> std::optional<server::world::loot::ItemStack> {
                                if (stack == nullptr) {
                                    return std::nullopt;
                                }
                                const std::expected<wire::common::Identifier, wire::common::IdentifierError> id =
                                    wire::common::Identifier::parse(stack->id);
                                if (!id.has_value()) {
                                    return std::nullopt;
                                }
                                return server::world::loot::ItemStack{
                                    .item = *id, .count = stack->count, .components = stack->components};
                            };
                            const std::optional<server::world::loot::ItemStack> pattern = asCrafting(templateStack);
                            const std::optional<server::world::loot::ItemStack> under = asCrafting(base);
                            const std::optional<server::world::loot::ItemStack> onto = asCrafting(addition);
                            if (!under.has_value() || !onto.has_value()) {
                                return std::nullopt;
                            }
                            const server::world::crafting::Recipe* recipe = vanillaData_.recipes.smithingFor(
                                pattern.has_value() ? &*pattern : nullptr, &*under, &*onto);
                            const std::optional<server::world::loot::ItemStack> made =
                                recipe == nullptr ? std::nullopt : recipe->assembleSmithing(*under, *onto);
                            if (!made.has_value()) {
                                return std::nullopt;
                            }
                            return storage::StoredItemStack{
                                .id = made->item.toString(), .count = made->count, .components = made->components};
                        },
                        // parity: `LoomMenu.slotsChanged`, which only weaves a pattern the item in
                        // the third slot offers, or one of the free ones when it is empty.
                        .loomResult = [this](const storage::StoredItemStack& banner,
                                             const storage::StoredItemStack& dye,
                                             const storage::StoredItemStack* patternItem,
                                             const std::string& pattern) -> std::optional<storage::StoredItemStack> {
                            const std::optional<std::string> colour = world::looms::dyeColourOf(dye.id);
                            if (!colour.has_value() || pattern.empty() || !world::looms::isBanner(banner.id)) {
                                return std::nullopt;
                            }
                            const std::string tag =
                                patternItem == nullptr
                                    ? std::string{"minecraft:no_item_required"}
                                    : world::looms::patternTagOf(patternItem->id).value_or(std::string{});
                            const std::string named = "minecraft:" + pattern;
                            const std::vector<wire::common::Identifier>& offered =
                                world::realBannerPatterns::membersOf(vanillaData_.bannerPatterns, tag);
                            if (std::ranges::none_of(offered, [&named](const wire::common::Identifier& one) {
                                    return one.toString() == named;
                                })) {
                                return std::nullopt;
                            }
                            return world::looms::weave(banner, named, *colour);
                        }});
                answer(applied.accepted, applied.containers);
                if (!applied.accepted && session->connection().sendInventoryAll) {
                    session->connection().sendInventoryAll();
                }
                if (applied.tradedWith.has_value()) {
                    // parity: `MerchantMenu` hands the experience to the merchant, not to the
                    // buyer, and the book is drawn again so the used offer stops being offered.
                    if (where != nullptr) {
                        entity::ServerEntity* merchant = where->state().entity(*applied.tradedWith);
                        if (merchant != nullptr) {
                            merchant->villagerExperience += applied.tradeExperience;
                            // parity: `Villager.rewardTradeXp`, three to six points at the merchant.
                            random::RandomSource random =
                                random::RandomSource::legacy(static_cast<std::int64_t>(scratchRandom()()));
                            orbs_->spawn(*where, merchant->x, merchant->y + 0.5, merchant->z,
                                         world::trades::kTradeOrbBase + random.nextInt(4));
                        }
                    }
                    if (session->connection().sendInventoryAll) {
                        session->connection().sendInventoryAll();
                    }
                    if (session->connection().sendTrades) {
                        session->connection().sendTrades();
                    }
                }
                if (applied.enchantCost.has_value()) {
                    // parity: `clickMenuButton` charges the row's own number plus one in levels and
                    // moves the seed on, which is what gives the next item new rows.
                    if (!free) {
                        session->experience.giveLevels(-*applied.enchantCost);
                        if (session->connection().syncExperience) {
                            session->connection().syncExperience();
                        }
                    }
                    session->stats.increment(storage::stats::enchantItem());
                    session->enchantmentSeed = static_cast<std::int32_t>(scratchRandom()());
                }
                if (applied.accepted && where != nullptr && windows->open.has_value() &&
                    windows->open->type == wire::bedrock::BedrockContainerType::Enchantment) {
                    // What is in it decides what it offers, so every move over that screen rolls
                    // the three rows again.
                    rollBedrockEnchantment(*session, *where, *windows);
                }
                if (applied.grindstoneWorth.has_value() && windows->open.has_value()) {
                    // parity: `GrindstoneMenu.ResultSlot.onTake` drops the experience as orbs at
                    // the middle of the block.
                    const std::int32_t half = (*applied.grindstoneWorth + 1) / 2;
                    const auto roll = static_cast<std::int32_t>(
                        std::uniform_real_distribution<double>{0.0, 1.0}(scratchRandom()) * static_cast<double>(half));
                    const std::int32_t points = world::grindstones::experienceFor(*applied.grindstoneWorth, roll);
                    if (points > 0 && where != nullptr) {
                        orbs_->award(*where, windows->open->x + 0.5, windows->open->y + 0.5, windows->open->z + 0.5,
                                     points);
                    }
                    if (where != nullptr) {
                        const std::int32_t stateId =
                            where->state().blockState(windows->open->x, windows->open->y, windows->open->z).value_or(0);
                        broadcaster_->sendEffects(
                            player::playersIn(breakContext(), session->worldName), windows->open->x, windows->open->y,
                            windows->open->z, stateId,
                            {world::blockEffect::Sound{.name = "block.grindstone.use",
                                                       .source = wire::protocol::play::SoundSource::Blocks,
                                                       .volume = 1.0F,
                                                       .pitch = 1.0F}});
                    }
                    if (session->connection().sendInventoryAll) {
                        session->connection().sendInventoryAll();
                    }
                }
                if (applied.beacon.has_value() && where != nullptr && windows->open.has_value()) {
                    // parity: `BeaconMenu.updateEffects` refuses a pair the pyramid is not tall
                    // enough for, and the payment has already gone either way.
                    server::world::Chunk* chunk =
                        where->state().chunkIfLoaded(windows->open->x >> 4, windows->open->z >> 4);
                    server::world::ChunkBlockEntity* entity =
                        chunk == nullptr
                            ? nullptr
                            : chunk->blockEntityAt(windows->open->x & 15, windows->open->y, windows->open->z & 15);
                    if (entity != nullptr) {
                        world::beacons::Stored stored = world::beacons::read(entity->data());
                        const auto named = [](const std::optional<std::int32_t>& id) {
                            const std::optional<wire::common::Identifier> effect =
                                id.has_value() ? world::mobEffects::byProtocolId(*id) : std::nullopt;
                            return effect.has_value() ? effect->toString() : std::string{};
                        };
                        const std::string primary = named(applied.beacon->primary);
                        const std::string secondary = named(applied.beacon->secondary);
                        if (world::beacons::allows(primary, secondary, stored.levels)) {
                            stored.primary = primary;
                            stored.secondary = secondary;
                            world::beacons::write(stored, entity->data());
                            chunk->markDirty();
                        }
                    }
                }
                if (applied.anvilCost.has_value() && windows->open.has_value()) {
                    if (!free) {
                        session->experience.giveLevels(-*applied.anvilCost);
                        if (session->connection().syncExperience) {
                            session->connection().syncExperience();
                        }
                    }
                    // parity: `AnvilMenu.onTake`: creative wears nothing out, and a block that
                    // goes takes the screen standing over it with it.
                    const bool gone = !free && where != nullptr &&
                                      player::wearAnvilAt(
                                          breakContext(), *where, windows->open->x, windows->open->y, windows->open->z,
                                          std::uniform_real_distribution<float>{0.0F, 1.0F}(scratchRandom()));
                    if (gone) {
                        windows->open.reset();
                        windows->saidSlots.clear();
                    }
                    if (session->connection().sendInventoryAll) {
                        session->connection().sendInventoryAll();
                    }
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
