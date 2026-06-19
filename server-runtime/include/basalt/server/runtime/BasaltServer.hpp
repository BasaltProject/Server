#pragma once

#include <basalt/server/plugins/PluginApi.hpp>
#include <basalt/server/plugins/PluginHost.hpp>
#include <basalt/server/runtime/AreaEffectCloudSystem.hpp>
#include <basalt/server/runtime/BlockChangeBroadcaster.hpp>
#include <basalt/server/runtime/ChunkSimulationView.hpp>
#include <basalt/server/runtime/ClientSession.hpp>
#include <basalt/server/runtime/CombatSystem.hpp>
#include <basalt/server/runtime/CommandWiring.hpp>
#include <basalt/server/runtime/ConduitSystem.hpp>
#include <basalt/server/runtime/EndFightSystem.hpp>
#include <basalt/server/runtime/EntityPushSystem.hpp>
#include <basalt/server/runtime/EnvironmentTickSystem.hpp>
#include <basalt/server/runtime/ExperienceOrbSystem.hpp>
#include <basalt/server/runtime/ExplosionSystem.hpp>
#include <basalt/server/runtime/FireworkSystem.hpp>
#include <basalt/server/runtime/FishingSystem.hpp>
#include <basalt/server/runtime/LightningBoltSystem.hpp>
#include <basalt/server/runtime/MapSystem.hpp>
#include <basalt/server/runtime/MobActionSystem.hpp>
#include <basalt/server/runtime/MobSpawnerSystem.hpp>
#include <basalt/server/runtime/MobTickSystem.hpp>
#include <basalt/server/runtime/NaturalSpawningSystem.hpp>
#include <basalt/server/runtime/PersistenceCoordinator.hpp>
#include <basalt/server/runtime/RaidSystem.hpp>
#include <basalt/server/runtime/RedstoneTickSystem.hpp>
#include <basalt/server/runtime/SculkCatalystSystem.hpp>
#include <basalt/server/runtime/ServerListStatus.hpp>
#include <basalt/server/runtime/ServerMetrics.hpp>
#include <basalt/server/runtime/ShriekerSystem.hpp>
#include <basalt/server/runtime/StatusEffectSystem.hpp>
#include <basalt/server/runtime/StructureBlockSystem.hpp>
#include <basalt/server/runtime/SubstituteLabels.hpp>
#include <basalt/server/runtime/TickLoop.hpp>
#include <basalt/server/runtime/VanillaGameData.hpp>
#include <basalt/server/runtime/VehicleSystem.hpp>
#include <basalt/server/runtime/WitherSystem.hpp>
#include <basalt/server/runtime/WorldRegistry.hpp>
#include <basalt/server/runtime/bedrock/BedrockListener.hpp>
#include <basalt/server/runtime/bedrock/BedrockRecipes.hpp>
#include <basalt/server/runtime/bedrock/BedrockWindows.hpp>
#include <basalt/server/runtime/bukkit/JavaPluginScan.hpp>
#include <basalt/server/runtime/command/commands/SaveCommands.hpp>
#include <basalt/server/runtime/config/CompatibilityConfig.hpp>
#include <basalt/server/runtime/config/LiveConfigs.hpp>
#include <basalt/server/runtime/datapack/DataPacks.hpp>
#include <basalt/server/runtime/function/FunctionLibrary.hpp>
#include <basalt/server/runtime/player/PlayerBlockBreak.hpp>
#include <basalt/server/runtime/player/PlayerDamage.hpp>
#include <basalt/server/runtime/player/PlayerVersusPlayer.hpp>
#include <basalt/server/runtime/plugins/EventSink.hpp>
#include <basalt/server/runtime/plugins/ServerInventories.hpp>
#include <basalt/server/runtime/plugins/ServerMiscServices.hpp>
#include <basalt/server/runtime/plugins/ServerObjectModel.hpp>
#include <basalt/server/runtime/plugins/ServerPluginServices.hpp>
#include <basalt/server/runtime/scoreboard/Scoreboard.hpp>
#include <basalt/server/runtime/scoreboard/Teams.hpp>
#include <basalt/server/runtime/timings/TimingsReport.hpp>
#include <basalt/server/runtime/world/AdvancementTriggers.hpp>
#include <basalt/server/runtime/world/PlayerAdvancements.hpp>
#include <basalt/server/runtime/world/PlayerProgress.hpp>
#include <basalt/server/runtime/world/maps/MapStorage.hpp>
#include <basalt/wire/auth/SessionKeyPair.hpp>
#include <basalt/wire/auth/SignatureValidator.hpp>
#include <basalt/wire/io/TcpSocket.hpp>
#include <basalt/wire/protocol/configuration/ConfigurationPackets.hpp>

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
class BlockTests;

class BasaltServer {
public:
    static constexpr std::int32_t kSpawnRadiusChunks = 1;

