#include <basalt/server/plugins/PluginEventTypes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/BasaltServer.hpp>
#include <basalt/server/runtime/BuildInfo.hpp>
#include <basalt/server/runtime/entity/Breeding.hpp>
#include <basalt/server/runtime/entity/CopperGolem.hpp>
#include <basalt/server/runtime/entity/MobRules.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/WardenBrain.hpp>
#include <basalt/server/runtime/placeholder/AnimationPlaceholders.hpp>
#include <basalt/server/runtime/placeholder/PlayerPlaceholders.hpp>
#include <basalt/server/runtime/placeholder/ServerPlaceholders.hpp>
#include <basalt/server/runtime/player/PlayerContainers.hpp>
#include <basalt/server/runtime/player/StatAwards.hpp>
#include <basalt/server/runtime/plugins/PluginsCommand.hpp>
#include <basalt/server/runtime/world/BiomeTagLoader.hpp>
#include <basalt/server/runtime/world/EndPodium.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
#include <basalt/server/runtime/world/RecipeBook.hpp>
#include <basalt/server/runtime/world/SummonAwards.hpp>
#include <basalt/server/runtime/world/TriggerFacts.hpp>
#include <basalt/wire/auth/MojangSessionService.hpp>
#include <basalt/wire/multiversion/java/JavaTranslators.hpp>
#include <basalt/wire/text/TranslationResolver.hpp>

#include <cstdint>
#include <map>
#include <memory>
#include <random>
#include <set>
#include <string>
#include <utility>

namespace basalt::server::runtime {
namespace {

// Beyond the view distance, so a requested chunk is still there when its session gets round to sending it.
constexpr std::int32_t kRetainMarginChunks = 4;

[[nodiscard]] std::mt19937_64& scratchRandom() {
    static thread_local std::mt19937_64 kRandom{std::random_device{}()};
    return kRandom;
}

[[nodiscard]] std::int64_t scratchSeed() { return static_cast<std::int64_t>(scratchRandom()()); }

[[nodiscard]] double scratchDouble() { return std::uniform_real_distribution<double>{0.0, 1.0}(scratchRandom()); }

void golemSpawned(entity::ServerEntity* golem, world::AdvancementTriggers* triggers,
                  const std::vector<player::PlayerSession*>& players, std::string_view worldName) {
    if (golem == nullptr) {
        return;
    }
    if (entity::copperGolems::isCopperGolem(golem->type())) {
        random::RandomSource random = random::RandomSource::legacy(scratchSeed());
        entity::copperGolems::stateOf(*golem, random).fromPattern = true;
    }
    world::summonAwards::award(triggers, players, worldName, *golem, world::summonAwards::kGolemRange);
}

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
      vanillaData_(dataJar(), logger_), chunkSimulation_(playerRegistry_) {
    if (options_.compatibility.nameSubstitutes()) {
        substitutionLabels_ = std::make_unique<SubstituteLabels>();
    }
    // A percentage needs two readings; this is the first, so nobody asking later gets nothing.
    cpuMeter_.prime();

    // Player state sits beside `worlds/`, so a world can be deleted, copied or reset without touching a row.
    std::expected<std::unique_ptr<storage::PlayerStore>, storage::PlayerDatabaseSchemaError> store =
        storage::PlayerStore::open(options_.playersDatabasePath);
    if (!store.has_value()) {
        throw std::runtime_error("players.db: " + store.error().message);
    }
    playerStore_ = std::move(*store);
    playerProgress_ = std::make_unique<world::PlayerProgress>(&playerStore_->progress());

    // The announcement goes to everyone only when the default world's rule allows it; advancements are per player.
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
    triggerHooks.entityTagged = [this](const wire::common::Identifier& type, std::string_view tag) {
        return world::realEntityTypeTags::membersOf(vanillaData_.entityTags,
                                                    wire::common::Identifier::parse(tag).value_or(type))
            .contains(type);
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
    triggerHooks.tags = [this](std::string_view registry, const wire::common::Identifier& id, std::string_view tag) {
        const wire::common::Identifier named = wire::common::Identifier::parse(tag).value_or(id);
        if (registry == "item") {
            return world::realItemTags::membersOf(vanillaData_.itemTags, named.toString()).contains(id);
        }
        if (registry == "entity_type") {
            return world::realEntityTypeTags::membersOf(vanillaData_.entityTags, named).contains(id);
        }
        if (registry == "block") {
            return world::realBlockTags::membersOf(vanillaData_.blockTags, named.toString()).contains(id);
        }
        if (registry == "enchantment") {
            return std::ranges::contains(vanillaData_.lootRegistries.tagMembers(registry, named), id);
        }
        return false;
    };
    triggerHooks.damageTagged = [this](const wire::common::Identifier& type, std::string_view tag) {
        const std::expected<wire::common::Identifier, wire::common::IdentifierError> id =
            wire::common::Identifier::parse(tag);
        return id.has_value() && world::realDamageTypeTags::isIn(vanillaData_.damageTypeTags, *id, type);
    };
    triggerHooks.playerContext = [this](std::string_view uuid) -> std::optional<server::world::loot::LootContext> {
        const std::optional<wire::common::Uuid> id = wire::common::Uuid::parse(uuid);
        player::PlayerSession* player = id.has_value() ? playerRegistry_.get(*id) : nullptr;
        if (player == nullptr || worldRegistry_ == nullptr) {
            return std::nullopt;
        }
        const std::map<std::string, world::LoadedWorld*> worlds = worldRegistry_->worlds();
        const auto found = worlds.find(player->worldName);
        if (found == worlds.end()) {
            return std::nullopt;
        }
        return world::triggerFacts::ofPlayerAt(vanillaData_.lootTables, *found->second, *player, &playerRegistry_);
    };
    addAdvancementHooks(triggerHooks);
    advancementTriggers_ = std::make_unique<world::AdvancementTriggers>(vanillaData_.advancements, *playerProgress_,
                                                                        std::move(triggerHooks));

    const VanillaGameData::Jar jar = dataJar();
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
                               .jukeboxes = &vanillaData_.jukeboxSongs,
                               .lootTables = &vanillaData_.lootTables,
                               .triggers = advancementTriggers_.get()},
        logger_);

