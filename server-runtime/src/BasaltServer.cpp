#include <basalt/server/plugins/PluginEventTypes.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/BasaltServer.hpp>
#include <basalt/server/runtime/BuildInfo.hpp>
#include <basalt/server/runtime/placeholder/AnimationPlaceholders.hpp>
#include <basalt/server/runtime/placeholder/PlayerPlaceholders.hpp>
#include <basalt/server/runtime/placeholder/ServerPlaceholders.hpp>
#include <basalt/server/runtime/plugins/PluginsCommand.hpp>
#include <basalt/server/runtime/world/EndPodium.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
#include <basalt/server/runtime/world/RecipeBook.hpp>
#include <basalt/wire/auth/MojangSessionService.hpp>
#include <basalt/wire/multiversion/java/JavaTranslators.hpp>
#include <basalt/wire/text/TranslationResolver.hpp>

#include <cstdint>
#include <random>
#include <utility>

namespace basalt::server::runtime {
namespace {

/// Beyond the view distance, so a chunk already requested by a session is still there when that
/// session gets round to sending it.
constexpr std::int32_t kRetainMarginChunks = 4;

/// One source for everything that needs a number nobody is checking bit for bit: a sound's seed,
/// a scatter, a loot roll outside a world's own stream.
[[nodiscard]] std::mt19937_64& scratchRandom() {
    static thread_local std::mt19937_64 kRandom{std::random_device{}()};
    return kRandom;
}

[[nodiscard]] std::int64_t scratchSeed() { return static_cast<std::int64_t>(scratchRandom()()); }

[[nodiscard]] double scratchDouble() { return std::uniform_real_distribution<double>{0.0, 1.0}(scratchRandom()); }

[[nodiscard]] wire::protocol::play::Gamemode gamemodeOf(std::string_view name) {
    if (name == "creative") {
        return wire::protocol::play::Gamemode::Creative;
    }
    if (name == "adventure") {
        return wire::protocol::play::Gamemode::Adventure;
    }
    if (name == "spectator") {
        return wire::protocol::play::Gamemode::Spectator;
    }
    return wire::protocol::play::Gamemode::Survival;
}

} // namespace

BasaltServer::BasaltServer(const config::BasaltConfig& config, const config::WorldsConfig& worldsConfig,
                           config::LiveConfigs& live, Options options)
    : config_(&config), worldsConfig_(&worldsConfig), live_(&live), options_(std::move(options)),
      protocolGate_(options_.compatibility.gate(wire::multiversion::java::javaTranslators::supported(), logger_)),
      vanillaData_(VanillaGameData::jarOf(options_.serverJar), logger_), chunkSimulation_(playerRegistry_) {
    if (options_.compatibility.nameSubstitutes()) {
        substitutionLabels_ = std::make_unique<SubstituteLabels>();
    }
    // A percentage needs two readings; this is the first, so nobody asking later gets nothing.
    cpuMeter_.prime();

    // Player state, deliberately outside every world directory: the database sits beside `worlds/`,
    // so a world can be deleted, copied or reset without touching a single row.
    std::expected<std::unique_ptr<storage::PlayerStore>, storage::PlayerDatabaseSchemaError> store =
        storage::PlayerStore::open(options_.playersDatabasePath);
    if (!store.has_value()) {
        // Nothing downstream can run without it, and there are no migrations to fall back on.
        throw std::runtime_error("players.db: " + store.error().message);
    }
    playerStore_ = std::move(*store);
    playerProgress_ = std::make_unique<world::PlayerProgress>(&playerStore_->progress());

    // What turns playing into an advancement. The announcement goes to everyone, and only when the
    // default world's rule allows it: game rules are per world, and an advancement belongs to the
    // player rather than to the world they happen to stand in.
    world::AdvancementTriggers::Hooks triggerHooks;
    triggerHooks.itemTagMembers = [this](const wire::common::Identifier& tag) {
        return world::realItemTags::membersOf(vanillaData_.itemTags, tag.toString());
    };
    triggerHooks.announce = [this](std::string_view, const wire::text::Component& message) {
        if (worldRegistry_ == nullptr ||
            !worldRegistry_->defaultWorld().gameRules().get(world::gameRules::kShowAdvancementMessages)) {
            return;
        }
        for (player::PlayerSession* player : playerRegistry_.snapshot()) {
            if (player->connection().sendSystemMessage) {
                player->connection().sendSystemMessage(message);
            }
        }
        // Resolved, not serialized: a client translates the key itself and a log cannot.
        logger_.info([this, &message] { return wire::text::resolvePlainText(message, options_.lang); });
    };
    triggerHooks.awardExperience = [this](std::string_view name, std::int32_t amount) {
        for (player::PlayerSession* player : playerRegistry_.snapshot()) {
            if (player->name() != name) {
                continue;
            }
            player->experience.givePoints(amount);
            if (player->connection().syncExperience) {
                player->connection().syncExperience();
            }
        }
    };
    triggerHooks.awardRecipe = [this](std::string_view name, const wire::common::Identifier& recipe) {
        for (player::PlayerSession* player : playerRegistry_.snapshot()) {
            if (player->name() == name) {
                world::recipeBook::add(*player, vanillaData_.recipes, {recipe});
            }
        }
    };
    advancementTriggers_ = std::make_unique<world::AdvancementTriggers>(vanillaData_.advancements, *playerProgress_,
                                                                        std::move(triggerHooks));

    const VanillaGameData::Jar jar = VanillaGameData::jarOf(options_.serverJar);
    spawnEquipment_ = std::make_unique<world::SpawnEquipmentFinalizer>(
        world::SpawnEquipmentJar{.entriesUnder = jar.entriesUnder, .readText = jar.readText});

    WorldRegistry::Hooks worldHooks;
    worldHooks.worldLoaded = [this](const std::string& name) {
        server::plugins::PluginEventBus* bus = plugins_ == nullptr ? nullptr : &plugins_->events();
        static_cast<void>(
            server::plugins::emit(bus, bus == nullptr ? nullptr : &server::plugins::pluginEventTypes::worldLoad(),
                                  [&name](server::plugins::PluginEvent& event) { event.set(0, name); }));
    };
    worldHooks.rebuildCommands = [this] {
        if (wired_.environment != nullptr) {
            wired_.environment->setWorlds(namedWorlds());
        }
        if (pluginServices_ != nullptr) {
            pluginServices_->refreshCommands();
        }
    };
    worldRegistry_ = std::make_unique<WorldRegistry>(
        config, worldsConfig, options_.generators, playerRegistry_, std::move(worldHooks),
        WorldRegistry::Options{.worldsRoot = options_.worldsRoot,
                               .timings = &timings_,
                               .generationMetrics = config.generationMetrics,
                               .jukeboxes = &vanillaData_.jukeboxSongs},
        logger_);

    for (const auto& [name, world] : worldRegistry_->worlds()) {
        logger_.info([world] {
            return "Loading world '" + world->name() + "' (seed " + std::to_string(world->metadata().seed) + ").";
        });
        world->warmSpawnArea(kSpawnRadiusChunks);
        // Not parity: the fight builds it the first time a player is in the End, not on load. See
        // the note on `endPodium`.
        if (world->declaration().dimension == config::WorldDimension::End) {
            const std::optional<std::array<std::int32_t, 3>> origin =
                world::endPodium::ensure(world->state(), world->settings().openEndExitPortal);
            if (origin.has_value()) {
                logger_.info([world, &origin] {
                    return "Built the End's exit podium at " + std::to_string((*origin)[0]) + ", " +
                           std::to_string((*origin)[1]) + ", " + std::to_string((*origin)[2]) + " (" +
                           (world->settings().openEndExitPortal ? "open" : "closed") + ").";
                });
            }
        }
        world->saveDirty();
    }

    // Every placeholder this server answers, in one registry shared by everything that renders
    // operator-written text.
    placeholders_.registerProvider(
        "basalt_player",
        placeholder::playerPlaceholders(placeholder::PlayerPlaceholderSources{
            .groups = [this]() -> const config::GroupsConfig& { return live_->current()->groups; },
            .world = [this](std::string_view name) -> const world::LoadedWorld* {
                const std::map<std::string, world::LoadedWorld*> loaded = worldRegistry_->worlds();
                const auto found = loaded.find(std::string{name});
                return found == loaded.end() ? nullptr : found->second;
            }}),
        placeholder::playerPlaceholderNames());
    placeholders_.registerProvider(
        std::string{placeholder::kAnimationIdentifier},
        placeholder::animationPlaceholders(placeholder::AnimationPlaceholderSources{
            .animations = [this]() -> const config::AnimationsConfig& { return live_->current()->animations; },
            .registry = [this]() -> const wire::common::placeholder::PlaceholderRegistry& { return placeholders_; },
            .nowMillis =
                [] {
                    return std::chrono::duration_cast<std::chrono::milliseconds>(
                               std::chrono::system_clock::now().time_since_epoch())
                        .count();
                }}));
    placeholder::ServerFacts facts;
    facts.versionName = std::string{buildInfo::kMinecraftVersion};
    facts.protocolVersion = buildInfo::kProtocolVersion;
    facts.maxPlayers = [this] { return config_->maxPlayers; };
    facts.onlineByWorld = [this] {
        std::map<std::string, std::int32_t> counted;
        for (const player::PlayerSession* player : playerRegistry_.snapshot()) {
            counted[player->worldName]++;
        }
        return counted;
    };
    facts.uniqueJoins = [this] { return static_cast<std::int64_t>(playerStore_->players().count()); };
    facts.loadedChunks = [this] {
        std::int32_t total = 0;
        for (const auto& [name, world] : worldRegistry_->worlds()) {
            total += static_cast<std::int32_t>(world->state().loadedChunkCount());
        }
        return total;
    };
    facts.liveHeapBytes = [this] { return metrics_.residentBytes(); };
    facts.tickRates = [this]() -> std::optional<std::array<double, 3>> {
        const TickRates rates = tickLoop_->tickRates();
        return std::array<double, 3>{rates.oneMinute, rates.fiveMinutes, rates.fifteenMinutes};
    };
    facts.cpuPercent = [this] { return cpuMeter_.percent(); };
    facts.startedAtMillis =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
            .count();
    facts.nowMillis = [] {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
            .count();
    };
    placeholders_.registerProvider("basalt_server", placeholder::serverPlaceholders(std::move(facts)));

    tickLoop_ = std::make_unique<TickLoop>(
        TickLoop::Options{.saveIntervalSeconds = worldRegistry_->defaultWorld().settings().saveIntervalSeconds,
                          .onSaveInterval =
                              [this] {
                                  if (saveControl_ != nullptr && saveControl_->enabled()) {
                                      persistence_->persistEverything();
                                  }
                              },
                          .onUnloadInterval = [this] { persistence_->unloadUnreachableChunks(); },
                          .onTick = [this](bool worldRuns) { tickWorlds(worldRuns); }});

    functions_.reload();
    functionRunner_ = std::make_unique<command::commands::FunctionRunner>(functions_);
    consoleSource_ = std::make_unique<command::ConsoleCommandSource>(
        logger_, options_.lang, [this] { return worldRegistry_->defaultWorld().spawn(); });
    stopwatches_ = std::make_unique<command::commands::Stopwatches>([this] {
        return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - serverClock_)
            .count();
    });
    commandStorage_.load();