    struct Options {
        /// Null in a test that never breaks a block in survival.
        assets::ServerJarAssets* serverJar = nullptr;
        datapack::DataPacks* datapacks = nullptr;
        world::VanillaWorldGeneratorFactory* generators = nullptr;
        const wire::auth::SessionKeyPair* keyPair = nullptr;
        wire::auth::SessionService* sessionService = nullptr;
        const wire::auth::ServicesKeySet* servicesKeys = nullptr;
        config::CompatibilityConfig compatibility{config::BackwardsSelection{},
                                                  wire::multiversion::UnknownProtocolPolicy::Refuse,
                                                  wire::multiversion::UnknownProtocolPolicy::Refuse};
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

    [[nodiscard]] bool shouldRestart() const noexcept { return restarting_.load(); }

    [[nodiscard]] server::plugins::PluginHost& plugins() noexcept { return *plugins_; }

    [[nodiscard]] command::CommandDispatcher* commands() const noexcept { return wired_.dispatcher.get(); }

    void start(std::chrono::steady_clock::time_point startedAt = std::chrono::steady_clock::now());

    void shutdown();

    void requestRestart();

private:
    void tickWorlds(bool worldRuns);

    void startBedrockListener();

    void startBukkit();
    void enableBukkit();

    void tickEntities(world::LoadedWorld& world, const sched::Region& region);

    /// parity: the sculk catalyst's `CatalystListener` on a death.
    bool drinkBySculk(world::LoadedWorld& world, const entity::ServerEntity& dead);
    /// parity: `WorldDataConfiguration.enabledFeatures`.
    [[nodiscard]] std::vector<std::string> enabledFeatures() const;
    /// parity: `AbstractMinecart.useExperimentalMovement`.
    [[nodiscard]] bool minecartImprovements() const;
    /// parity: `LivingEntity.die`.
    void tickDeaths(world::LoadedWorld& world, const sched::Region& region);

    /// parity: `ServerPlayer.doTick`, `die` and `awardKillScore`.
    void tickScoreCriteria();

    void updateCriterion(std::string_view criterion, std::string_view holder,
                         const std::function<std::int32_t(std::int32_t)>& update);

    [[nodiscard]] std::vector<player::PlayerSession*> watchersIn(const world::LoadedWorld& world);

    void tickBoat(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                  const std::vector<entity::ServerEntity*>& living, entity::ServerEntity& boat);

    /// parity: the `DamageSource` of the blow that killed `dead`, as `killing_blow` reads it.
    [[nodiscard]] world::TriggerDamage killingBlowOf(const world::LoadedWorld& world, const entity::ServerEntity& dead);

    /// parity: `ServerPlayer.awardKillScore`.
    void awardKillScore(player::PlayerSession& killer, const std::string& victim, bool victimIsPlayer);

    struct PendingKill {
        wire::common::Uuid killer;
        std::string victim;
        wire::common::Identifier victimType;
        server::world::loot::EntityFacts victimFacts;
        world::TriggerDamage killingBlow;
    };

    /// parity: `TntBlock.prime`.
    void tickPrimedTnt(world::LoadedWorld& world);

    void shutdownIfSignalled();

    /// Connects what the server tells plugins about a world to that world; a world made later needs it too.
    void wireWorld(const std::string& name, world::LoadedWorld& world);

    void startPluginWatchdog();

    void reportOrphanedPluginData();

    void awaitShutdown();

    void serveConnection(wire::io::TcpConnection connection);

    [[nodiscard]] ClientSession::PluginHooks pluginHooks();

    [[nodiscard]] std::map<std::string, world::LoadedWorld*, std::less<>> namedWorlds() const;

    const config::BasaltConfig* config_;
    const config::WorldsConfig* worldsConfig_;
    config::LiveConfigs* live_;
    Options options_;
    wire::common::Logger logger_{"Server"};

    player::PlayerRegistry playerRegistry_;
    wire::multiversion::ProtocolGate protocolGate_;
    std::unique_ptr<SubstituteLabels> substitutionLabels_;
    plugins::NpcRegistry npcs_;
    ServerMetrics metrics_;
    CpuMeter cpuMeter_;
    [[nodiscard]] player::DamageContext damageContext();

    [[nodiscard]] player::BreakContext breakContext();

    [[nodiscard]] player::pvp::Context meleeContext(const player::BreakContext& blocks);

    /// parity: `Player.attack` on a `ServerPlayer`.
    void bedrockAttackPlayer(player::PlayerSession& session, player::PlayerSession& victim, world::LoadedWorld& world,
                             bool falling);

    [[nodiscard]] const bedrock::BedrockRecipeList& bedrockRecipeBook();

    [[nodiscard]] std::optional<storage::StoredItemStack> bedrockRecipeResult(std::int32_t netId);

    void openersChanged(world::LoadedWorld& world, const bedrock::OpenWindow& window, bool opened);

    void rollBedrockEnchantment(player::PlayerSession& session, world::LoadedWorld& world,
                                bedrock::BedrockWindows& windows);

    [[nodiscard]] VanillaGameData::Jar dataJar() const;

    [[nodiscard]] std::unique_ptr<command::commands::FunctionRunner> makeFunctionRunner(std::int32_t permissionLevel);

    /// parity: the `scheduled_events` saved data.
    void saveSchedule() const;
    void loadScoreboard();
    void saveScoreboard() const;
    void loadBossBars();
    void saveStopwatches() const;
    void loadStopwatches();
    void saveBossBars() const;