    for (const auto& [name, world] : worldRegistry_->worlds()) {
        logger_.info([world] {
            return "Loading world '" + world->name() + "' (seed " + std::to_string(world->metadata().seed) + ").";
        });
        world->warmSpawnArea();
        if (world->declaration().dimension == config::WorldDimension::End && world->settings().openEndExitPortal) {
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

    functionRunner_ = makeFunctionRunner(config.functionPermissionLevel);
    consoleSource_ = std::make_unique<command::ConsoleCommandSource>(
        logger_, options_.lang, [this] { return worldRegistry_->defaultWorld().spawn(); });
    stopwatches_ = std::make_unique<command::commands::Stopwatches>([this] {
        return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - serverClock_)
            .count();
    });
    commandStorage_.load();
    loadStopwatches();

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
    persistenceHooks.saveShared = [this] {
        if (maps_ != nullptr) {
            maps_->save();
        }
        saveSchedule();
        saveScoreboard();
        saveBossBars();
        saveStopwatches();
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

    vanillaData_.lootTables.useScoreboard(&scoreboard_);
    loadScoreboard();
    loadBossBars();

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
    wiring.datapacks = command::commands::DataPackControl{
        .packs = options_.datapacks,
        .reload = [this](const std::vector<std::string>& selected) { return reloadData(selected); }};
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
        golemSpawned(spawning_->spawnMob(world, wire::common::Identifier::vanilla(std::string{type}), x, y, z, 0.0F,
                                         "triggered", {}),
                     advancementTriggers_.get(), playerRegistry_.snapshot(), world.name());
    };
    wiring.killOutright = [this](world::LoadedWorld& world, entity::ServerEntity& entity) {
        return endFight_ != nullptr && endFight_->killOutright(world, entity);
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
    loadFunctions();

    // Plugins run on the global owner: the one owner that exists in both scheduler modes, as a single thread.
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

    // Read once, not swapped by a reload: the limits are baked into a plugin's state when it loads.
    plugins_ = std::make_unique<server::plugins::PluginHost>(
        *pluginServices_, config.pluginLanes,
        wire::lua::LuaLimits{.instructionsPerTurn = config.pluginInstructionsPerTurn,
                             .memoryBytes = static_cast<std::size_t>(config.pluginMemoryBytes)});
    plugins_->setApiInstaller(&server::plugins::pluginApi::install);
    static_cast<void>(wired_.dispatcher->registerCommand(std::make_shared<plugins::PluginsCommand>(
        *plugins_, [this]() -> std::optional<std::string> { return options_.playersDatabasePath; })));
    // A decider may only run on the owner plugins live on; unclaimed counts as ours, before the first tick.
    plugins_->setOnPluginThread([this] {
        return worldRegistry_->scheduler().global().isCurrent() || !worldRegistry_->scheduler().global().isClaimed();
    });
    plugins_->setTimings(
        [this](const std::string& pluginId, std::int64_t nanos) { timings_.of("plugin." + pluginId).record(nanos); });
    plugins_->setAllowed([this](const std::string& id) {
        std::set<wire::plugins::PluginPermission> granted;
        const auto found = config_->pluginAllowances.find(id);
        if (found == config_->pluginAllowances.end()) {
            return granted;
        }
        for (const std::string& name : found->second) {
            const std::optional<wire::plugins::PluginPermission> permission = wire::plugins::permissionOf(name);
            if (permission.has_value()) {
                granted.insert(*permission);
            }
        }
        return granted;
    });

    broadcaster_ = std::make_unique<BlockChangeBroadcaster>(
        playerRegistry_, vanillaData_.lootTables,
        // parity: `spawnGolemInWorld`.
        BlockChangeBroadcaster::Hooks{
            .summonGolem =
                [this](world::LoadedWorld& world, std::string_view type, double x, double y, double z) {
                    if (spawning_ == nullptr) {
                        return;
                    }
                    golemSpawned(spawning_->spawnMob(world, wire::common::Identifier::vanilla(std::string{type}), x, y,
                                                     z, 0.0F, "triggered", {}),
                                 advancementTriggers_.get(), playerRegistry_.snapshot(), world.name());
                }},
        BlockChangeBroadcaster::Options{.nextSeed = [] { return scratchSeed(); },
                                        .blockTags = &vanillaData_.blockTags});
    explosions_ = std::make_unique<ExplosionSystem>(
        playerRegistry_, vanillaData_.lootTables,
        ExplosionSystem::Hooks{
            .struck =
                [this](world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers, double x,
                       double y, double z, float power, std::int32_t sourceEntityId) {
                    combat_->hurtByExplosion(world, watchers, x, y, z, power, sourceEntityId);
                },
            .hiveBlown =
                [this](world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId,
                       const wire::nbt::NbtCompound& hive, std::int32_t sourceEntityId) {
                    if (mobActions_ != nullptr) {
                        mobActions_->hiveBlown(world, {x, y, z}, stateId, hive, sourceEntityId);
                    }
                }},
        ExplosionSystem::Options{.nextSeed = [] { return scratchSeed(); },
                                 .nextDouble = [] { return scratchDouble(); }});
    orbs_ = std::make_unique<ExperienceOrbSystem>(
        playerRegistry_, ExperienceOrbSystem::Options{.nextSeed = [] { return scratchSeed(); }});
    fishing_ = std::make_unique<FishingSystem>(playerRegistry_, *orbs_,
                                               FishingSystem::Options{.nextSeed = [] { return scratchSeed(); }});
    mapStorage_.readAlsoFrom(worldRegistry_->defaultWorld().worldRoot() + "/data/minecraft/maps");
    maps_ = std::make_unique<MapSystem>(
        playerRegistry_, mapStorage_,
        MapSystem::Options{.worldNamed = [this](std::string_view name) -> world::LoadedWorld* {
                               const std::map<std::string, world::LoadedWorld*> worlds = worldRegistry_->worlds();
                               const auto found = worlds.find(std::string{name});
                               return found == worlds.end() ? nullptr : found->second;
                           },
                           .itemTags = &vanillaData_.itemTags,
                           .nextSeed = [] { return scratchSeed(); },
                           .readJar = dataJar().readText});
    vanillaData_.lootTables.useExplorationMaps([this](world::LoadedWorld& world, server::world::loot::ItemStack& stack,
                                                      const server::world::loot::ExplorationMapRequest& request,
                                                      const std::array<double, 3>& origin) {
        static_cast<void>(maps_->explore(world, stack.components, request, origin));
    });
    spawning_ = std::make_unique<NaturalSpawningSystem>(
        playerRegistry_, vanillaData_.biomeSpawns, chunkSimulation_, *spawnEquipment_, NaturalSpawningSystem::Hooks{},
        NaturalSpawningSystem::Options{.nextSeed = [] { return scratchSeed(); }});
    // parity: the biome tags a spawner's rules ask, read once: `BiomeTagLoader` caches as it goes.
    auto spawnerBiomes = std::make_shared<std::map<std::string, std::set<std::string>, std::less<>>>();
    world::BiomeTagLoader biomeTags{jar.readText};
    for (const std::string tag :
         {"minecraft:more_frequent_drowned_spawns", "minecraft:allows_tropical_fish_spawns_at_any_height",
          "minecraft:polar_bears_spawn_on_alternate_blocks", "minecraft:allows_surface_slime_spawns"}) {
        spawnerBiomes->emplace(tag, biomeTags.biomesIn(tag));
    }
    mobSpawners_ = std::make_unique<MobSpawnerSystem>(
        playerRegistry_, *spawning_,
        MobSpawnerSystem::Options{
            .nextSeed = [] { return scratchSeed(); },
            .facts = {.blockTagged =
                          [this](std::string_view tag, const wire::common::Identifier& block) {
                              return world::realBlockTags::contains(vanillaData_.blockTags, tag, block);
                          },
                      .biomeTagged =
                          [spawnerBiomes](std::string_view tag, const wire::common::Identifier& biome) {
                              const auto found = spawnerBiomes->find(tag);
                              return found != spawnerBiomes->end() && found->second.contains(biome.toString());
                          },
                      .playerWithin =
                          [this](const world::LoadedWorld& world, double x, double y, double z, double range) {
                              for (const player::PlayerSession* player : playerRegistry_.snapshot()) {
                                  const double dx = player->x - x;
                                  const double dy = player->y - y;
                                  const double dz = player->z - z;
                                  if (player->worldName == world.name() &&
                                      player->gamemode != wire::protocol::play::Gamemode::Creative &&
                                      player->gamemode != wire::protocol::play::Gamemode::Spectator &&
                                      (range < 0.0 || (dx * dx) + (dy * dy) + (dz * dz) < range * range)) {
                                      return true;
                                  }
                              }
                              return false;
                          }},
            .lootTables = &vanillaData_.lootTables});
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
            .hurtVehicle =
                [this](world::LoadedWorld& world, entity::ServerEntity& vehicle, const VehicleSystem::Hit& hit) {
                    return vehicles_ != nullptr && vehicles_->hurt(world, vehicle, hit);
                },
            .blockTags = &vanillaData_.blockTags,
            .entityTags = &vanillaData_.entityTags,
            .dragonFireballLanded =
                [this](world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                       const std::vector<entity::ServerEntity*>& living, const entity::ServerEntity& fireball) {
                    clouds_->fireballLanded(world, watchers, living, fireball);
                },
            .mobEffect =
                [this](world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                       entity::ServerEntity& mob, const world::ActiveEffect& effect, std::int32_t sourceId) {
                    return mobActions_ != nullptr && mobActions_->addMobEffect(world, watchers, mob, effect, sourceId);
                },
            .witherSkullLanded =
                [this](world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                       entity::ServerEntity& skull, entity::ServerEntity* mob,
                       player::PlayerSession* player) { withers_->skullLanded(world, watchers, skull, mob, player); },
            .awardExperience = [this](world::LoadedWorld& world, double x, double y, double z,
                                      std::int32_t amount) { orbs_->award(world, x, y, z, amount); },
            .targetHit =
                [this](world::LoadedWorld& world, player::PlayerSession& shooter,
                       const entity::ServerEntity& projectile, std::int32_t signal) {
                    player::statAwards::award(shooter, "target_hit");
                    advancementTriggers_->fire(
                        "target_hit", shooter.name(), shooter.uuid().toString(),
                        world::TriggerEvent{
                            .entities = {{"projectile", world::triggerFacts::ofEntity(world, projectile)}},
                            .numbers = {{"signal_strength", signal}}});
                },
            .advancementTriggers = advancementTriggers_.get(),
            .crystalDestroyed =
                [this](world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                       const entity::ServerEntity& crystal, std::optional<std::int32_t> byPlayer) {
                    endFight_->crystalDestroyed(world, watchers, crystal, byPlayer);
                },
            .enchantmentDefinition =
                [this](const wire::common::Identifier& id) { return vanillaData_.enchantments.definitionOf(id); },
            .summonLightning =
                [this](world::LoadedWorld& world, double x, double y, double z, std::optional<std::int32_t> cause) {
                    static_cast<void>(
                        LightningBoltSystem::spawn(world, x, y, z, playerRegistry_.allocateEntityId(), cause));
                },
            // parity: `Entity.spawnAtLocation`.
            .dropItem =
                [this](world::LoadedWorld& world, double x, double y, double z,
                       const server::world::loot::ItemStack& stack) {
                    world.state().spawnItem(world::DroppedItem{.entityId = playerRegistry_.allocateEntityId(),
                                                               .uuid = wire::common::Uuid::random(),
                                                               .item = stack.item,
                                                               .count = stack.count,
                                                               .x = x,
                                                               .y = y,
                                                               .z = z,
                                                               .velocityX = (scratchDouble() * 0.2) - 0.1,
                                                               .velocityY = 0.2,
                                                               .velocityZ = (scratchDouble() * 0.2) - 0.1,
                                                               .pickupDelay = world::kSpawnedPickupDelay,
                                                               .components = stack.components});
                }});
    fireworks_ = std::make_unique<FireworkSystem>(playerRegistry_, *combat_,
                                                  FireworkSystem::Options{.nextSeed = [] { return scratchSeed(); }});
    clouds_ = std::make_unique<AreaEffectCloudSystem>(
        playerRegistry_, *combat_, AreaEffectCloudSystem::Options{.entityTags = &vanillaData_.entityTags});
    endFight_ = std::make_unique<EndFightSystem>(
        playerRegistry_, *broadcaster_, *combat_, *explosions_, *clouds_,
        EndFightSystem::Options{
            .nextSeed = [] { return scratchSeed(); },
            .blockTags = &vanillaData_.blockTags,
            .awardExperience = [this](world::LoadedWorld& world, double x, double y, double z,
                                      std::int32_t amount) { orbs_->award(world, x, y, z, amount); }});
    redstone_ = std::make_unique<RedstoneTickSystem>(
        playerRegistry_, vanillaData_.recipes, *broadcaster_, chunkSimulation_,
        // parity: `BaseCommandBlock.performCommand`.
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
        RedstoneTickSystem::Options{
            .nextSeed = [] { return scratchSeed(); },
            .jukeboxes = &vanillaData_.jukeboxSongs,
            .itemTags = &vanillaData_.itemTags,
            .blockTags = &vanillaData_.blockTags,
            .steppedOn =
                [this](world::LoadedWorld& world, player::PlayerSession& player, std::int32_t x, std::int32_t y,
                       std::int32_t z) {
                    if (shriekers_ != nullptr) {
                        shriekers_->steppedOn(world, player, x, y, z);
                    }
                },
            .foreignTick =
                [this](world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers, std::int32_t x,
                       std::int32_t y, std::int32_t z) {
                    if (shriekers_ != nullptr) {
                        shriekers_->scheduledTick(world, watchers, x, y, z);
                    }
                    if (sculkCatalysts_ != nullptr) {
                        static_cast<void>(sculkCatalysts_->scheduledTick(world, x, y, z));
                    }
                },
            .skullPlaced =
                [this](world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z) {
                    if (withers_ != nullptr) {
                        withers_->placed(world, x, y, z);
                    }
                },
            .launchFirework =
                [this](world::LoadedWorld& world, double x, double y, double z, double dirX, double dirY, double dirZ,
                       const server::world::loot::ItemStack& stack) {
                    // parity: `FireworkRocketItem.asProjectile` takes one of the stack.
                    wire::nbt::NbtCompound item;
                    item.putString("id", stack.item.toString());
                    item.putInt("count", 1);
                    if (stack.components.size() > 0) {
                        item.set("components", wire::nbt::NbtTag::ofCompound(stack.components));
                    }
                    static_cast<void>(
                        fireworks_->shoot(world, std::move(item), x, y, z, dirX, dirY, dirZ, 0.5F, 1.0F, std::nullopt));
                },
            .spawnHatchling =
                [this](world::LoadedWorld& world, const world::scheduledBlocks::Hatchling& hatched) {
                    if (spawning_ == nullptr) {
                        return;
                    }
                    // parity: all three hatch with `EntitySpawnReason.BREEDING`.
                    spawning_->spawnMob(world, hatched.type, hatched.x, hatched.y, hatched.z, hatched.yaw, "breeding",
                                        [&hatched](entity::ServerEntity& born) {
                                            if (hatched.baby) {
                                                born.age = entity::breeding::kBabyAge;
                                            }
                                            born.persistent = hatched.persistent;
                                            born.headYaw = hatched.yaw;
                                        });
                },
            .advancementTriggers = advancementTriggers_.get(),
            .releaseHive =
                [this](world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z,
                       std::int32_t stateId) {
                    if (mobActions_ != nullptr) {
                        mobActions_->emptyHive(world, {x, y, z}, stateId, nullptr,
                                               world::beehives::Release::BeeReleased);
                    }
                },
            .tickBeehive =
                [this](world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z,
                       std::vector<world::BlockChange>& pending) {
                    return mobActions_ != nullptr && mobActions_->tickBeehive(world, x, y, z, pending);
                },
            .tickCreakingHeart =
                [this](world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z,
                       std::vector<world::BlockChange>& pending) {
                    return mobActions_ != nullptr && mobActions_->tickCreakingHeart(world, x, y, z, pending);
                },
            .tickSculkCatalyst =
                [this](world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z) {
                    return sculkCatalysts_ != nullptr && sculkCatalysts_->tick(world, x, y, z);
                },
            .tickConduit =
                [this](world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z,
                       wire::nbt::NbtCompound& data) {
                    return conduits_ != nullptr && conduits_->tick(world, x, y, z, data);
                },
            .triggerGatewayCooldown =
                [this](world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z) {
                    return environment_ == nullptr || environment_->triggerGatewayCooldown(world, x, y, z);
                },
            .shriekerHeard =
                [this](world::LoadedWorld& world, const world::vibrations::ShriekerArrival& arrival) {
                    if (shriekers_ != nullptr) {
                        shriekers_->heard(world, arrival);
                    }
                },
            .brushArmadillo =
                [this](world::LoadedWorld& world, entity::ServerEntity& armadillo) {
                    return mobActions_ != nullptr && mobActions_->brushArmadillo(world, armadillo);
                },
            .mobSpawners = mobSpawners_.get()});
    // parity: `Raid.absorbRaidOmen` and `Raid.tick`'s victory.
    RaidSystem::Hooks raidHooks;
    raidHooks.raidTriggered = [this](player::PlayerSession& player) {
        player::statAwards::award(player, "raid_trigger");
        advancementTriggers_->fire("voluntary_exile", player.name(), player.uuid().toString());
    };
    raidHooks.raidWon = [this](player::PlayerSession& player) {
        player::statAwards::award(player, "raid_win");
        advancementTriggers_->fire("hero_of_the_village", player.name(), player.uuid().toString());
    };
    raids_ = std::make_unique<RaidSystem>(playerRegistry_, *broadcaster_, *spawning_, std::move(raidHooks),
                                          RaidSystem::Options{.nextSeed = [] { return scratchSeed(); }}, logger_);
    statusEffects_ = std::make_unique<StatusEffectSystem>(
        playerRegistry_, [this](world::LoadedWorld& world, player::PlayerSession& player,
                                const world::ActiveEffect& active) { raids_->tickOmens(world, player, active); });
    environment_ = std::make_unique<EnvironmentTickSystem>(
        playerRegistry_, worldsConfig, model.allWorlds, chunkSimulation_, *broadcaster_, *wired_.environment,
        *spawning_, EnvironmentTickSystem::Hooks{},
        EnvironmentTickSystem::Options{.nextSeed = [] { return scratchSeed(); },
                                       .advancementTriggers = advancementTriggers_.get(),
                                       .difficulty = [this] { return difficulty_; }},
        logger_);
    MobActionSystem::Hooks mobHooks;
    withers_ = std::make_unique<WitherSystem>(
        playerRegistry_, *broadcaster_, *explosions_,
        WitherSystem::Options{.nextSeed = [] { return scratchSeed(); },
                              .blockTags = &vanillaData_.blockTags,
                              .advancementTriggers = advancementTriggers_.get(),
                              .mobEffect =
                                  [this](world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                                         entity::ServerEntity& mob, const world::ActiveEffect& effect) {
                                      return mobActions_ != nullptr &&
                                             mobActions_->addMobEffect(world, watchers, mob, effect);
                                  }});
    // parity: `ContainerOpenersCounter` over a chest a copper golem works.
    mobHooks.openersChanged = [this](world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                     std::int32_t openers) {
        const std::optional<std::int32_t> stateId = world.state().blockState(x, y, z);
        const wire::common::Identifier* block =
            stateId.has_value() ? registry::blockStateDefinitions::blockOf(*stateId) : nullptr;
        if (block == nullptr) {
            return;
        }
        player::OpenersContext context;
        context.blocks = breakContext();
        context.nextFloat = [] { return std::uniform_real_distribution<float>{0.0F, 1.0F}(scratchRandom()); };
        context.nextSeed = [] { return static_cast<std::int64_t>(scratchRandom()()); };
        player::announceOpeners(context, world, x, y, z, block->path(), openers);
    };
    mobHooks.teamCarried = [this](const std::string& from, const std::string& to) {
        scoreboard::Team* team = teams_.teamOf(from);
        if (team == nullptr) {
            return;
        }
        static_cast<void>(teams_.join(*team, to));
        static_cast<void>(team->removeMember(from));
        for (player::PlayerSession* player : playerRegistry_.snapshot()) {
            if (player->connection().sendPlayerTeam) {
                player->connection().sendPlayerTeam(team->name, wire::protocol::play::SetPlayerTeamPacket::kMethodJoin,
                                                    std::nullopt, {to});
                player->connection().sendPlayerTeam(team->name, wire::protocol::play::SetPlayerTeamPacket::kMethodLeave,
                                                    std::nullopt, {from});
            }
        }
    };
    mobHooks.curedZombieVillager = [this](const wire::common::Uuid& uuid) {
        player::PlayerSession* session = playerRegistry_.get(uuid);
        if (session != nullptr && advancementTriggers_ != nullptr) {
            // parity: `CuredZombieVillagerTrigger`, the zombie that was and the villager that is.
            advancementTriggers_->fire(
                "cured_zombie_villager", session->name(), uuid.toString(),
                world::TriggerEvent{
                    .entities = {{"zombie", {.type = wire::common::Identifier::vanilla("zombie_villager")}},
                                 {"villager", {.type = wire::common::Identifier::vanilla("villager")}}}});
        }
    };
    mobHooks.witherShoots = [this](world::LoadedWorld& world, entity::ServerEntity& wither) {
        withers_->shootAtTarget(world, wither);
    };
    mobHooks.launchFirework = [this](world::LoadedWorld& world, wire::nbt::NbtCompound item, double x, double y,
                                     double z, std::int32_t ownerId) {
        static_cast<void>(fireworks_->launch(world, std::move(item), x, y, z, ownerId));
    };
    mobHooks.spawnOrb = [this](world::LoadedWorld& world, double x, double y, double z, std::int32_t value) {
        orbs_->spawn(world, x, y, z, value);
    };
    mobHooks.bredAnimals = [this](std::string_view cause, const wire::common::Identifier& parent,
                                  const wire::common::Identifier& partner, const wire::common::Identifier& child) {
        const std::optional<wire::common::Uuid> uuid = wire::common::Uuid::parse(cause);
        player::PlayerSession* session = uuid.has_value() ? playerRegistry_.get(*uuid) : nullptr;
        if (session == nullptr) {
            return;
        }
        session->stats.increment(storage::stats::animalsBred());
        if (advancementTriggers_ != nullptr) {
            // parity: `BredAnimalsTrigger`, which tries the parents both ways round.
            const server::world::loot::EntityFacts newborn{.type = child, .baby = true};
            for (const auto& [one, other] : {std::pair{parent, partner}, std::pair{partner, parent}}) {
                advancementTriggers_->fire(
                    "bred_animals", session->name(), std::string{cause},
                    world::TriggerEvent{
                        .entities = {{"child", newborn}, {"parent", {.type = one}}, {"partner", {.type = other}}}});
            }
        }
    };
    // parity: `LevelResource.GENERATED_DIR`.
    vanillaData_.structureTemplates.setGeneratedRoot(std::filesystem::path{options_.worldsRoot}.parent_path() /
                                                     "generated");
    structureBlocks_ = std::make_unique<StructureBlockSystem>(vanillaData_.structureTemplates, playerRegistry_);
    mobActions_ =
        std::make_unique<MobActionSystem>(playerRegistry_, *broadcaster_, *explosions_, *spawning_, *combat_, *raids_,
                                          vanillaData_.lootTables, model.allWorlds, std::move(mobHooks),
                                          MobActionSystem::Options{.nextSeed = [] { return scratchSeed(); },
                                                                   .entityTags = &vanillaData_.entityTags,
                                                                   .teams = &teams_,
                                                                   .nextDouble = [] { return scratchDouble(); },
                                                                   .recipes = &vanillaData_.recipes,
                                                                   .damageTypeTags = &vanillaData_.damageTypeTags,
                                                                   .blockTags = &vanillaData_.blockTags,
                                                                   .itemTags = &vanillaData_.itemTags,
                                                                   .villagerTrades = &vanillaData_.villagerTrades,
                                                                   .advancementTriggers = advancementTriggers_.get()});
    shriekers_ = std::make_unique<ShriekerSystem>(playerRegistry_, *broadcaster_, *spawning_, *mobActions_,
                                                  ShriekerSystem::Options{.nextSeed = [] { return scratchSeed(); }});
    sculkCatalysts_ = std::make_unique<SculkCatalystSystem>(
        playerRegistry_, *broadcaster_,
        SculkCatalystSystem::Options{.nextSeed = [] { return scratchSeed(); }, .blockTags = &vanillaData_.blockTags});
    conduits_ = std::make_unique<ConduitSystem>(
        playerRegistry_, *broadcaster_,
        ConduitSystem::Options{.nextSeed = [] { return scratchSeed(); }, .blockTags = &vanillaData_.blockTags});
    lightning_ = std::make_unique<LightningBoltSystem>(*broadcaster_, *mobActions_, advancementTriggers_.get());
    pushes_ = std::make_unique<EntityPushSystem>(
        playerRegistry_, *combat_,
        EntityPushSystem::Hooks{
            // parity: `Warden.doPush`: once a second, anger at what it bumped into and a look there.
            .wardenTouched =
                [this](world::LoadedWorld& world, entity::ServerEntity& warden, const EntityPushSystem::Body& touched) {
                    static_cast<void>(world);
                    if (warden.noAi || warden.touchCooldown > 0) {
                        return;
                    }
                    warden.touchCooldown = 20;
                    const double x = touched.mob != nullptr ? touched.mob->x : touched.player->x;
                    const double y = touched.mob != nullptr ? touched.mob->y : touched.player->y;
                    const double z = touched.mob != nullptr ? touched.mob->z : touched.player->z;
                    const bool targetable =
                        touched.player != nullptr
                            ? !touched.player->dead &&
                                  (touched.player->gamemode == wire::protocol::play::Gamemode::Survival ||
                                   touched.player->gamemode == wire::protocol::play::Gamemode::Adventure)
                            : registry::entityAttributes::isLiving(touched.mob->type()) && !touched.mob->dying &&
                                  !entity::wardens::isWarden(touched.mob->type()) &&
                                  touched.mob->type().path() != "armor_stand";
                    if (targetable) {
                        mobActions_->wardenAngered(
                            warden, touched.mob != nullptr ? touched.mob->entityId() : touched.player->entityId(),
                            entity::wardens::kHeard, true);
                    }
                    entity::wardenBrain::setDisturbance(warden, {static_cast<std::int32_t>(std::floor(x)),
                                                                 static_cast<std::int32_t>(std::floor(y)),
                                                                 static_cast<std::int32_t>(std::floor(z))});
                },
            .golemTouched = [](entity::ServerEntity& golem,
                               entity::ServerEntity& monster) { golem.targetId = monster.entityId(); }},
        EntityPushSystem::Options{.nextSeed = [] { return scratchSeed(); },
                                  .blockTags = &vanillaData_.blockTags,
                                  .teams = &teams_,
                                  .minecartImprovements = [this] { return minecartImprovements(); }});
    vehicles_ = std::make_unique<VehicleSystem>(
        playerRegistry_, *broadcaster_, *explosions_, *pushes_, *mobActions_,
        VehicleSystem::Options{
            .nextSeed = [] { return scratchSeed(); },
            .minecartImprovements = [this] { return minecartImprovements(); },
            .itemTags = &vanillaData_.itemTags,
            .blockTags = &vanillaData_.blockTags,
            .damageTypeTags = &vanillaData_.damageTypeTags,
            .entityTags = &vanillaData_.entityTags,
            // parity: `MinecartCommandBlock.MinecartCommandBase.createCommandSourceStack`.
            .runCommand =
                [this](world::LoadedWorld& world, entity::ServerEntity& cart, const std::string& line) {
                    static_cast<void>(world);
                    command::CommandDispatcher* dispatcher = commands();
                    if (dispatcher == nullptr) {
                        return 0;
                    }
                    const command::CommandBlockSource source{
                        {cart.x, cart.y, cart.z}, "@", true, wire::common::Logger{"CommandBlock"}};
                    std::int32_t successes = 0;
                    static_cast<void>(
                        dispatcher->execute(source, line.starts_with('/') ? line.substr(1) : line, successes));
                    return successes;
                },
            // parity: `MinecartSpawner.tick`, the same spawner run at the cart's square.
            .tickSpawner =
                [this](world::LoadedWorld& world, wire::nbt::NbtCompound& data, const std::array<std::int32_t, 3>& pos,
                       const std::function<void(std::int32_t eventId)>& broadcastEvent) {
                    return mobSpawners_ != nullptr && mobSpawners_->tick(world, data, pos, broadcastEvent);
                }});
    mobTicks_ = std::make_unique<MobTickSystem>(*mobActions_, *pushes_);

    // The online count comes from the registry, not a second counter that could advertise numbers that never come down.
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
