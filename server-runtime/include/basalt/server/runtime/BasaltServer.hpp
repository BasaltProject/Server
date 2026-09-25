#pragma once

#include <basalt/server/plugins/PluginApi.hpp>
#include <basalt/server/plugins/PluginHost.hpp>
#include <basalt/server/runtime/AreaEffectCloudSystem.hpp>
#include <basalt/server/runtime/BlockChangeBroadcaster.hpp>
#include <basalt/server/runtime/ChunkSimulationView.hpp>
#include <basalt/server/runtime/ClientSession.hpp>
#include <basalt/server/runtime/CombatSystem.hpp>
#include <basalt/server/runtime/CommandWiring.hpp>
#include <basalt/server/runtime/EndFightSystem.hpp>
#include <basalt/server/runtime/EnvironmentTickSystem.hpp>
#include <basalt/server/runtime/ExperienceOrbSystem.hpp>
#include <basalt/server/runtime/ExplosionSystem.hpp>
#include <basalt/server/runtime/FireworkSystem.hpp>
#include <basalt/server/runtime/FishingSystem.hpp>
#include <basalt/server/runtime/LightningBoltSystem.hpp>
#include <basalt/server/runtime/MapSystem.hpp>
#include <basalt/server/runtime/MobActionSystem.hpp>
#include <basalt/server/runtime/MobTickSystem.hpp>
#include <basalt/server/runtime/NaturalSpawningSystem.hpp>
#include <basalt/server/runtime/PersistenceCoordinator.hpp>
#include <basalt/server/runtime/RaidSystem.hpp>
#include <basalt/server/runtime/RedstoneTickSystem.hpp>
#include <basalt/server/runtime/ServerListStatus.hpp>
#include <basalt/server/runtime/ServerMetrics.hpp>
#include <basalt/server/runtime/ShriekerSystem.hpp>
#include <basalt/server/runtime/StatusEffectSystem.hpp>
#include <basalt/server/runtime/SubstituteLabels.hpp>
#include <basalt/server/runtime/TickLoop.hpp>
#include <basalt/server/runtime/VanillaGameData.hpp>
#include <basalt/server/runtime/WitherSystem.hpp>
#include <basalt/server/runtime/WorldRegistry.hpp>
#include <basalt/server/runtime/bedrock/BedrockListener.hpp>
#include <basalt/server/runtime/bedrock/BedrockRecipes.hpp>
#include <basalt/server/runtime/bedrock/BedrockWindows.hpp>
#include <basalt/server/runtime/command/commands/SaveCommands.hpp>
#include <basalt/server/runtime/config/CompatibilityConfig.hpp>
#include <basalt/server/runtime/config/LiveConfigs.hpp>
#include <basalt/server/runtime/function/FunctionLibrary.hpp>
#include <basalt/server/runtime/player/PlayerBlockBreak.hpp>
#include <basalt/server/runtime/player/PlayerDamage.hpp>
#include <basalt/server/runtime/plugins/ServerObjectModel.hpp>
#include <basalt/server/runtime/plugins/ServerPluginServices.hpp>
#include <basalt/server/runtime/scoreboard/Scoreboard.hpp>
#include <basalt/server/runtime/scoreboard/Teams.hpp>
#include <basalt/server/runtime/timings/TimingsReport.hpp>
#include <basalt/server/runtime/world/AdvancementTriggers.hpp>
#include <basalt/server/runtime/world/PlayerProgress.hpp>
#include <basalt/server/runtime/world/maps/MapStorage.hpp>
#include <basalt/wire/auth/SessionKeyPair.hpp>
#include <basalt/wire/auth/SignatureValidator.hpp>
#include <basalt/wire/io/TcpSocket.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::runtime {

/// A thread-per-connection server covering handshake, status, login, configuration and play.
///
/// The generator, the router and the surface environment stay optional so a protocol test needs no
/// `server.jar`.
class BasaltServer {
public:
    /// parity: the jar decorates the spawn 3x3 first and everything after it in plain raster,
    /// measured on seed 11. Preparing more than that square puts chunks in front of the ones that
    /// asked for them, and their neighbours then read a world the jar had not built yet.
    static constexpr std::int32_t kSpawnRadiusChunks = 1;