    void loadFunctions();

    /// parity: `MinecraftServer.reloadResources`.
    bool reloadData(const std::vector<std::string>& selected);

    /// parity: `ServerFunctionManager.tick` and the scheduled functions of `TimerQueue`.
    void tickFunctions();
    void runFunctionById(const wire::common::Identifier& id);

    /// parity: `PlayerList.reloadResources`.
    void resyncAfterReload();

    void addAdvancementHooks(world::AdvancementTriggers::Hooks& hooks);

    /// parity: `AdvancementRewards.grant`'s loot.
    void awardAdvancementLoot(std::string_view uuid, const std::vector<wire::common::Identifier>& tables);

    /// parity: `ServerPlayer.tick`'s `flushDirty`, for every player.
    void flushAdvancements();

    void sendAdvancementPacket(const player::PlayerSession& player, std::int32_t id,
                               const std::function<void(wire::io::ByteBuffer&)>& write) const;

    VanillaGameData vanillaData_;
    ChunkSimulationView chunkSimulation_;
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
    std::unique_ptr<world::PlayerAdvancements> playerAdvancements_;
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

    std::int32_t idleTimeoutMinutes_ = 0;
    bool whitelistEnforced_ = false;

    WiredCommands wired_;

    std::unique_ptr<plugins::RegistryPlayers> pluginPlayers_;
    std::unique_ptr<plugins::LoadedWorlds> pluginWorlds_;
    std::unique_ptr<plugins::RegistryPlaceholders> pluginPlaceholders_;
    std::unique_ptr<plugins::DatabasePluginStorage> pluginStorage_;
    std::unique_ptr<plugins::SessionMenus> pluginMenus_;
    std::unique_ptr<plugins::ServerInventories> pluginInventories_;
    std::unique_ptr<plugins::SessionPackets> pluginPackets_;
    std::unique_ptr<plugins::SessionChannels> pluginChannels_;
    plugins::MiscServices miscServices_;
    plugins::ServerSettings settings_;
    std::unique_ptr<plugins::ServerPluginServices> pluginServices_;
    std::unique_ptr<server::plugins::PluginHost> plugins_;
    std::shared_ptr<void> bukkit_;
    std::vector<bukkit::JavaPluginJar> javaPlugins_;
    plugins::EventSink eventSink_{[this] { return plugins_ == nullptr ? nullptr : &plugins_->events(); }};

    std::unique_ptr<BlockChangeBroadcaster> broadcaster_;
    std::unique_ptr<ExplosionSystem> explosions_;
    std::optional<bedrock::BedrockRecipeList> bedrockRecipes_;
    std::unique_ptr<NaturalSpawningSystem> spawning_;
    std::unique_ptr<MobSpawnerSystem> mobSpawners_;
    std::unique_ptr<ExperienceOrbSystem> orbs_;
    std::unique_ptr<FishingSystem> fishing_;
    world::maps::MapStorage mapStorage_;
    std::unique_ptr<MapSystem> maps_;
    std::unique_ptr<WitherSystem> withers_;
    std::unique_ptr<LightningBoltSystem> lightning_;
    std::unique_ptr<ShriekerSystem> shriekers_;
    std::unique_ptr<SculkCatalystSystem> sculkCatalysts_;
    std::unique_ptr<ConduitSystem> conduits_;
    std::unique_ptr<CombatSystem> combat_;
    entity::entityCombat::Hooks mobCombatHooks_;
    std::map<std::string, std::int32_t> launchWatermarks_;
    std::unique_ptr<FireworkSystem> fireworks_;
    std::unique_ptr<AreaEffectCloudSystem> clouds_;
    std::unique_ptr<EndFightSystem> endFight_;
    std::unique_ptr<RedstoneTickSystem> redstone_;
    std::unique_ptr<RaidSystem> raids_;
    std::unique_ptr<StatusEffectSystem> statusEffects_;
    std::unique_ptr<EnvironmentTickSystem> environment_;
    std::unique_ptr<MobActionSystem> mobActions_;
    std::unique_ptr<MobTickSystem> mobTicks_;
    std::unique_ptr<EntityPushSystem> pushes_;
    std::unique_ptr<VehicleSystem> vehicles_;
    std::unique_ptr<StructureBlockSystem> structureBlocks_;
    std::unique_ptr<ServerListStatus> serverList_;

    wire::protocol::play::Gamemode defaultGamemode_ = wire::protocol::play::Gamemode::Survival;
    std::string difficulty_ = "easy";

    std::optional<wire::io::TcpListener> listener_;
    std::unique_ptr<bedrock::BedrockListener> bedrockListener_;
    std::unique_ptr<BlockTests> blockTests_;
    std::atomic<bool> accepting_{true};
    std::atomic<bool> shuttingDown_{false};
    std::atomic<bool> shutdownComplete_{false};
    std::atomic<bool> restarting_{false};
    std::vector<std::jthread> connections_;
    std::jthread watchdog_;
};

} // namespace basalt::server::runtime
