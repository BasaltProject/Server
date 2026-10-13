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
#include <basalt/server/runtime/bedrock/BedrockShield.hpp>
#include <basalt/server/runtime/config/ForwardingConfig.hpp>
#include <basalt/server/runtime/config/GameplayRules.hpp>
#include <basalt/server/runtime/entity/Breeding.hpp>
#include <basalt/server/runtime/entity/EntityCombat.hpp>
#include <basalt/server/runtime/entity/MobRules.hpp>
#include <basalt/server/runtime/entity/Villagers.hpp>
#include <basalt/server/runtime/entity/villager/VillagerBrain.hpp>
#include <basalt/server/runtime/item/ItemDamageEffects.hpp>
#include <basalt/server/runtime/item/ItemWear.hpp>
#include <basalt/server/runtime/player/Combat.hpp>
#include <basalt/server/runtime/player/PlayerAnvil.hpp>
#include <basalt/server/runtime/player/PlayerArrowPickup.hpp>
#include <basalt/server/runtime/player/PlayerAttributes.hpp>
#include <basalt/server/runtime/player/PlayerBlockBreak.hpp>
#include <basalt/server/runtime/player/PlayerBlockItemUse.hpp>
#include <basalt/server/runtime/player/PlayerBlockPlace.hpp>
#include <basalt/server/runtime/player/PlayerBlockSlots.hpp>
#include <basalt/server/runtime/player/PlayerBlockToggle.hpp>
#include <basalt/server/runtime/player/PlayerBooks.hpp>
#include <basalt/server/runtime/player/PlayerBuckets.hpp>
#include <basalt/server/runtime/player/PlayerCakesAndPots.hpp>
#include <basalt/server/runtime/player/PlayerCauldrons.hpp>
#include <basalt/server/runtime/player/PlayerContainers.hpp>
#include <basalt/server/runtime/player/PlayerCooldowns.hpp>
#include <basalt/server/runtime/player/PlayerDamage.hpp>
#include <basalt/server/runtime/player/PlayerDeath.hpp>
#include <basalt/server/runtime/player/PlayerDigSpeed.hpp>
#include <basalt/server/runtime/player/PlayerEating.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/player/PlayerItemPickup.hpp>
#include <basalt/server/runtime/player/PlayerMace.hpp>
#include <basalt/server/runtime/player/PlayerParticles.hpp>
#include <basalt/server/runtime/player/PlayerPick.hpp>
#include <basalt/server/runtime/player/PlayerSigns.hpp>
#include <basalt/server/runtime/player/PlayerSleep.hpp>
#include <basalt/server/runtime/player/PlayerSounds.hpp>
#include <basalt/server/runtime/player/PlayerSpawnItems.hpp>
#include <basalt/server/runtime/player/PlayerThrown.hpp>
#include <basalt/server/runtime/player/StatAwards.hpp>
#include <basalt/server/runtime/world/AntiXray.hpp>
#include <basalt/server/runtime/world/AnvilEnchantments.hpp>
#include <basalt/server/runtime/world/Beacons.hpp>
#include <basalt/server/runtime/world/Beds.hpp>
#include <basalt/server/runtime/world/BreedingCooldowns.hpp>
#include <basalt/server/runtime/world/ChiseledBookshelves.hpp>
#include <basalt/server/runtime/world/CommandBlocks.hpp>
#include <basalt/server/runtime/world/ComparatorReading.hpp>
#include <basalt/server/runtime/world/ContainerOpeners.hpp>
#include <basalt/server/runtime/world/Crafters.hpp>
#include <basalt/server/runtime/world/DecoratedPots.hpp>
#include <basalt/server/runtime/world/DetectorRails.hpp>
#include <basalt/server/runtime/world/Enchanting.hpp>
#include <basalt/server/runtime/world/EnchantmentBooks.hpp>
#include <basalt/server/runtime/world/EnderEyes.hpp>
#include <basalt/server/runtime/world/Fluids.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
#include <basalt/server/runtime/world/Grindstones.hpp>
#include <basalt/server/runtime/world/Lecterns.hpp>
#include <basalt/server/runtime/world/Looms.hpp>
#include <basalt/server/runtime/world/RealEnchantments.hpp>
#include <basalt/server/runtime/world/RedstoneComponents.hpp>
#include <basalt/server/runtime/world/Shelves.hpp>
#include <basalt/server/runtime/world/TriggerFacts.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>
#include <basalt/server/runtime/world/WeaponDamage.hpp>
#include <basalt/server/runtime/world/maps/Cartography.hpp>
#include <basalt/server/world/GenerationMetrics.hpp>
#include <basalt/wire/bedrock/BedrockEntityIdentifiers.hpp>
#include <basalt/wire/io/Platform.hpp>
#include <basalt/wire/multiversion/ProtocolVersions.hpp>
#include <basalt/wire/protocol/play/BlockPos.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>
#include <basalt/wire/text/MiniMessage.hpp>
#include <basalt/wire/text/Serializers.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <random>
#include <thread>