    PersistenceCoordinator::Hooks persistenceHooks;
    persistenceHooks.worlds = [this] { return worldRegistry_->worlds(); };
    persistenceHooks.wantsUnloadEvents = [this] {
        return plugins_ != nullptr && plugins_->events().listening(server::plugins::pluginEventTypes::worldChunkLoad());
    };
    persistenceHooks.chunkUnloaded = [this](const std::string& worldName, std::int32_t chunkX, std::int32_t chunkZ) {
        server::plugins::PluginEventBus* bus = plugins_ == nullptr ? nullptr : &plugins_->events();
        static_cast<void>(
            server::plugins::emit(bus, bus == nullptr ? nullptr : &server::plugins::pluginEventTypes::worldChunkLoad(),
                                  [&worldName, chunkX, chunkZ](server::plugins::PluginEvent& event) {
                                      event.set(0, worldName);
                                      event.set(1, static_cast<std::int64_t>(chunkX));
                                      event.set(2, static_cast<std::int64_t>(chunkZ));
                                      event.set(3, false);
                                  }));
    };
    persistence_ = std::make_unique<PersistenceCoordinator>(
        std::move(persistenceHooks), playerRegistry_, *playerStore_, commandStorage_, metrics_,
        PersistenceCoordinator::Options{.spawnRadiusChunks = kSpawnRadiusChunks,
                                        .retainMarginChunks = kRetainMarginChunks},
        logger_);
    saveControl_ = std::make_unique<command::commands::SaveControl>([this] { persistence_->persistEverything(); });