    struct Options {
        /// The operator's own jar. Null in a test that never breaks a block in survival.
        assets::ServerJarAssets* serverJar = nullptr;
        world::VanillaWorldGeneratorFactory* generators = nullptr;
        /// Online mode's own key pair and session service, or null for offline mode.
        const wire::auth::SessionKeyPair* keyPair = nullptr;
        wire::auth::SessionService* sessionService = nullptr;
        /// Mojang's public keys, which a chat session is checked against. Online mode only.
        const wire::auth::ServicesKeySet* servicesKeys = nullptr;
        /// Which client versions may in. Read once: it decides what a connection is before there
        /// is a player, so it is not one of the files a reload swaps.
        config::CompatibilityConfig compatibility{config::BackwardsSelection{},
                                                  wire::multiversion::UnknownProtocolPolicy::Refuse,
                                                  wire::multiversion::UnknownProtocolPolicy::Refuse};
        /// Where `plugins/` is: the working directory on a real server, under `build/` in a test.
        std::string pluginsRoot = ".";
        std::string playersDatabasePath = "players.db";
        std::string worldsRoot = "worlds";
        wire::text::LangTable lang;
    };

    BasaltServer(const config::BasaltConfig& config, const config::WorldsConfig& worldsConfig,
                 config::LiveConfigs& live, Options options);

    BasaltServer(const BasaltServer&) = delete;
    BasaltServer& operator=(const BasaltServer&) = delete;
    BasaltServer(BasaltServer&&) = delete;
    BasaltServer& operator=(BasaltServer&&) = delete;
    ~BasaltServer();

    /// Whether this instance stopped in order to be replaced by a fresh one in the same process.
    [[nodiscard]] bool shouldRestart() const noexcept { return restarting_.load(); }

    [[nodiscard]] server::plugins::PluginHost& plugins() noexcept { return *plugins_; }

    [[nodiscard]] command::CommandDispatcher* commands() const noexcept { return wired_.dispatcher.get(); }

    /// Binds the port, brings the plugins up, starts the tick loop and accepts until it is stopped.
    void start(std::chrono::steady_clock::time_point startedAt = std::chrono::steady_clock::now());

    /// Disconnects everyone, stops the tick loop, drains the queues, then saves once: saving while
    /// still live may never finish, since connected players keep generating more to save.
    void shutdown();

    /// Stops this instance so a fresh one can take its place **in the same process**.
    ///
    /// Nothing is re-executed and no new process is spawned: everything this instance owns is wound
    /// down exactly as a stop winds it down, and the caller then builds another one.
    void requestRestart();

private:
    /// Everything the world itself owes per tick, plus the termination check.
    /// `worldRuns` is false on a turn `/tick freeze` is holding. Then only the queued tasks run, so
    /// the console keeps working, and nothing that belongs to the world's clock moves.
    void tickWorlds(bool worldRuns);

    /// Opens the Bedrock door, if the operator left it open. Never fatal: a taken port costs that
    /// door and nothing else.
    void startBedrockListener();

    /// Ticks the entities that are not players, items or falling blocks.
    void tickEntities(world::LoadedWorld& world, const sched::Region& region);

    /// parity: `LivingEntity.die` is not a sweep: it runs where the damage happened, so the loot and
    /// the `ENTITY_DIE` a sculk sensor hears come out on that very tick. Here the kill leaves a flag,
    /// so this is run wherever a kill can have been made: after the queued commands, which is where
    /// vanilla's command phase is and the only thing that runs while `/tick freeze` holds, and again
    /// after the entities have moved, which is where a mob killed by another one dies.
    void tickDeaths(world::LoadedWorld& world, const sched::Region& region);

    /// The objectives the server keeps itself: deaths, kills, health, food, air, armour and
    /// experience. parity: `ServerPlayer.doTick`, `die` and `awardKillScore`.
    void tickScoreCriteria();

    /// Applies `update` to every objective of `criterion` for `holder`, telling whoever shows it.
    void updateCriterion(std::string_view criterion, std::string_view holder,
                         const std::function<std::int32_t(std::int32_t)>& update);

    /// parity: the `DamageSource` of the blow that killed `dead`, as `killing_blow` reads it.
    [[nodiscard]] world::TriggerDamage killingBlowOf(const world::LoadedWorld& world, const entity::ServerEntity& dead);