namespace basalt::server::runtime {
namespace {

[[nodiscard]] std::mt19937_64& scratchRandom() {
    static thread_local std::mt19937_64 random{std::random_device{}()};
    return random;
}

// Five ticks: long enough that no honest handler reaches it, short enough to be felt once.
constexpr std::int64_t kPluginWatchdogIntervalMillis = 250;
constexpr std::int64_t kPluginStuckMillis = 250;

constexpr std::int64_t kShutdownDrainTimeoutMillis = 5000;

// Generous on purpose: cutting a large world's save short is far worse than waiting for it.
constexpr std::int64_t kShutdownWaitTimeoutMillis = 120000;

/// parity: `Player.drop`.
constexpr double kThrowEyeHeight = 1.5;
constexpr double kThrowLift = 0.1;

/// parity: `ItemEntity`'s own delay, so a dropped stack is not picked straight back up.
constexpr std::int32_t kPickupDelay = 40;

} // namespace

void BasaltServer::start(const std::chrono::steady_clock::time_point startedAt) {
    // The driver opens lazily; querying here surfaces a bad path before the first player joins and creates the file.
    logger_.info([this] {
        return "Player database ready: " + std::to_string(playerStore_->players().count()) + " player(s) known.";
    });
    if (config_->javaEnabled) {
        const auto address =
            wire::io::TcpAddress::literal(config_->javaBind, static_cast<std::uint16_t>(config_->port));
        if (!address)
            throw std::runtime_error("Invalid Java bind address: " + config_->javaBind);
        std::expected<wire::io::TcpListener, wire::io::SocketError> bound =
            wire::io::TcpListener::bind(*address, 128, config_->javaDualStack);
        if (!bound.has_value()) {
            throw std::runtime_error("Could not bind " + config_->javaBind + " port " + std::to_string(config_->port) +
                                     ". Another server is probably already running on it; stop it or change `port` in "
                                     "[network.java] of config/server.toml.");
        }
        listener_ = std::move(*bound);
    }
    startBedrockListener();
    startRemote(startedAt);

    // Logged at startup because regional mode changes the observable tick order.
    if (worldRegistry_->scheduler().regional()) {
        logger_.warn([] {
            return std::string{"Regions are on, which is NOT VANILLA: two of them have no fixed order of tick "} +
                   "between each other, and what crosses a border is deferred a tick.";
        });
    }

    // Loaded before the loop so plugin code never races the tick thread over its own state.
    if (config_->pluginsEnabled) {
        static_cast<void>(plugins_->loadAll());
        reportOrphanedPluginData();
        startPluginWatchdog();
    } else {
        logger_.info([] { return "Plugins are off in server.toml."; });
    }

    startBukkit();

    // Queued on the owner, since a decider only runs where the plugins do.
    worldRegistry_->scheduler().global().submit([this] {
        server::plugins::PluginEventBus& bus = plugins_->events();
        for (const auto& [name, world] : worldRegistry_->worlds()) {
            static_cast<void>(
                server::plugins::emit(&bus, &server::plugins::pluginEventTypes::worldLoad(),
                                      [&name](server::plugins::PluginEvent& event) { event.set(0, name); }));
        }
    });
    for (const auto& [name, world] : worldRegistry_->worlds()) {
        wireWorld(name, *world);
    }
    // Emitted after the plugins are up, so the first thing a plugin is told is that everything is there.
    static_cast<void>(server::plugins::emit(&plugins_->events(), &server::plugins::pluginEventTypes::serverLoad(),
                                            [this](server::plugins::PluginEvent& event) {
                                                event.set(0,
                                                          static_cast<std::int32_t>(worldRegistry_->worlds().size()));
                                                event.set(1, static_cast<std::int32_t>(plugins_->statuses().size()));
                                            }));

    tickLoop_->start();
    if (config_->tuning.watchdogSeconds > 0) {
        serverWatchdog_ = std::make_unique<ServerWatchdog>(
            std::chrono::seconds{config_->tuning.watchdogSeconds}, [this] { return tickLoop_->ticksRun(); },
            [this](double stalledSeconds) {
                logger_.error([stalledSeconds] {
                    return "A single server tick took " + std::to_string(stalledSeconds) +
                           " seconds (should be max 0.05); considering it to be crashed, the server will forcibly "
                           "shut down.";
                });
                std::fflush(nullptr);
                std::_Exit(1);
            });
    }
    enableBukkit();
    runStartupCommands();
    startQuery();
    const double elapsedSeconds = static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                                          std::chrono::steady_clock::now() - startedAt)
                                                          .count()) /
                                  1000.0;
    std::set<std::int32_t> translated = config_->javaEnabled ? protocolGate_.supported() : std::set<std::int32_t>{};
    translated.erase(wire::multiversion::protocolVersions::kNative);
    logger_.info([this, elapsedSeconds, &translated] {
        std::string line = "Done (" + std::to_string(elapsedSeconds) + "s)! ";
        const bool java = config_->javaEnabled && listener_.has_value();
        if (java) {
            line += "Java on TCP port " + std::to_string(config_->port) + " (" +
                    (options_.sessionService != nullptr ? "online" : "offline") + "-mode)";
            if (!translated.empty()) {
                line += " with " + std::to_string(translated.size()) + " older versions down to " +
                        wire::multiversion::protocolVersions::describe(*translated.begin());
            }
        }
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
        auto finished = std::make_shared<std::atomic<bool>>(false);
        connections_.push_back(
            ConnectionThread{finished, std::jthread{[this, finished, connection = std::move(*accepted)]() mutable {
                                 serveConnection(std::move(connection));
                                 finished->store(true);
                             }}});
        std::erase_if(connections_, [](const ConnectionThread& entry) { return entry.finished->load(); });
    }

    awaitShutdown();
}