    timings::ReportSources sources;
    sources.versionName = std::string{buildInfo::kMinecraftVersion};
    sources.protocolVersion = buildInfo::kProtocolVersion;
    sources.uptimeSeconds = [this] {
        return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - serverClock_)
            .count();
    };
    sources.onlinePlayers = [this] { return static_cast<std::int32_t>(playerRegistry_.snapshot().size()); };
    sources.maxPlayers = [this] { return config_->maxPlayers; };
    sources.loadedChunks = [this] {
        std::int32_t total = 0;
        for (const auto& [name, world] : worldRegistry_->worlds()) {
            total += static_cast<std::int32_t>(world->state().loadedChunkCount());
        }
        return total;
    };
    timingsReport_ = std::make_unique<timings::TimingsReport>(timings_, tickLoop_->durations(), std::move(sources));
    whitelistEnforced_ = config.whitelist;

    CommandWiring wiring;
    wiring.players = &playerRegistry_;
    wiring.live = live_;
    wiring.playerStore = playerStore_.get();
    wiring.gameData = &vanillaData_;
    wiring.scoreboard = &scoreboard_;
    wiring.teams = &teams_;
    wiring.progress = playerProgress_.get();
    wiring.advancementTriggers = advancementTriggers_.get();
    wiring.functions = &functions_;
    wiring.functionRunner = functionRunner_.get();
    wiring.functionSchedule = &functionSchedule_;
    wiring.bossBars = &bossBars_;
    wiring.commandStorage = &commandStorage_;
    wiring.stopwatches = stopwatches_.get();
    wiring.saveControl = saveControl_.get();
    wiring.timingsReport = timingsReport_.get();
    wiring.timings = &timings_;
    wiring.tickDurations = &tickLoop_->durations();
    wiring.tickLoop = tickLoop_.get();
    wiring.placeholders = &placeholders_;
    wiring.protocolGate = &protocolGate_;
    wiring.worlds = namedWorlds();
    wiring.defaultWorldName = worldRegistry_->defaultWorld().name();
    wiring.worldSeed = worldRegistry_->worldSeed();
    wiring.maxPlayers = config.maxPlayers;
    wiring.port = config.port;
    wiring.lang = options_.lang;
    wiring.pluginEvents = [this] { return plugins_ == nullptr ? nullptr : &plugins_->events(); };
    wiring.stopServer = [this] { shutdown(); };
    wiring.dropBrokenBlock = [this](world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                    std::int32_t destroyedState) {
        broadcaster_->dropBrokenBlock(world, x, y, z, destroyedState);
    };
    wiring.summonGolem = [this](world::LoadedWorld& world, std::string_view type, double x, double y, double z) {
        if (spawning_ == nullptr) {
            return;
        }
        static_cast<void>(spawning_->spawnMob(world, wire::common::Identifier::vanilla(std::string{type}), x, y, z,
                                              0.0F, "triggered", {}));
    };
    wiring.requestRestart = [this] { requestRestart(); };
    wiring.onConfigsReloaded = [this] { worldRegistry_->onConfigsReloaded(); };
    wiring.nowNanos = [this] {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - serverClock_)
            .count();
    };
    wiring.nowMillis = [this] {
        return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - serverClock_)
            .count();
    };
    wiring.nowEpochSeconds = [] {
        return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch())
            .count();
    };
    wiring.nextDouble = [] { return scratchDouble(); };
    wiring.nextSeed = [] { return scratchSeed(); };
    wiring.fetchMojangUuid = [](const std::string& name) { return wire::auth::mojangUuidOf(name); };
    wiring.whitelistEnforced = [this] { return whitelistEnforced_; };
    wiring.setWhitelistEnforced = [this](bool value) { whitelistEnforced_ = value; };
    wiring.setIdleTimeout = [this](std::int32_t minutes) { idleTimeoutMinutes_ = minutes; };
    wired_ = buildCommands(wiring);

    // The seam the plugins see the server through. Everything they run goes on the global owner: it
    // is the one owner that exists in both scheduler modes and is a single thread in both.
    plugins::ModelHooks model;
    model.worldFor = [this](const std::string& name) -> world::LoadedWorld* {
        const std::map<std::string, world::LoadedWorld*> loaded = worldRegistry_->worlds();
        const auto found = loaded.find(name);
        return found == loaded.end() ? nullptr : found->second;
    };
    model.allWorlds = [this] {
        std::vector<world::LoadedWorld*> all;
        for (const auto& [name, world] : worldRegistry_->worlds()) {
            all.push_back(world);
        }
        return all;
    };
    model.defaultWorldName = [this] { return worldRegistry_->defaultWorld().name(); };
    model.groups = [this]() -> const config::GroupsConfig& { return live_->current()->groups; };
    model.spawnMob = [this](world::LoadedWorld& world, const wire::common::Identifier& type, double x, double y,
                            double z) { return spawning_->spawnMob(world, type, x, y, z, 0.0F, "plugin"); };
    model.makeWorld = [this](const std::string& name, config::WorldDimension dimension, config::WorldType type,
                             std::optional<std::int64_t> seed) {
        return worldRegistry_->createWorld(name, dimension, type, seed);
    };
    model.npcs = &npcs_;
    model.nextEntityId = [this] { return playerRegistry_.allocateEntityId(); };

    const auto sessions = [this] { return playerRegistry_.snapshot(); };
    pluginPlayers_ = std::make_unique<plugins::RegistryPlayers>(playerRegistry_, model);
    pluginWorlds_ = std::make_unique<plugins::LoadedWorlds>(model, sessions);
    pluginPlaceholders_ = std::make_unique<plugins::RegistryPlaceholders>(placeholders_, *pluginPlayers_, sessions);
    pluginStorage_ = std::make_unique<plugins::DatabasePluginStorage>([this] { return playerStore_.get(); });
    pluginMenus_ = std::make_unique<plugins::SessionMenus>(playerRegistry_);
    pluginPackets_ = std::make_unique<plugins::SessionPackets>(playerRegistry_);
    pluginChannels_ = std::make_unique<plugins::SessionChannels>(playerRegistry_);

    plugins::ServerPluginServices::Parts parts;
    parts.players = pluginPlayers_.get();
    parts.worlds = pluginWorlds_.get();
    parts.placeholders = pluginPlaceholders_.get();
    parts.storage = pluginStorage_.get();
    parts.menus = pluginMenus_.get();
    parts.packets = pluginPackets_.get();
    parts.channels = pluginChannels_.get();
    // The owner of a position, which with regions off is the world's one.
    parts.ownerAt = [this](const std::string& name, double x, double z) -> sched::Owner* {
        const std::map<std::string, world::LoadedWorld*> loaded = worldRegistry_->worlds();
        const auto found = loaded.find(name);
        if (found == loaded.end()) {
            return nullptr;
        }
        return &worldRegistry_->scheduler()
                    .regionFor(found->second->name(), static_cast<std::int32_t>(x) >> 4,
                               static_cast<std::int32_t>(z) >> 4)
                    .owner();
    };
    pluginServices_ = std::make_unique<plugins::ServerPluginServices>(
        worldRegistry_->scheduler().global(), *wired_.dispatcher, [this] { return tickLoop_->ticksRun(); },
        [this]() -> const command::CommandSource* { return consoleSource_.get(); }, options_.pluginsRoot,
        std::move(parts));

    // Read once rather than on every use, and not part of what a reload swaps: the limits are baked
    // into a plugin's state when it loads.
    plugins_ = std::make_unique<server::plugins::PluginHost>(
        *pluginServices_, config.pluginLanes,
        lua::LuaLimits{.instructionsPerTurn = config.pluginInstructionsPerTurn,
                       .memoryBytes = static_cast<std::size_t>(config.pluginMemoryBytes)});
    plugins_->setApiInstaller(&server::plugins::pluginApi::install);
    static_cast<void>(wired_.dispatcher->registerCommand(std::make_shared<plugins::PluginsCommand>(
        *plugins_, [this]() -> std::optional<std::string> { return options_.playersDatabasePath; })));
    // A decider answers where the question was asked, so it may only run on the owner plugins live
    // on. Unclaimed counts as ours: before the first tick nothing else is running.
    plugins_->setOnPluginThread([this] {
        return worldRegistry_->scheduler().global().isCurrent() || !worldRegistry_->scheduler().global().isClaimed();
    });
    plugins_->setTimings(
        [this](const std::string& pluginId, std::int64_t nanos) { timings_.of("plugin." + pluginId).record(nanos); });
    plugins_->setAllowed([this](const std::string& id) {
        std::set<server::plugins::PluginPermission> granted;
        const auto found = config_->pluginAllowances.find(id);
        if (found == config_->pluginAllowances.end()) {
            return granted;
        }
        for (const std::string& name : found->second) {
            const std::optional<server::plugins::PluginPermission> permission = server::plugins::permissionOf(name);
            if (permission.has_value()) {
                granted.insert(*permission);
            }
        }
        return granted;
    });

    broadcaster_ = std::make_unique<BlockChangeBroadcaster>(
        playerRegistry_, vanillaData_.lootTables,
        // parity: `spawnGolemInWorld`, which is `EntitySpawnReason.TRIGGERED` and no rotation at all.
        BlockChangeBroadcaster::Hooks{
            .summonGolem =
                [this](world::LoadedWorld& world, std::string_view type, double x, double y, double z) {
                    if (spawning_ == nullptr) {
                        return;
                    }
                    static_cast<void>(spawning_->spawnMob(world, wire::common::Identifier::vanilla(std::string{type}),
                                                          x, y, z, 0.0F, "triggered", {}));
                }},
        BlockChangeBroadcaster::Options{.nextSeed = [] { return scratchSeed(); },
                                        .blockTags = &vanillaData_.blockTags});
    explosions_ = std::make_unique<ExplosionSystem>(
        playerRegistry_, vanillaData_.lootTables,
        ExplosionSystem::Hooks{.struck =
                                   [this](world::LoadedWorld& world,
                                          const std::vector<player::PlayerSession*>& watchers, double x, double y,
                                          double z, float power, std::int32_t sourceEntityId) {
                                       combat_->hurtByExplosion(world, watchers, x, y, z, power, sourceEntityId);
                                   }},
        ExplosionSystem::Options{.nextSeed = [] { return scratchSeed(); },
                                 .nextDouble = [] { return scratchDouble(); }});
    spawning_ = std::make_unique<NaturalSpawningSystem>(
        playerRegistry_, vanillaData_.biomeSpawns, chunkSimulation_, *spawnEquipment_, NaturalSpawningSystem::Hooks{},
        NaturalSpawningSystem::Options{.nextSeed = [] { return scratchSeed(); }});
    combat_ = std::make_unique<CombatSystem>(
        *broadcaster_, *explosions_, CombatSystem::Hooks{},
        CombatSystem::Options{
            .nextInt =
                [](std::int32_t bound) {
                    return bound <= 0
                               ? 0
                               : static_cast<std::int32_t>(scratchRandom()() % static_cast<std::uint64_t>(bound));
                },
            .nextSeed = [] { return scratchSeed(); },
            .blockTags = &vanillaData_.blockTags,
            .dragonFireballLanded =
                [this](world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                       const std::vector<entity::ServerEntity*>& living, const entity::ServerEntity& fireball) {
                    clouds_->fireballLanded(world, watchers, living, fireball);
                },
            .crystalDestroyed =
                [this](world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                       const entity::ServerEntity& crystal, std::optional<std::int32_t> byPlayer) {
                    endFight_->crystalDestroyed(world, watchers, crystal, byPlayer);
                }});
    clouds_ = std::make_unique<AreaEffectCloudSystem>(
        playerRegistry_, *combat_, AreaEffectCloudSystem::Options{.entityTags = &vanillaData_.entityTags});
    endFight_ = std::make_unique<EndFightSystem>(
        playerRegistry_, *broadcaster_, *combat_, *explosions_, *clouds_,
        EndFightSystem::Options{.nextSeed = [] { return scratchSeed(); }, .blockTags = &vanillaData_.blockTags});
    redstone_ = std::make_unique<RedstoneTickSystem>(
        playerRegistry_, vanillaData_.recipes, *broadcaster_, chunkSimulation_,
        // parity: `BaseCommandBlock.performCommand`, which stands the source at the middle of the
        // block with permission two, and only lets it speak while `commandBlockOutput` is on.
        [this](std::int32_t x, std::int32_t y, std::int32_t z, const std::string& line) {
            command::CommandDispatcher* dispatcher = commands();
            if (dispatcher == nullptr) {
                return 0;
            }
            const command::CommandBlockSource source{
                {x + 0.5, y + 0.5, z + 0.5}, "@", true, wire::common::Logger{"CommandBlock"}};
            std::int32_t successes = 0;
            static_cast<void>(dispatcher->execute(source, line.starts_with('/') ? line.substr(1) : line, successes));
            return successes;
        },
        RedstoneTickSystem::Hooks{},
        RedstoneTickSystem::Options{.nextSeed = [] { return scratchSeed(); },
                                    .jukeboxes = &vanillaData_.jukeboxSongs,
                                    .itemTags = &vanillaData_.itemTags,
                                    .blockTags = &vanillaData_.blockTags});
    raids_ = std::make_unique<RaidSystem>(playerRegistry_, *broadcaster_, *spawning_, RaidSystem::Hooks{},
                                          RaidSystem::Options{.nextSeed = [] { return scratchSeed(); }}, logger_);
    statusEffects_ = std::make_unique<StatusEffectSystem>(
        playerRegistry_, [this](world::LoadedWorld& world, player::PlayerSession& player,
                                const world::ActiveEffect& active) { raids_->tickOmens(world, player, active); });
    environment_ = std::make_unique<EnvironmentTickSystem>(
        playerRegistry_, worldsConfig, model.allWorlds, chunkSimulation_, *broadcaster_, *wired_.environment,
        *spawning_, EnvironmentTickSystem::Hooks{},
        EnvironmentTickSystem::Options{.nextSeed = [] { return scratchSeed(); },
                                       .difficulty = [this] { return difficulty_; }},
        logger_);
    mobActions_ =
        std::make_unique<MobActionSystem>(playerRegistry_, *broadcaster_, *explosions_, *spawning_, *combat_, *raids_,
                                          vanillaData_.lootTables, model.allWorlds, MobActionSystem::Hooks{},
                                          MobActionSystem::Options{.nextSeed = [] { return scratchSeed(); },
                                                                   .nextDouble = [] { return scratchDouble(); }});
    mobTicks_ = std::make_unique<MobTickSystem>(*mobActions_);

    // The online count comes from the registry rather than from a counter kept alongside it: a
    // second source of truth for "how many players are connected" is how a server ends up
    // advertising numbers that never come back down.
    ServerListStatus::Hooks listHooks;
    listHooks.playerByAddress = [this](const std::string& address) {
        return playerStore_->players().findNameByAddress(address);
    };
    serverList_ = std::make_unique<ServerListStatus>(
        [this]() -> const config::ServerListConfig& { return live_->current()->serverList; }, placeholders_,
        [this] {
            std::vector<ListedPlayer> listed;
            for (const player::PlayerSession* player : playerRegistry_.snapshot()) {
                listed.push_back(ListedPlayer{.name = player->name(), .uuid = player->uuid().toString()});
            }
            return listed;
        },
        ServerListStatus::Options{.slots = config.maxPlayers,
                                  .versionName = std::string{buildInfo::kMinecraftVersion},
                                  .protocolVersion = buildInfo::kProtocolVersion},
        std::move(listHooks));

    defaultGamemode_ = gamemodeOf(config.gamemode);
    difficulty_ = worldRegistry_->defaultWorld().settings().difficulty;
}

BasaltServer::~BasaltServer() { shutdown(); }

std::map<std::string, world::LoadedWorld*, std::less<>> BasaltServer::namedWorlds() const {
    std::map<std::string, world::LoadedWorld*, std::less<>> named;
    for (const auto& [name, world] : worldRegistry_->worlds()) {
        named.emplace(name, world);
    }
    return named;
}

} // namespace basalt::server::runtime