    /// parity: `ServerPlayer.awardKillScore`.
    void awardKillScore(player::PlayerSession& killer, const std::string& victim, bool victimIsPlayer);

    /// A mob a player killed, waiting for the scoring pass: killer uuid and the mob's holder name.
    struct PendingKill {
        wire::common::Uuid killer;
        std::string victim;
        wire::common::Identifier victimType;
        /// What `player_killed_entity` reads: the victim and the blow that killed it.
        server::world::loot::EntityFacts victimFacts;
        world::TriggerDamage killingBlow;
    };

    /// parity: `TntBlock.prime` makes the entity inside the neighbour update that lit the block, so
    /// the fuse and its `PRIME_FUSE` land on that turn. Queued because the block half can run on a
    /// client's thread; drained here, before the world's own turn.
    void tickPrimedTnt(world::LoadedWorld& world);

    /// The bridge between a signal and an orderly stop: the handler only sets a flag, and this is
    /// what notices, once per tick.
    void shutdownIfSignalled();

    /// Watches for a plugin that went into the server and did not come back.
    void startPluginWatchdog();

    /// Names the plugins that left data behind, once, at startup. Nothing is deleted.
    void reportOrphanedPluginData();

    /// Keeps the caller alive while the shutdown finishes on another thread.
    void awaitShutdown();

    /// One session, on its own thread, with everything this server hands a connection.
    void serveConnection(wire::io::TcpConnection connection);

    /// Everything a connection shows a plugin, built once per connection. Unset hooks mean nobody
    /// objects, which is what a server with the plugins switched off hands every session.
    [[nodiscard]] ClientSession::PluginHooks pluginHooks();

    /// The loaded worlds in the shape commands look them up in.
    [[nodiscard]] std::map<std::string, world::LoadedWorld*, std::less<>> namedWorlds() const;

    const config::BasaltConfig* config_;
    const config::WorldsConfig* worldsConfig_;
    config::LiveConfigs* live_;
    Options options_;
    wire::common::Logger logger_{"Server"};

    player::PlayerRegistry playerRegistry_;
    /// Built once from the configuration and what this build can translate.
    wire::multiversion::ProtocolGate protocolGate_;
    /// Null when the operator turned the naming off, which the rewriter reads as "leave it".
    std::unique_ptr<SubstituteLabels> substitutionLabels_;
    /// Player-shaped entities a plugin made, drawn by every connection that may see them.
    plugins::NpcRegistry npcs_;
    ServerMetrics metrics_;
    CpuMeter cpuMeter_;
    /// What hurting or healing a player needs, whichever door they came in by.
    [[nodiscard]] player::DamageContext damageContext();

    /// What breaking a block needs, whichever door the breaker came in by.
    [[nodiscard]] player::BreakContext breakContext();

    /// The recipe book as the other door reads it, built the first time it is asked for: the list
    /// is the same for everyone and costs a walk of every recipe to build.
    [[nodiscard]] const bedrock::BedrockRecipeList& bedrockRecipeBook();

    /// What the recipe with that number makes, which is the only thing a craft asks for.
    [[nodiscard]] std::optional<storage::StoredItemStack> bedrockRecipeResult(std::int32_t netId);

    /// Tells a world that one more, or one fewer, is looking into the block that window belongs to.
    void openersChanged(world::LoadedWorld& world, const bedrock::OpenWindow& window, bool opened);

    /// Rolls what the open enchanting table offers this player and leaves it in their window, for
    /// the door whose screen reads the rows out of a packet rather than out of the window.
    void rollBedrockEnchantment(player::PlayerSession& session, world::LoadedWorld& world,
                                bedrock::BedrockWindows& windows);

    VanillaGameData vanillaData_;
    ChunkSimulationView chunkSimulation_;
    /// What a mob spawns holding, out of the operator's own jar.
    std::unique_ptr<world::SpawnEquipmentFinalizer> spawnEquipment_;
    wire::common::Timings timings_;
    std::chrono::steady_clock::time_point serverClock_ = std::chrono::steady_clock::now();

    std::unique_ptr<storage::PlayerStore> playerStore_;
    std::unique_ptr<world::PlayerProgress> playerProgress_;
    std::unique_ptr<world::AdvancementTriggers> advancementTriggers_;
    std::unique_ptr<WorldRegistry> worldRegistry_;
    std::unique_ptr<TickLoop> tickLoop_;