void BasaltServer::wireWorld(const std::string& name, world::LoadedWorld& worldRef) {
    world::LoadedWorld* world = &worldRef;
    world->border().absoluteMaxSize = std::min(world->border().absoluteMaxSize, config_->tuning.maxWorldSize);
    world->state().setOnChunkInstalled([this, worldName = name](server::world::Chunk& chunk) {
        static_cast<void>(server::plugins::emit(&plugins_->events(),
                                                &server::plugins::pluginEventTypes::worldChunkLoad(),
                                                [&worldName, &chunk](server::plugins::PluginEvent& event) {
                                                    event.set(0, worldName);
                                                    event.set(1, chunk.chunkX());
                                                    event.set(2, chunk.chunkZ());
                                                    event.set(3, chunk.takeFresh());
                                                }));
    });
    // parity: `ServerLevel.sendParticles`, which only reaches players within thirty-two blocks.
    world->state().setVibrationParticleSender(
        [this, worldName = name](double x, double y, double z,
                                 const world::WorldState::ParticleDestination& destination, std::int32_t arrivalTicks) {
            const server::registry::ParticleType* type =
                server::registry::particleTypes::of(wire::common::Identifier::vanilla("vibration"));
            if (type == nullptr) {
                return false;
            }
            wire::protocol::play::ParticleOptions::Vibration vibration;
            if (const auto& entity = destination.entity; entity.has_value()) {
                vibration.target =
                    wire::protocol::play::ParticleOptions::Vibration::EntityTarget{entity->first, entity->second};
            } else if (const auto& block = destination.block; block.has_value()) {
                vibration.target = wire::protocol::play::BlockPos{(*block)[0], (*block)[1], (*block)[2]};
            } else {
                return false;
            }
            vibration.arrivalTicks = arrivalTicks;
            const wire::protocol::play::LevelParticlesPacket packet{
                .particleTypeId = type->protocolId,
                .x = x,
                .y = y,
                .z = z,
                .count = 1,
                .options = wire::protocol::play::ParticleOptions{vibration}};
            constexpr double kParticleRange = 32.0;
            bool anyone = false;
            for (player::PlayerSession* player : playerRegistry_.snapshot()) {
                if (player->worldName != worldName) {
                    continue;
                }
                const double dx = player->x - x;
                const double dy = player->y - y;
                const double dz = player->z - z;
                if ((dx * dx) + (dy * dy) + (dz * dz) >= kParticleRange * kParticleRange) {
                    continue;
                }
                player::showParticleFor(*player, "minecraft:vibration", packet);
                anyone = true;
            }
            return anyone;
        });
}

void BasaltServer::shutdown() {
    // Runs on whichever thread asked, nearly always the tick thread; once only, since `/stop` and a signal can race.
    bool expected = false;
    if (!shuttingDown_.compare_exchange_strong(expected, true)) {
        return;
    }

    // First, so an auto-reconnecting client cannot walk back in mid-save.
    accepting_.store(false);
    stopRemote();
    // Closed, not destroyed: the thread that started the server is inside `accept` on it.
    if (listener_.has_value()) {
        listener_->close();
    }
    // Only the door: the thread stays up to carry the goodbye to Bedrock clients.
    if (bedrockListener_) {
        bedrockListener_->closeDoor();
    }

    const std::vector<player::PlayerSession*> players = playerRegistry_.snapshot();
    if (!players.empty()) {
        // Saved synchronously before disconnecting: sessions saving on their own threads raced the database close.
        logger_.info([&players] { return "Saving " + std::to_string(players.size()) + " player(s)."; });
        persistence_->persistPlayers();

        logger_.info([&players] { return "Disconnecting " + std::to_string(players.size()) + " player(s)."; });
        const std::shared_ptr<const config::ConfigSnapshot> snapshot = live_->current();
        const std::string* custom = snapshot->messages.find("server-closed");
        const wire::text::Component serverClosed =
            custom != nullptr ? wire::text::miniMessage::parse(*custom)
                              : wire::text::Component::text("Server closed").color(wire::text::kRed);
        for (player::PlayerSession* player : players) {
            if (player->connection().disconnect) {
                player->connection().disconnect(serverClosed);
            }
            playerRegistry_.remove(player->uuid());
        }
    }
    if (bedrockListener_) {
        bedrockListener_->stop();
    }

    serverWatchdog_.reset();
    query_.reset();
    tickLoop_->stop();
    static_cast<void>(server::plugins::emit(
        &plugins_->events(), &server::plugins::pluginEventTypes::serverStop(),
        [&players](server::plugins::PluginEvent& event) { event.set(0, static_cast<std::int32_t>(players.size())); }));
    bukkit_.reset();
    plugins_->unloadAll();
    for (const auto& [name, world] : worldRegistry_->worlds()) {
        world->stopGenerating(kShutdownDrainTimeoutMillis);
    }

    if (server::world::generationMetrics::enabled()) {
        logger_.info([] { return server::world::generationMetrics::report(); });
    }
    logger_.info([this] { return "Saving " + std::to_string(worldRegistry_->worlds().size()) + " world(s)."; });
    for (const auto& [name, world] : worldRegistry_->worlds()) {
        static_cast<void>(server::plugins::emit(&plugins_->events(), &server::plugins::pluginEventTypes::worldSave(),
                                                [&name](server::plugins::PluginEvent& event) { event.set(0, name); }));
        world->saveDirty();
        world->flushRegions();
        // The clock, weather and rules no chunk carries are written with the rest, not only by an autosave.
        world->saveMetadata();
    }
    commandStorage_.save();
    maps_->save();
    saveSchedule();
    saveScoreboard();
    saveBossBars();
    saveStopwatches();
    // Queued work is cancelled rather than run against worlds that are already written.
    worldRegistry_->scheduler().close();
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
    // Own thread, since a watchdog on the thread it watches is not one; it covers a plugin stuck inside the host.
    watchdog_ = std::jthread{[this](const std::stop_token& stop) {
        while (!stop.stop_requested() && accepting_.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds{kPluginWatchdogIntervalMillis});
            static_cast<void>(plugins_->interruptIfStuck(kPluginStuckMillis));
        }
    }};
}

void BasaltServer::reportOrphanedPluginData() {
    // Nothing is deleted: a plugin taken out for an afternoon must find its data again.
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
    context.triggers = advancementTriggers_.get();
    context.nextDouble = [] { return std::uniform_real_distribution<double>{0.0, 1.0}(scratchRandom()); };
    context.nextSeed = [] { return static_cast<std::int64_t>(scratchRandom()()); };
    context.bedLeft = [this](const player::PlayerSession& who, std::int32_t x, std::int32_t y, std::int32_t z) {
        eventSink_.notify(&server::plugins::pluginEventTypes::playerBed, [&](server::plugins::PluginEvent& event) {
            plugins::EventSink::player(event, who);
            event.set("x", static_cast<std::int64_t>(x));
            event.set("y", static_cast<std::int64_t>(y));
            event.set("z", static_cast<std::int64_t>(z));
            event.set("entering", false);
        });
    };
    context.awardExperience = [this](world::LoadedWorld& world, double x, double y, double z, std::int32_t amount) {
        if (orbs_ != nullptr) {
            const ExperienceOrbSystem::ReasonScope reason{ExperienceOrbSystem::Reason::BlockBreak};
            orbs_->award(world, x, y, z, amount);
        }
    };
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
    context.triggers = advancementTriggers_.get();
    context.damageTypeTags = &vanillaData_.damageTypeTags;
    context.enchantments = &vanillaData_.enchantments;
    context.teams = &teams_;
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

// A double chest counts as two, because a lid is per block entity.
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

    // A list naming a protocol this build has no codec for is reported rather than accepted.
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

    hooks.recipes = [this] { return bedrockRecipeBook().recipes; };

    // Asked before the world is described: `start_game` names a position and a later teleport would be refused.
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

    hooks.join =
        [this](const bedrock::BedrockListener::Joining& joining) -> std::unique_ptr<bedrock::BedrockListener::Player> {
        bedrock::BedrockPlayer::Context context;
        context.registry = &playerRegistry_;
        // Queued onto the owner because a decider only runs where plugins do, with copies since the player may be gone.
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
        // Chunks are read and written on the region's owner; only finished bytes cross to the Bedrock thread.
        context.sendChunks = [this](const player::PlayerSession* viewer,
                                    std::vector<std::pair<std::int32_t, std::int32_t>> columns, const int protocol,
                                    std::function<void(std::vector<std::uint8_t>)> send, std::function<void()> done) {
            if (worldRegistry_ == nullptr) {
                return;
            }
            std::string worldName = viewer == nullptr ? std::string{} : viewer->worldName;
            std::optional<std::string> group = viewer == nullptr ? std::nullopt : viewer->group;
            const std::optional<bool> grant =
                viewer == nullptr ? std::nullopt : viewer->permissionGrant("basalt.antixray.bypass");
            worldRegistry_->scheduler().global().submit([this, worldName = std::move(worldName),
                                                         group = std::move(group), grant, columns = std::move(columns),
                                                         protocol, send = std::move(send), done = std::move(done)] {
                const auto worlds = namedWorlds();
                const auto named = worlds.find(worldName);
                world::LoadedWorld& world =
                    named != worlds.end() && named->second != nullptr ? *named->second : worldRegistry_->defaultWorld();
                const world::AntiXray* antiXray = world.antiXray();
                const bool bypass =
                    antiXray != nullptr && antiXray->usePermission() &&
                    grant.value_or(wired_.dispatcher->environment().groups().resolve(group, "basalt.antixray.bypass"));
                for (const auto& [x, z] : columns) {
                    const server::world::Chunk& chunk = world.state().getOrGenerateChunk(x, z);
                    std::vector<std::vector<std::int32_t>> hidden;
                    if (antiXray != nullptr && !bypass) {
                        hidden =
                            antiXray->obfuscate(chunk, [&world](const std::int32_t chunkX, const std::int32_t chunkZ) {
                                return static_cast<const server::world::Chunk*>(
                                    world.state().chunkIfLoaded(chunkX, chunkZ));
                            });
                    }
                    send(bedrock::writeColumn(chunk, protocol, hidden));
                }
                if (done) {
                    done();
                }
            });
        };

        context.stored = [this](const wire::common::Uuid& uuid) -> std::optional<storage::StoredPlayer> {
            if (playerStore_ == nullptr || !playerStore_->isOpen()) {
                return std::nullopt;
            }
            return playerStore_->players().load(uuid.toString());
        };
        context.save = [this](const storage::StoredPlayer& row) {
            // The database closes at shutdown after everyone was saved, so this is only the door catching up.
            if (playerStore_ != nullptr && playerStore_->isOpen()) {
                playerStore_->players().save(row);
            }
        };

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

        // Queued to the owner: dying touches the registry and every other player's connection.
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
                if (target == nullptr) {
                    for (player::PlayerSession* other : playerRegistry_.snapshot()) {
                        if (other->entityId() == entityId && other != session &&
                            other->worldName == session->worldName) {
                            bedrockAttackPlayer(*session, *other, world, falling);
                            return;
                        }
                    }
                }
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
                world::RealEnchantments* enchantments = &vanillaData_.enchantments;
                const player::playerAttributes::DefinitionOf definitionOf =
                    [enchantments](const wire::common::Identifier& id) { return enchantments->definitionOf(id); };
                const float strength = player::combat::attackStrengthScale(
                    session->ticksSinceSwing,
                    player::playerAttributes::valueOf(*session, "minecraft:attack_speed", definitionOf));
                session->ticksSinceSwing = 0;
                const auto base = static_cast<float>(
                    player::playerAttributes::valueOf(*session, "minecraft:attack_damage", definitionOf));
                const world::realEntityTypeTags::TagMembers* entityTags = &vanillaData_.entityTags;
                const world::weaponDamage::Victim victim{
                    .type = target->type(), .inTag = [entityTags, target](const wire::common::Identifier& tag) {
                        return world::realEntityTypeTags::membersOf(*entityTags, tag).contains(target->type());
                    }};
                const float enchanted = world::weaponDamage::of(held, base, definitionOf, &victim);
                // parity: `MaceItem.getAttackDamageBonus` and its `mace_smash` source.
                const bool smash = player::mace::smashes(*session, held);
                const float bonus = player::mace::damageBonus(*session, held, definitionOf);
                const player::combat::Swing swing = player::combat::swingOf(
                    strength, base, enchanted, player::pvp::criticalPossible(*session, world, falling),
                    session->onGround, session->sprinting, bonus,
                    world.state().gameplay().player.criticalDamageMultiplier);
                if (swing.damage <= 0.0F) {
                    return;
                }

                entity::entityCombat::blame(*target, session->entityId(), session->entityId(),
                                            wire::common::Identifier::vanilla("player"));
                const entity::entityCombat::Result result = entity::entityCombat::hurt(
                    *target, swing.damage, session->x, session->z, {}, smash ? "mace_smash" : "attack", session->name(),
                    true, player::mace::armorFractionAdd(held, definitionOf));
                if (!result.landed) {
                    return;
                }
                if (advancementTriggers_ != nullptr) {
                    world::triggerFacts::playerHurtEntity(*advancementTriggers_, world, *session, *target, swing.damage,
                                                          smash ? world::damageTypes::kMaceSmash
                                                                : world::damageTypes::kPlayerAttack);
                }
                const player::BreakContext blocks = breakContext();
                const player::pvp::Context melee = meleeContext(blocks);
                // parity: `causeExtraKnockback`, the weapon's and the sprint's, along the facing.
                player::pvp::knockMob(*session, *target,
                                      player::pvp::knockbackOf(melee, *session, held) +
                                          (swing.knockbackAttack ? 0.5F : 0.0F));
                // parity: `isSweepAttack`.
                if (swing.fullStrength && !swing.critical && !swing.knockbackAttack && session->onGround &&
                    weapon.has_value() && weapon->path().ends_with("_sword")) {
                    player::pvp::Context sweeping = melee;
                    const std::string worldName = session->worldName;
                    sweeping.sweptMob = [this, worldName](entity::ServerEntity& struck, bool died) {
                        for (player::PlayerSession* watcher : playerRegistry_.snapshot()) {
                            if (watcher->worldName == worldName && watcher->connection().playSound) {
                                watcher->connection().playSound(died ? entity::entityCombat::deathSound(struck)
                                                                     : entity::entityCombat::hurtSound(struck),
                                                                struck.x, struck.y, struck.z, 1.0F, 1.0F);
                            }
                        }
                    };
                    player::pvp::sweep(
                        *session, world, sweeping, target->entityId(),
                        entity::Aabb::of(target->x, target->y, target->z, target->width(), target->height()),
                        (base * player::combat::chargeScale(strength)) + bonus, strength, held);
                }
                // parity: `HurtByTargetGoal` and `PanicGoal` both read the last thing that hurt it.
                target->hurtBy = session->entityId();
                entity::entityCombat::rememberPlayer(*target, session->uuid(),
                                                     world.state().gameplay().mechanics.mobLastHurtByPlayerTime);
                // parity: `itemAttackInteraction`.
                player::pvp::itemAttackInteraction(melee, world, *session, nullptr, target, held,
                                                   player::PlayerInventory::kHotbarStart +
                                                       session->inventory.heldSlot());
                world::vibrations::emit(world.state(), "entity_damage", target->x, target->y, target->z,
                                        world.gameTime(), {.entity = target->uuid()});
                for (player::PlayerSession* watcher : playerRegistry_.snapshot()) {
                    if (watcher->worldName != session->worldName) {
                        continue;
                    }
                    player::playSoundFor(*watcher,
                                         result.died ? entity::entityCombat::deathSound(*target)
                                                     : entity::entityCombat::hurtSound(*target),
                                         wire::protocol::play::SoundSource::Hostile, target->x, target->y, target->z,
                                         1.0F, 1.0F, 0);
                    // parity: `ServerPlayer.crit`.
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
                entity::villagers::offersOf(vanillaData_.villagerTrades, *target, *found->second);
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
                // parity: `ZombieVillager.mobInteract`.
                if (item.has_value() && item->path() == "golden_apple" && target->type().path() == "zombie_villager") {
                    if (mobActions_->startCure(*found->second, *target, session->uuid()) &&
                        session->gamemode != wire::protocol::play::Gamemode::Creative) {
                        storage::StoredItemStack left = *held;
                        left.count = held->count - 1;
                        session->inventory.set(heldSlot, left.count <= 0 ? std::nullopt : std::optional{left});
                    }
                    return;
                }
                if (!item.has_value() || !entity::breeding::isFood(*target, *item)) {
                    return;
                }
                // parity: `Animal.mobInteract`: love for a grown one, a faster childhood for a calf.
                const std::optional<world::TriggerItem> used = world::triggerFacts::itemOf(held);
                const bool waiting = found->second->state().gameplay().mechanics.animalBreedingCooldownSeconds > 0 &&
                                     world::breedingCooldowns::active(session->uuid().toString(), target->type(),
                                                                      found->second->gameTime());
                if (entity::breeding::canFallInLove(*target) && !waiting) {
                    entity::breeding::feed(*target, session->uuid().toString());
                } else if (entity::breeding::canAgeUp(*target)) {
                    entity::breeding::feedBaby(*target);
                } else {
                    return;
                }
                if (advancementTriggers_ != nullptr) {
                    advancementTriggers_->fire(
                        "player_interacted_with_entity", session->name(), session->uuid().toString(),
                        world::TriggerEvent{
                            .entities = {{"entity", world::triggerFacts::ofEntity(*found->second, *target)}},
                            .items = {{"item", used.value_or(world::TriggerItem{.item = *item})}}});
                }
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
                    const wire::protocol::play::SwingAnimationPacket packet{.entityId = who};
                    other->connection().sendPlayPacket(
                        wire::protocol::play::SwingAnimationPacket::kId,
                        [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
                }
            }
        };

        // Only creative may be handed a block it is not already carrying.
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
                // parity: `Entity.getPickResult`.
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

        // A comparator counts a switched-off crafter slot as full.
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

        context.emote = [this](player::PlayerSession& player, const std::string& emoteId, const std::int32_t duration,
                               const std::uint8_t flags) {
            const std::int32_t who = player.entityId();
            const std::string worldName = player.worldName;
            for (player::PlayerSession* other : playerRegistry_.snapshot()) {
                if (other->entityId() == who || other->worldName != worldName || !other->connection().showEmote) {
                    continue;
                }
                other->connection().showEmote(who, emoteId, duration, flags);
            }
        };

        // The permission check is the Java door's own: creative, and operator through their own command source.
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

        // The book is the player's own, so only the thread that owns the player is needed.
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
        context.mapInfo = [this](player::PlayerSession& player, const std::int64_t mapId) {
            const wire::common::Uuid uuid = player.uuid();
            worldRegistry_->scheduler().global().submit([this, uuid, mapId] {
                if (const player::PlayerSession* session = playerRegistry_.get(uuid); session != nullptr) {
                    maps_->sendWhole(*session, static_cast<std::int32_t>(mapId));
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
                static_cast<void>(player::writeSignFor(breakContext(), *found->second, *session, x, y, z, true,
                                                       player::signLinesOf(text)));
            });
        };

        // Arrives from the door's own thread, so the world is reached through the scheduler.
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

        // Morning reaches every connection on the world's own thread, so this runs inline.
        context.wakeUp = [this](player::PlayerSession& player) {
            const std::map<std::string, world::LoadedWorld*> worlds = worldRegistry_->worlds();
            const auto found = worlds.find(player.worldName);
            player::wakeUpFor(breakContext(), player, found == worlds.end() ? nullptr : found->second);
        };

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
                // parity: `ItemCooldowns`.
                if (!item.has_value() || player::onCooldownFor(*session, *item, found->second->gameTime())) {
                    return;
                }
                if (fishing_->useRod(breakContext(), *session, *found->second, slot)) {
                    return;
                }
                if (maps_->useEmptyMap(breakContext(), *session, *found->second, slot)) {
                    return;
                }
                if (world::enderEyes::throwFor(breakContext(), *session, *found->second, slot)) {
                    return;
                }
                if (player::throwItemFor(breakContext(), *session, *found->second, *item, slot)) {
                    const config::GameplayRules::Items& rules = found->second->state().gameplay().items;
                    if (item->path() == "ender_pearl") {
                        player::startCooldownFor(*session, *item, found->second->gameTime(),
                                                 session->gamemode == wire::protocol::play::Gamemode::Creative
                                                     ? rules.enderPearlCreativeCooldown
                                                     : rules.enderPearlCooldown);
                    } else {
                        player::startCooldownFor(*session, *item, found->second->gameTime());
                    }
                }
            });
        };

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
                bedrock::shield::tick(*session, found->second->gameTime(), playerRegistry_.snapshot());
                // parity: `AbstractArrow.playerTouch`, the same arrows a Java player picks up.
                const std::vector<player::PickedArrow> arrows =
                    player::pickupNearbyArrows(*session, found->second->state());
                for (const player::PickedArrow& arrow : arrows) {
                    for (const player::PlayerSession* watcher : playerRegistry_.snapshot()) {
                        if (watcher->worldName == session->worldName && watcher->connection().sendPickupAnimation) {
                            watcher->connection().sendPickupAnimation(arrow.entityId, session->entityId());
                        }
                    }
                }
                const std::vector<player::PickedItem> taken =
                    player::pickupNearbyItems(*session, found->second->state(), pluginHooks().pickupAllowed);
                if (taken.empty()) {
                    if (!arrows.empty() && session->connection().sendInventoryAll) {
                        session->connection().sendInventoryAll();
                    }
                    return;
                }
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
                // parity: `ServerPlayerGameMode.destroyProgress`.
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

        // The client's claimed held item is ignored: the slot is asked and what is in it goes down.
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
                    // parity: `ServerPlayerGameMode.useItemOn`.
                    if (!session->sneaking && windows != nullptr && opened) {
                        if (std::optional<bedrock::OpenWindow> window =
                                bedrock::windowFor(*found->second, hit.x, hit.y, hit.z);
                            window.has_value()) {
                            windows->nextContainerId =
                                windows->nextContainerId >= bedrock::kMaxContainerId ? 1 : windows->nextContainerId + 1;
                            window->containerId = windows->nextContainerId;
                            windows->open = *window;
                            windows->trades.reset();
                            // parity: `RandomizableContainer.unpackLootTable` before the menu reads a slot.
                            if (!window->playerOwned && window->size > 0) {
                                world::WorldState& state = found->second->state();
                                state.unpackLoot(window->x, window->y, window->z, session->uuid());
                                if (window->second.has_value()) {
                                    state.unpackLoot((*window->second)[0], (*window->second)[1], (*window->second)[2],
                                                     session->uuid());
                                }
                            }
                            // The next window is a different window: earlier slot echoes say nothing about it.
                            windows->saidSlots = bedrock::readWindow(*found->second, *window, session);
                            windows->saidNumbers.reset();
                            windows->sinceRead = 0;
                            opened(*window, windows->saidSlots);
                            rollBedrockEnchantment(*session, *found->second, *windows);
                            openersChanged(*found->second, *window, true);
                            return;
                        }
                    }
                    // parity: `ServerPlayerGameMode.useItemOn`.
                    if (!session->sneaking) {
                        const player::BreakContext blocks = breakContext();
                        // parity: `BedBlock.useWithoutItem`.
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
                                const config::GameplayRules::Blocks& blocks = where.state().gameplay().blocks;
                                explosions_->blowUp(where, watchers, bx, by, bz, blocks.bedExplosionPower, "bed",
                                                    byEntityId, {}, {}, blocks.bedExplosionFire);
                            };
                            static_cast<void>(player::sleepInBedFor(sleeping, *session, *found->second, hit.x, hit.y,
                                                                    hit.z, *clickedState));
                            return;
                        }
                        if (player::ringBellFor(blocks, *found->second, hit.x, hit.y, hit.z, hit.direction, hit.clickY,
                                                session) ||
                            player::tuneNoteBlockFor(blocks, *found->second, hit.x, hit.y, hit.z, session) ||
                            player::toggleBlockFor(blocks, *found->second, hit.x, hit.y, hit.z, session->yaw) ||
                            player::cycleBlockFor(blocks, *found->second, hit.x, hit.y, hit.z)) {
                            return;
                        }
                        const std::int32_t usedSlot = player::PlayerInventory::kHotbarStart + hotbarSlot;
                        if (player::useCauldronFor(blocks, *session, *found->second, hit.x, hit.y, hit.z, usedSlot,
                                                   session->inventory.at(usedSlot)) ||
                            player::useCakeFor(blocks, *session, *found->second, hit.x, hit.y, hit.z, usedSlot,
                                               session->inventory.at(usedSlot), hit.clickY) ||
                            player::useFlowerPotFor(blocks, *session, *found->second, hit.x, hit.y, hit.z, usedSlot,
                                                    session->inventory.at(usedSlot))) {
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
                    // parity: `ServerPlayerGameMode.useItemOn`.
                    const player::BreakContext blocks = breakContext();
                    // parity: `MapItem.useOn`.
                    if (maps_->useOnBanner(*session, *found->second, slot, hit.x, hit.y, hit.z).has_value() ||
                        maps_->useEmptyMap(blocks, *session, *found->second, slot)) {
                        return;
                    }
                    // parity: `BucketItem.use`.
                    // parity: `ThrowableProjectileItem.use`.
                    if (world::enderEyes::throwFor(blocks, *session, *found->second, slot) ||
                        player::throwItemFor(blocks, *session, *found->second, *item, slot)) {
                        return;
                    }

                    // parity: `BucketItem.use`.
                    if (player::useBucketOnFor(blocks, *session, *found->second, *item, slot, *held, hit.x, hit.y,
                                               hit.z, hit.direction, {})) {
                        return;
                    }

                    // parity: `TntBlock.useItemOn`.
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
                        // parity: `FlintAndSteelItem.useOn`.
                        const wire::protocol::play::BlockPos beside =
                            wire::protocol::play::BlockPos{hit.x, hit.y, hit.z}.offset(hit.direction);
                        player::igniteFor(blocks, *session, *found->second, beside.x, beside.y, beside.z, slot, *held);
                        return;
                    }
                    // parity: `ChiseledBookShelfBlock.useItemOn`.
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
                        // parity: `JukeboxBlock`.
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

                    // parity: `MinecartItem.useOn`.
                    const std::string path{item->path()};
                    const bool boat = path.ends_with("_boat") || path == "bamboo_raft" || path == "bamboo_chest_raft";
                    // parity: `HangingEntityItem.useOn`.
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
                    // Without this a Bedrock player places a sign and is never asked for text.
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

        // Whatever was being dragged goes back in the bag rather than staying in a closed window.
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
                // parity: `AbstractContainerMenu.removed`.
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
                        // parity: `LoomMenu.slotsChanged`.
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
                        },
                        .cartographyResult =
                            [this](const storage::StoredItemStack& map, const storage::StoredItemStack& additional) {
                                return maps_ == nullptr ? std::nullopt
                                                        : world::maps::cartographyResult(map, additional, *maps_,
                                                                                         &vanillaData_.itemTags);
                            }});
                answer(applied.accepted, applied.containers);
                if (!applied.accepted && session->connection().sendInventoryAll) {
                    session->connection().sendInventoryAll();
                }
                // parity: `MapItem.onCraftedPostProcess`.
                if (applied.accepted && maps_ != nullptr && where != nullptr &&
                    world::maps::settleCraftedMaps(*session, &windows->cursor, *maps_, *where) &&
                    session->connection().sendInventoryAll) {
                    session->connection().sendInventoryAll();
                }
                if (applied.tradedWith.has_value()) {
                    // parity: `MerchantMenu`.
                    if (where != nullptr) {
                        entity::ServerEntity* merchant = where->state().entity(*applied.tradedWith);
                        if (merchant != nullptr && mobActions_ != nullptr) {
                            // parity: `Villager.rewardTradeXp`, promotion included.
                            random::RandomSource random =
                                random::RandomSource::legacy(static_cast<std::int64_t>(scratchRandom()()));
                            const std::int32_t orb = entity::villager::villagerBrain::rewardTradeXp(
                                *merchant, mobActions_->villagersIn(*where), applied.tradeExperience, random);
                            const ExperienceOrbSystem::ReasonScope reason{ExperienceOrbSystem::Reason::VillagerTrade,
                                                                          merchant->uuid()};
                            orbs_->spawn(*where, merchant->x, merchant->y + 0.5, merchant->z, orb);
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
                    // parity: `clickMenuButton`.
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
                    rollBedrockEnchantment(*session, *where, *windows);
                }
                if (applied.grindstoneWorth.has_value() && windows->open.has_value()) {
                    // parity: `GrindstoneMenu.ResultSlot.onTake`.
                    const std::int32_t half = (*applied.grindstoneWorth + 1) / 2;
                    const auto roll = static_cast<std::int32_t>(
                        std::uniform_real_distribution<double>{0.0, 1.0}(scratchRandom()) * static_cast<double>(half));
                    const std::int32_t points = world::grindstones::experienceFor(*applied.grindstoneWorth, roll);
                    if (points > 0 && where != nullptr) {
                        const ExperienceOrbSystem::ReasonScope reason{ExperienceOrbSystem::Reason::Grindstone};
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
                    // parity: `BeaconMenu.updateEffects`.
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
                    // parity: `AnvilMenu.onTake`.
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

        // Read off the dispatcher rather than queued: the tree only changes when a plugin or group does.
        context.commandList =
            [this](const player::PlayerSession& player) -> std::vector<std::pair<std::string, std::string>> {
            command::CommandDispatcher* dispatcher = wired_.dispatcher.get();
            // The source wants the registry-owned session, not the const view.
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
                if (dispatcher->environment().commands().options().logCommands) {
                    logger_.info([&name, &line] { return name + " issued server command: /" + line; });
                }
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
        context.uiConfigs = [this] { return live_->current()->ui; };
        context.placeholders = &placeholders_;
        if (worldRegistry_ != nullptr) {
            const world::LoadedWorld& world = worldRegistry_->defaultWorld();
            const std::array<double, 3>& spawn = world.spawn();
            context.worldName = world.name();
            context.spawnX = spawn[0];
            context.spawnY = spawn[1];
            context.spawnZ = spawn[2];
            context.worldTime = static_cast<std::int32_t>(world.clock().totalTicks());
            context.weather = world.weather();
        }
        return std::make_unique<bedrock::BedrockPlayer>(joining, std::move(context));
    };

    bedrockListener_ = std::make_unique<bedrock::BedrockListener>(std::move(options), std::move(hooks),
                                                                  std::string{bedrock::kBedrockGameVersion});
    if (!bedrockListener_->start()) {
        // It already logged why; dropping it keeps the startup line honest.
        bedrockListener_.reset();
    }
}

} // namespace basalt::server::runtime