    wire::common::placeholder::PlaceholderRegistry placeholders_;
    scoreboard::Scoreboard scoreboard_;
    scoreboard::Teams teams_;
    std::mutex pendingKillsLock_;
    std::vector<PendingKill> pendingKills_;
    std::unique_ptr<timings::TimingsReport> timingsReport_;
    function::FunctionLibrary functions_;
    std::unique_ptr<command::commands::FunctionRunner> functionRunner_;
    command::commands::FunctionSchedule functionSchedule_;
    std::unique_ptr<command::ConsoleCommandSource> consoleSource_;
    std::unique_ptr<command::commands::Stopwatches> stopwatches_;
    command::CommandStorage commandStorage_;
    command::commands::BossBarRegistry bossBars_;
    std::unique_ptr<PersistenceCoordinator> persistence_;
    std::unique_ptr<command::commands::SaveControl> saveControl_;

    /// What `/setidletimeout` last set. Recorded and reported; nothing disconnects on it yet.
    std::int32_t idleTimeoutMinutes_ = 0;
    /// `server.toml` decides what the whitelist starts as, so a restart follows the file again.
    bool whitelistEnforced_ = false;

    WiredCommands wired_;

    std::unique_ptr<plugins::RegistryPlayers> pluginPlayers_;
    std::unique_ptr<plugins::LoadedWorlds> pluginWorlds_;
    std::unique_ptr<plugins::RegistryPlaceholders> pluginPlaceholders_;
    std::unique_ptr<plugins::DatabasePluginStorage> pluginStorage_;
    std::unique_ptr<plugins::SessionMenus> pluginMenus_;
    std::unique_ptr<plugins::SessionPackets> pluginPackets_;
    std::unique_ptr<plugins::SessionChannels> pluginChannels_;
    std::unique_ptr<plugins::ServerPluginServices> pluginServices_;
    std::unique_ptr<server::plugins::PluginHost> plugins_;

    std::unique_ptr<BlockChangeBroadcaster> broadcaster_;
    std::unique_ptr<ExplosionSystem> explosions_;
    /// Built the first time a Bedrock client asks for it, and the same for every one after.
    std::optional<bedrock::BedrockRecipeList> bedrockRecipes_;
    std::unique_ptr<NaturalSpawningSystem> spawning_;
    std::unique_ptr<ExperienceOrbSystem> orbs_;
    std::unique_ptr<FishingSystem> fishing_;
    world::maps::MapStorage mapStorage_;
    std::unique_ptr<MapSystem> maps_;
    std::unique_ptr<WitherSystem> withers_;
    std::unique_ptr<LightningBoltSystem> lightning_;
    std::unique_ptr<ShriekerSystem> shriekers_;
    std::unique_ptr<CombatSystem> combat_;
    std::unique_ptr<FireworkSystem> fireworks_;
    std::unique_ptr<AreaEffectCloudSystem> clouds_;
    std::unique_ptr<EndFightSystem> endFight_;
    std::unique_ptr<RedstoneTickSystem> redstone_;
    std::unique_ptr<RaidSystem> raids_;
    std::unique_ptr<StatusEffectSystem> statusEffects_;
    std::unique_ptr<EnvironmentTickSystem> environment_;
    std::unique_ptr<MobActionSystem> mobActions_;
    std::unique_ptr<MobTickSystem> mobTicks_;
    std::unique_ptr<ServerListStatus> serverList_;

    wire::protocol::play::Gamemode defaultGamemode_ = wire::protocol::play::Gamemode::Survival;
    std::string difficulty_ = "easy";

    std::optional<wire::io::TcpListener> listener_;
    /// The Bedrock door, when the operator left it open. Its own socket and its own thread.
    std::unique_ptr<bedrock::BedrockListener> bedrockListener_;
    std::atomic<bool> accepting_{true};
    /// Guards against running the shutdown twice: a stop command and a signal can arrive together.
    std::atomic<bool> shuttingDown_{false};
    /// Set once the shutdown has finished its last write, so the caller knows it is safe to proceed.
    std::atomic<bool> shutdownComplete_{false};
    std::atomic<bool> restarting_{false};
    std::vector<std::jthread> connections_;
    std::jthread watchdog_;
};

} // namespace basalt::server::runtime
