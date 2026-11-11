// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server;

import com.mojang.datafixers.DataFixer;
import net.minecraft.core.RegistryAccess;
import net.minecraft.server.dedicated.DedicatedServer;
import net.minecraft.server.network.ServerConnectionListener;
import org.basaltmc.bukkit.Natives;
import org.basaltmc.bukkit.facade.ServerAccess;
import org.basaltmc.bukkit.facade.TickQueue;
import com.mojang.authlib.GameProfile;
import java.io.IOException;
import java.net.Proxy;
import java.nio.file.Path;
import java.security.KeyPair;
import java.util.Collection;
import java.util.Map;
import java.util.Optional;
import java.util.Set;
import java.util.UUID;
import java.util.concurrent.CompletableFuture;
import java.util.function.BooleanSupplier;
import java.util.function.Consumer;
import java.util.function.Function;
import java.util.function.Supplier;
import net.minecraft.CrashReport;
import net.minecraft.SystemReport;
import net.minecraft.commands.CommandSource;
import net.minecraft.commands.CommandSourceStack;
import net.minecraft.commands.Commands;
import net.minecraft.core.BlockPos;
import net.minecraft.core.GlobalPos;
import net.minecraft.core.LayeredRegistryAccess;
import net.minecraft.nbt.Tag;
import net.minecraft.network.PacketProcessor;
import net.minecraft.network.chat.ChatDecorator;
import net.minecraft.network.chat.ChatType;
import net.minecraft.network.chat.Component;
import net.minecraft.network.protocol.PacketType;
import net.minecraft.network.protocol.status.ServerStatus;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;
import net.minecraft.server.bossevents.CustomBossEvents;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.server.level.ServerPlayerGameMode;
import net.minecraft.server.level.progress.ChunkLoadStatusView;
import net.minecraft.server.level.progress.LevelLoadListener;
import net.minecraft.server.network.TextFilter;
import net.minecraft.server.notifications.NotificationManager;
import net.minecraft.server.notifications.ServerActivityMonitor;
import net.minecraft.server.packs.repository.PackRepository;
import net.minecraft.server.packs.resources.CloseableResourceManager;
import net.minecraft.server.packs.resources.ResourceManager;
import net.minecraft.server.permissions.LevelBasedPermissionSet;
import net.minecraft.server.permissions.PermissionSet;
import net.minecraft.server.players.NameAndId;
import net.minecraft.server.players.PlayerList;
import net.minecraft.util.ModCheck;
import net.minecraft.util.RandomSource;
import net.minecraft.util.debug.ServerDebugSubscribers;
import net.minecraft.util.debugchart.SampleLogger;
import net.minecraft.util.profiling.ProfileResults;
import net.minecraft.util.thread.ReentrantBlockableEventLoop;
import net.minecraft.world.Difficulty;
import net.minecraft.world.RandomSequences;
import net.minecraft.world.Stopwatches;
import net.minecraft.world.clock.ServerClockManager;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.crafting.RecipeManager;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.GameType;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LevelSettings;
import net.minecraft.world.level.WorldDataConfiguration;
import net.minecraft.world.level.chunk.storage.ChunkIOErrorReporter;
import net.minecraft.world.level.chunk.storage.RegionStorageInfo;
import net.minecraft.world.level.gamerules.GameRule;
import net.minecraft.world.level.gamerules.GameRules;
import net.minecraft.world.level.levelgen.WorldGenSettings;
import net.minecraft.world.level.levelgen.structure.templatesystem.StructureTemplateManager;
import net.minecraft.world.level.saveddata.WeatherData;
import net.minecraft.world.level.storage.CommandStorage;
import net.minecraft.world.level.storage.LevelData;
import net.minecraft.world.level.storage.LevelResource;
import net.minecraft.world.level.storage.LevelStorageSource;
import net.minecraft.world.level.storage.PlayerDataStorage;
import net.minecraft.world.level.storage.SavedDataStorage;
import net.minecraft.world.level.storage.WorldData;
import net.minecraft.world.level.timers.TimerQueue;

public class MinecraftServer extends ReentrantBlockableEventLoop<TickTask> implements CommandSource, ServerInfo, ChunkIOErrorReporter {
    public static volatile int currentTick;
    private ServerAdvancementManager advancements;
    private final ServerConnectionListener connection = new ServerConnectionListener();
    private DataFixer fixer;

    private final java.util.List<Runnable> tickables = new java.util.concurrent.CopyOnWriteArrayList<>();
    private NotificationManager notifications;
    private ServerActivityMonitor activityMonitor;
    private Services serviceSet;
    private CustomBossEvents bossEvents;
    private SavedDataStorage dataStorage;
    private TimerQueue<MinecraftServer> scheduledEvents;
    private PacketProcessor packets;
    private ServerDebugSubscribers debug;
    private LevelLoadListener loadListener;

    protected MinecraftServer() {}

    public static MinecraftServer getServer() {
        return DedicatedServer.instance();
    }

    public RegistryAccess.Frozen registryAccess() {
        return ServerAccess.registries();
    }

    public void execute(Runnable task) {
        TickQueue.run(task);
    }

    public boolean isSameThread() {
        return Natives.onPluginThread();
    }

    public synchronized DataFixer getFixerUpper() {
        if (fixer == null) {
            fixer = ServerAccess.dataFixer();
        }
        return fixer;
    }

    public boolean isDebugging() {
        return false;
    }

    public ServerConnectionListener getConnection() {
        return connection;
    }
    private final ServerFunctionManager functions = new ServerFunctionManager(null, null);
    private PackRepository packRepository;
    private RecipeManager recipeManager;
    private String serverId;
    private LayeredRegistryAccess<RegistryLayer> layeredRegistries;
    private ReloadableServerRegistries.Holder reloadable;
    private final ServerClockManager clockManager = new ServerClockManager();
    private final ServerTickRateManager tickRateManager = new ServerTickRateManager(null);
    private Stopwatches stopwatches;
    private WorldData basaltWorldData;
    private CommandStorage commandStorage;
    private RandomSequences randomSequences;
    private ResourceManager resourceManager;
    public static final String VANILLA_BRAND = "vanilla";
    public static final int SPAWN_POSITION_SEARCH_RADIUS = 5;
    public static final int ABSOLUTE_MAX_WORLD_SIZE = 29999984;
    public static final LevelSettings DEMO_SETTINGS = new LevelSettings("Demo World", net.minecraft.world.level.GameType.SURVIVAL,
        LevelSettings.DifficultySettings.DEFAULT, false, net.minecraft.world.level.WorldDataConfiguration.DEFAULT);
    public static final Supplier<GameRules> DEFAULT_GAME_RULES = () -> new GameRules(net.minecraft.world.flag.FeatureFlagSet.of());
    public static final NameAndId ANONYMOUS_PLAYER_PROFILE = new NameAndId(net.minecraft.util.Util.NIL_UUID, "Anonymous Player");
    public static final String SERVER_THREAD_NAME = "Server thread";
    protected LevelStorageSource.LevelStorageAccess storageSource;
    protected PlayerDataStorage playerDataStorage;
    protected Proxy proxy;
    protected Services services;
    protected WorldData worldData;
    public static <S extends MinecraftServer> S spin(Function<Thread, S> factory) {
        java.util.concurrent.atomic.AtomicReference<S> reference = new java.util.concurrent.atomic.AtomicReference<>();
        Thread thread = new Thread(null, () -> reference.get().runServer(), SERVER_THREAD_NAME);
        S server = factory.apply(thread);
        reference.set(server);
        thread.start();
        return server;
    }
    public MinecraftServer(Thread serverThread, LevelStorageSource.LevelStorageAccess storageSource, PackRepository packRepository, WorldStem worldStem, Optional<GameRules> gameRules, Proxy proxy, DataFixer fixerUpper, Services services, LevelLoadListener levelLoadListener, boolean propagatesCrashes, NotificationManager notificationManager) {
        super(SERVER_THREAD_NAME, propagatesCrashes);
        this.storageSource = storageSource;
        this.proxy = proxy;
        this.services = services;
        this.packRepository = packRepository;
        this.fixer = fixerUpper;
        this.loadListener = levelLoadListener;
        this.notifications = notificationManager;
    }
    protected boolean initServer() throws IOException {
        return true;
    }
    public ChunkLoadStatusView createChunkLoadStatusView(int radius) {
        return new ChunkLoadStatusView() {
            public void moveTo(net.minecraft.resources.ResourceKey<net.minecraft.world.level.Level> dimension, net.minecraft.world.level.ChunkPos centerChunk) {}

            public net.minecraft.world.level.chunk.status.ChunkStatus get(int x, int z) {
                return net.minecraft.world.level.chunk.status.ChunkStatus.FULL;
            }

            public int radius() {
                return radius;
            }
        };
    }
    protected void loadLevel() {
    }
    protected void forceDifficulty() {
    }
    protected void createLevels() {
    }
    protected GlobalPos selectLevelLoadFocusPos() {
        return getRespawnData().globalPos();
    }
    public GameType getDefaultGameType() {
        return GameType.byId(org.bukkit.Bukkit.getDefaultGameMode().getValue());
    }
    public boolean isHardcore() {
        return org.bukkit.Bukkit.getServer().isHardcore();
    }
    public LevelBasedPermissionSet operatorUserPermissions() {
        return net.minecraft.server.permissions.LevelBasedPermissionSet.OWNER;
    }
    public PermissionSet getFunctionCompilationPermissions() {
        return net.minecraft.server.permissions.LevelBasedPermissionSet.OWNER;
    }
    public boolean shouldRconBroadcast() {
        return false;
    }
    public boolean saveAllChunks(boolean silent, boolean flush, boolean force) {
        for (org.bukkit.World world : org.bukkit.Bukkit.getWorlds()) {
            world.save();
        }
        return true;
    }
    public boolean saveEverything(boolean silent, boolean flush, boolean force) {
        return saveAllChunks(silent, flush, force);
    }
    public void close() {
        org.bukkit.Bukkit.shutdown();
    }
    protected void stopServer() {
        org.bukkit.Bukkit.shutdown();
    }
    public String getLocalIp() {
        return org.bukkit.Bukkit.getIp();
    }
    public void setLocalIp(String ip) {
        org.basaltmc.bukkit.WorldEnvNatives.setSetting("bind", ip);
    }
    public boolean isRunning() {
        return !org.bukkit.Bukkit.getServer().isStopping();
    }
    public void halt(boolean wait) {
        org.bukkit.Bukkit.shutdown();
    }
    protected void runServer() {
    }
    public NotificationManager notificationManager() {
        if (notifications == null) {
            notifications = new NotificationManager();
        }
        return notifications;
    }
    protected void waitUntilNextTick() {
    }
    protected void waitForTasks() {
    }
    public TickTask wrapRunnable(Runnable runnable) {
        return new TickTask((int) org.basaltmc.bukkit.Natives.currentTick(), runnable);
    }
    protected boolean shouldRun(TickTask task) {
        return true;
    }
    protected boolean pollTask() {
        return false;
    }
    protected void doRunTask(TickTask task) {
        task.run();
    }
    public Optional<Path> getWorldScreenshotFile() {
        return Optional.empty();
    }
    public Path getServerDirectory() {
        return Path.of(".");
    }
    public ServerActivityMonitor getServerActivityMonitor() {
        if (activityMonitor == null) {
            activityMonitor = new ServerActivityMonitor(notificationManager(), 30);
        }
        return activityMonitor;
    }
    protected void onServerCrash(CrashReport report) {
    }
    protected void onServerExit() {
    }
    public boolean isPaused() {
        return false;
    }
    protected void tickServer(BooleanSupplier haveTime) {
        tickables.forEach(Runnable::run);
    }
    protected void processPacketsAndTick(boolean sprinting) {
    }
    public void onTickRateChanged() {
    }
    protected SampleLogger getTickTimeLogger() {
        return null;
    }
    public boolean isTickTimeLoggingEnabled() {
        return false;
    }
    protected void tickChildren(BooleanSupplier haveTime) {
    }
    protected void tickConnection() {
    }
    public void forceGameTimeSynchronization() {
    }
    public void addTickable(Runnable tickable) {
        tickables.add(tickable);
    }
    protected void setId(String serverId) {
        this.serverId = serverId;
    }
    public boolean isShutdown() {
        return org.bukkit.Bukkit.getServer().isStopping();
    }
    public Path getFile(String name) {
        return getServerDirectory().resolve(name);
    }
    public ServerLevel overworld() {
        ServerLevel first = null;
        for (org.bukkit.World world : org.bukkit.Bukkit.getWorlds()) {
            ServerLevel level = ((org.bukkit.craftbukkit.CraftWorld) world).getHandle();
            if (world.getEnvironment() == org.bukkit.World.Environment.NORMAL) {
                return level;
            }
            first = first == null ? level : first;
        }
        return first;
    }
    public ServerLevel getLevel(ResourceKey<Level> dimension) {
        for (ServerLevel level : getAllLevels()) {
            if (level.dimension() == dimension) {
                return level;
            }
        }
        return null;
    }
    public Set<ResourceKey<Level>> levelKeys() {
        Set<ResourceKey<Level>> keys = new java.util.LinkedHashSet<>();
        for (ServerLevel level : getAllLevels()) {
            keys.add(level.dimension());
        }
        return keys;
    }
    public Iterable<ServerLevel> getAllLevels() {
        java.util.List<ServerLevel> levels = new java.util.ArrayList<>();
        for (org.bukkit.World world : org.bukkit.Bukkit.getWorlds()) {
            levels.add(((org.bukkit.craftbukkit.CraftWorld) world).getHandle());
        }
        return levels;
    }
    public String getServerVersion() {
        return org.bukkit.Bukkit.getServer().getMinecraftVersion();
    }
    public int getPlayerCount() {
        return org.bukkit.Bukkit.getServer().getOnlinePlayers().size();
    }
    public String[] getPlayerNames() {
        return org.bukkit.Bukkit.getServer().getOnlinePlayers().stream().map(org.bukkit.entity.Player::getName).toArray(String[]::new);
    }
    public String getServerModName() {
        return "Basalt";
    }
    public ServerClockManager clockManager() {
        return clockManager;
    }
    public SystemReport fillSystemReport(SystemReport systemReport) {
        systemReport.setDetail("Server Running", () -> Boolean.toString(isRunning()));
        systemReport.setDetail("Player Count", () -> Integer.toString(org.bukkit.Bukkit.getOnlinePlayers().size()));
        return systemReport;
    }
    public SystemReport fillServerSystemReport(SystemReport systemReport) {
        return fillSystemReport(systemReport);
    }
    public ModCheck getModdedStatus() {
        return new ModCheck(ModCheck.Confidence.PROBABLY_NOT, "Basalt native runtime");
    }
    public void sendSystemMessage(Component message) {
        org.bukkit.Bukkit.getConsoleSender().sendMessage(message.asAdventure());
    }
    public KeyPair getKeyPair() {
        return org.basaltmc.bukkit.facade.ServerKeys.pair();
    }
    public int getPort() {
        return org.bukkit.Bukkit.getServer().getPort();
    }
    public void setPort(int port) {
        org.basaltmc.bukkit.WorldEnvNatives.setSetting("port", String.valueOf(port));
    }
    public GameProfile getSingleplayerProfile() {
        return null;
    }
    public void setSingleplayerProfile(GameProfile singleplayerProfile) {

    }
    public boolean isSingleplayer() {
        return false;
    }
    protected void initializeKeyPair() {
    }
    public void setDifficulty(Difficulty difficulty, boolean ignoreLock) {
        for (org.bukkit.World world : org.bukkit.Bukkit.getWorlds()) {
            org.basaltmc.bukkit.WorldNatives.setDifficulty(world.getName(), difficulty.name().toLowerCase(java.util.Locale.ROOT));
        }
    }
    public int getScaledTrackingDistance(int baseRange) {
        return baseRange;
    }
    public void updateMobSpawningFlags() {
    }
    public void setDifficultyLocked(boolean locked) {
        for (org.bukkit.World each : org.bukkit.Bukkit.getWorlds()) {
            org.basaltmc.bukkit.WorldNatives.setDifficultyLocked(each.getName(), locked);
        }
    }
    public boolean isDemo() {
        return false;
    }
    public void setDemo(boolean demo) {
    }
    public Map<String, String> getCodeOfConducts() {
        return Map.of();
    }
    public Optional<ServerResourcePackInfo> getServerResourcePack() {
        return Optional.empty();
    }
    public boolean isResourcePackRequired() {
        return false;
    }
    public boolean isDedicatedServer() {
        return true;
    }
    public int getRateLimitPacketsPerSecond() {
        return 0;
    }
    public int getCommandSpamThresholdSeconds() {
        return 10;
    }
    public int getChatSpamThresholdSeconds() {
        return 10;
    }
    public boolean usesAuthentication() {
        return org.bukkit.Bukkit.getServer().getOnlineMode();
    }
    public void setUsesAuthentication(boolean onlineMode) {
        org.basaltmc.bukkit.WorldEnvNatives.setSetting("online_mode", String.valueOf(onlineMode));
    }
    public boolean getPreventProxyConnections() {
        return false;
    }
    public void setPreventProxyConnections(boolean preventProxyConnections) {
    }
    public boolean useNativeTransport() {
        return true;
    }
    public boolean allowFlight() {
        return org.bukkit.Bukkit.getServer().getAllowFlight();
    }
    public String getMotd() {
        return org.bukkit.Bukkit.getServer().getMotd();
    }
    public void setMotd(String motd) {
        org.basaltmc.bukkit.WorldEnvNatives.setSetting("motd", motd);
    }
    public boolean isStopped() {
        return org.bukkit.Bukkit.getServer().isStopping();
    }
    private PlayerList playerList;
    public synchronized PlayerList getPlayerList() {
        if (playerList == null) {
            playerList = new PlayerList(this, null, null, null);
        }
        return playerList;
    }
    public void setPlayerList(PlayerList players) {
        this.playerList = players;
    }
    public boolean isPublished() {
        return true;
    }
    public void setDefaultGameType(GameType gameType) {
        org.bukkit.Bukkit.setDefaultGameMode(org.bukkit.GameMode.getByValue(gameType.getId()));
    }
    public int enforceGameTypeForPlayers(GameType gameType) {
        int changed = 0;
        for (org.bukkit.entity.Player player : org.bukkit.Bukkit.getOnlinePlayers()) {
            org.bukkit.GameMode mode = org.bukkit.GameMode.getByValue(gameType.getId());
            if (player.getGameMode() != mode) {
                player.setGameMode(mode);
                changed++;
            }
        }
        return changed;
    }
    public boolean isReady() {
        return true;
    }
    public boolean publishServer(MultiplayerScope scope, boolean allowCommands, int port) {
        return false;
    }
    public boolean unpublishServer() {
        return false;
    }
    public int getTickCount() {
        return org.bukkit.Bukkit.getServer().getCurrentTick();
    }
    public boolean isUnderSpawnProtection(ServerLevel level, BlockPos pos, Player player) {
        org.bukkit.World world = level.getWorld();
        if (world.getEnvironment() != org.bukkit.World.Environment.NORMAL || org.bukkit.Bukkit.getOperators().isEmpty()) {
            return false;
        }
        org.bukkit.entity.Player bukkit = ((net.minecraft.server.level.ServerPlayer) player).bukkitPlayer();
        int radius = DedicatedServer.instance().spawnProtectionRadius();
        if (bukkit.isOp() || radius <= 0) {
            return false;
        }
        org.bukkit.Location spawn = world.getSpawnLocation();
        return Math.max(Math.abs(pos.getX() - spawn.getBlockX()), Math.abs(pos.getZ() - spawn.getBlockZ())) <= radius;
    }
    public boolean repliesToStatus() {
        return true;
    }
    public boolean hidesOnlinePlayers() {
        return false;
    }
    public Proxy getProxy() {
        return java.net.Proxy.NO_PROXY;
    }
    public int playerIdleTimeout() {
        return 0;
    }
    public void setPlayerIdleTimeout(int playerIdleTimeout) {
        org.basaltmc.bukkit.WorldEnvNatives.setSetting("idle_timeout", String.valueOf(playerIdleTimeout));
    }
    public Services services() {
        if (serviceSet == null) {
            serviceSet = Services.create(null, new java.io.File("."));
        }
        return serviceSet;
    }
    public ServerStatus getStatus() {
        ServerStatus.Players players = new ServerStatus.Players(org.bukkit.Bukkit.getMaxPlayers(), org.bukkit.Bukkit.getOnlinePlayers().size(),
            org.bukkit.Bukkit.getOnlinePlayers().stream().map(each -> new NameAndId(each.getUniqueId(), each.getName())).toList());
        return new ServerStatus(Component.literal(getMotd()), java.util.Optional.of(players), java.util.Optional.of(ServerStatus.Version.current()),
            java.util.Optional.empty(), false);
    }
    public void invalidateStatus() {
    }
    public int getAbsoluteMaxWorldSize() {
        return ABSOLUTE_MAX_WORLD_SIZE;
    }
    public boolean scheduleExecutables() {
        return !isStopped();
    }
    public void executeIfPossible(Runnable command) {
        execute(command);
    }
    public Thread getRunningThread() {
        return Thread.currentThread();
    }
    public int getCompressionThreshold() {
        return 256;
    }
    public boolean enforceSecureProfile() {
        return false;
    }
    public long getNextTickTime() {
        return System.currentTimeMillis() + 50L;
    }
    public ServerAdvancementManager getAdvancements() {
        if (advancements == null) {
            advancements = new ServerAdvancementManager(null);
        }
        return advancements;
    }
    public ServerFunctionManager getFunctions() {
        return functions;
    }
    public CompletableFuture<Void> reloadResources(Collection<String> packsToEnable) {
        org.basaltmc.bukkit.RestNatives.reloadData();
        return CompletableFuture.completedFuture(null);
    }
    public static WorldDataConfiguration configurePackRepository(PackRepository packRepository, WorldDataConfiguration initialDataConfig, boolean initMode, boolean safeMode) {
        java.util.List<String> selected = safeMode ? java.util.List.of("vanilla")
            : initMode ? initialDataConfig.dataPacks().getEnabled() : java.util.List.copyOf(packRepository.getSelectedIds());
        java.util.List<String> disabled = packRepository.getAvailableIds().stream().filter(id -> !selected.contains(id)).toList();
        return new WorldDataConfiguration(new net.minecraft.world.level.DataPackConfig(selected, disabled), initialDataConfig.enabledFeatures());
    }
    public void kickUnlistedPlayers() {
        if (org.bukkit.Bukkit.hasWhitelist()) {
            for (org.bukkit.entity.Player each : org.bukkit.Bukkit.getOnlinePlayers()) {
                if (!each.isWhitelisted()) {
                    each.kickPlayer("You are not white-listed on this server!");
                }
            }
        }
    }
    public PackRepository getPackRepository() {
        if (packRepository == null) {
            packRepository = new PackRepository();
        }
        return packRepository;
    }
    private Commands commands;
    public synchronized Commands getCommands() {
        if (commands == null) {
            commands = new Commands(Commands.CommandSelection.DEDICATED, null);
        }
        return commands;
    }
    public CommandSourceStack createCommandSourceStack() {
        return org.basaltmc.bukkit.facade.BukkitCommandBridge.stackOf(org.bukkit.Bukkit.getServer().getConsoleSender(), this);
    }
    public ServerLevel findRespawnDimension() {
        return ((org.bukkit.craftbukkit.CraftWorld) org.bukkit.Bukkit.getWorlds().get(0)).getHandle();
    }
    public void setRespawnData(LevelData.RespawnData respawnData) {
        net.minecraft.core.GlobalPos at = respawnData.globalPos();
        for (org.bukkit.World each : org.bukkit.Bukkit.getWorlds()) {
            if (((org.bukkit.craftbukkit.CraftWorld) each).getHandle().dimension().equals(at.dimension())) {
                each.setSpawnLocation(new org.bukkit.Location(each, at.pos().getX(), at.pos().getY(), at.pos().getZ(), respawnData.yaw(), respawnData.pitch()));
                return;
            }
        }
    }
    public LevelData.RespawnData getRespawnData() {
        org.bukkit.World main = org.bukkit.Bukkit.getWorlds().get(0);
        org.bukkit.Location spawn = main.getSpawnLocation();
        return new LevelData.RespawnData(new net.minecraft.core.GlobalPos(((org.bukkit.craftbukkit.CraftWorld) main).getHandle().dimension(),
            new net.minecraft.core.BlockPos(spawn.getBlockX(), spawn.getBlockY(), spawn.getBlockZ())), spawn.getYaw(), spawn.getPitch());
    }
    public boolean acceptsSuccess() {
        return true;
    }
    public boolean acceptsFailure() {
        return true;
    }
    public boolean shouldInformAdmins() {
        return true;
    }
    public WorldGenSettings getWorldGenSettings() {
        org.bukkit.World main = org.bukkit.Bukkit.getWorlds().get(0);
        return WorldGenSettings.of(new net.minecraft.world.level.levelgen.WorldOptions(main.getSeed(), main.canGenerateStructures(), false), registryAccess());
    }
    public RecipeManager getRecipeManager() {
        if (recipeManager == null) {
            recipeManager = new RecipeManager(registryAccess());
        }
        return recipeManager;
    }
    private volatile ServerScoreboard scoreboard;
    public ServerScoreboard getScoreboard() {
        ServerScoreboard board = scoreboard;
        if (board == null) {
            synchronized (this) {
                board = scoreboard == null ? (scoreboard = new ServerScoreboard(this)) : scoreboard;
            }
        }
        return board;
    }
    public CommandStorage getCommandStorage() {
        if (commandStorage == null) {
            commandStorage = new CommandStorage(null);
        }
        return commandStorage;
    }
    public Stopwatches getStopwatches() {
        if (stopwatches == null) {
            stopwatches = new Stopwatches();
        }
        return stopwatches;
    }
    public CustomBossEvents getCustomBossEvents() {
        if (bossEvents == null) {
            bossEvents = new CustomBossEvents();
        }
        return bossEvents;
    }
    public RandomSource getRandomSequence(Identifier key) {
        return getRandomSequences().get(key, org.bukkit.Bukkit.getWorlds().get(0).getSeed());
    }
    public RandomSequences getRandomSequences() {
        if (randomSequences == null) {
            randomSequences = new RandomSequences();
        }
        return randomSequences;
    }
    public void setWeatherParameters(int clearTime, int rainTime, boolean raining, boolean thundering) {
        for (org.bukkit.World world : org.bukkit.Bukkit.getWorlds()) {
            org.basaltmc.bukkit.WorldNatives.setWeather(world.getName(), raining, thundering);
        }
    }
    public WeatherData getWeatherData() {
        return new WeatherData(org.bukkit.Bukkit.getWorlds().get(0));
    }
    public boolean isEnforceWhitelist() {
        return org.bukkit.Bukkit.getServer().isWhitelistEnforced();
    }
    public void setEnforceWhitelist(boolean enforceWhitelist) {
        org.bukkit.Bukkit.getServer().setWhitelistEnforced(enforceWhitelist);
    }
    public boolean isUsingWhitelist() {
        return org.bukkit.Bukkit.getServer().hasWhitelist();
    }
    public void setUsingWhitelist(boolean usingWhitelist) {
        org.bukkit.Bukkit.getServer().setWhitelist(usingWhitelist);
    }
    public float getCurrentSmoothedTickTime() {
        return (float) org.bukkit.Bukkit.getAverageTickTime();
    }
    public ServerTickRateManager tickRateManager() {
        return tickRateManager;
    }
    public long getAverageTickTimeNanos() {
        return (long) (org.bukkit.Bukkit.getAverageTickTime() * 1.0E6);
    }
    public long[] getTickTimesNanos() {
        return org.bukkit.Bukkit.getTickTimes().clone();
    }
    public LevelBasedPermissionSet getProfilePermissions(NameAndId nameAndId) {
        return org.bukkit.Bukkit.getOfflinePlayer(nameAndId.id()).isOp() ? net.minecraft.server.permissions.LevelBasedPermissionSet.OWNER : net.minecraft.server.permissions.LevelBasedPermissionSet.ALL;
    }
    public boolean isSingleplayerOwner(NameAndId nameAndId) {
        return false;
    }
    public void dumpServerProperties(Path path) throws IOException {
        java.util.Properties out = new java.util.Properties();
        net.minecraft.server.dedicated.DedicatedServerProperties current = DedicatedServer.instance().getProperties();
        out.setProperty("level-name", current.levelName);
        out.setProperty("server-port", Integer.toString(current.serverPort));
        out.setProperty("max-players", Integer.toString(current.maxPlayers.get()));
        out.setProperty("view-distance", Integer.toString(current.viewDistance.get()));
        out.setProperty("motd", current.motd.get());
        try (java.io.OutputStream stream = java.nio.file.Files.newOutputStream(path)) {
            out.store(stream, "Server properties dump");
        }
    }
    protected void endMetricsRecordingTick() {
    }
    public boolean isRecordingMetrics() {
        return false;
    }
    public void startRecordingMetrics(Consumer<ProfileResults> onStopped, Consumer<Path> onFinished) {
    }
    public void stopRecordingMetrics() {
    }
    public void finishRecordingMetrics() {
    }
    public void cancelRecordingMetrics() {
    }
    public Path getWorldPath(LevelResource resource) {
        return org.bukkit.Bukkit.getWorlds().get(0).getWorldFolder().toPath().resolve(resource.id());
    }
    public boolean forceSynchronousWrites() {
        return false;
    }
    private StructureTemplateManager templateManager;
    public StructureTemplateManager getStructureTemplateManager() {
        if (templateManager == null) {
            templateManager = new StructureTemplateManager(null, null, null, null);
        }
        return templateManager;
    }
    public WorldData getWorldData() {
        if (basaltWorldData == null) {
            basaltWorldData = new net.minecraft.world.level.storage.BasaltWorldData();
        }
        return basaltWorldData;
    }
    public LayeredRegistryAccess<RegistryLayer> registries() {
        if (layeredRegistries == null) {
            layeredRegistries = new LayeredRegistryAccess<>(java.util.List.of(RegistryLayer.values()));
        }
        return layeredRegistries;
    }
    public ReloadableServerRegistries.Holder reloadableRegistries() {
        if (reloadable == null) {
            reloadable = new ReloadableServerRegistries.Holder(registryAccess());
        }
        return reloadable;
    }
    public TextFilter createTextFilterForPlayer(ServerPlayer player) {
        return TextFilter.DUMMY;
    }
    public ServerPlayerGameMode createGameModeForPlayer(ServerPlayer player) {
        return new ServerPlayerGameMode(player);
    }
    public GameType getForcedGameType() {
        return null;
    }
    public boolean forceGameMode() {
        return false;
    }
    public void setForceGameMode(boolean forceGameMode) {
    }
    public ResourceManager getResourceManager() {
        if (resourceManager == null) {
            resourceManager = new org.basaltmc.bukkit.facade.NativeResourceManager();
        }
        return resourceManager;
    }
    public boolean isCurrentlySaving() {
        return false;
    }
    public boolean isTimeProfilerRunning() {
        return false;
    }
    public void startTimeProfiler() {
    }
    public ProfileResults stopTimeProfiler() {
        return null;
    }
    public int getMaxChainedNeighborUpdates() {
        return 1000000;
    }
    public void logChatMessage(Component message, ChatType.Bound chatType, String tag) {
        org.bukkit.Bukkit.getConsoleSender().sendMessage(message.asAdventure());
    }
    public ChatDecorator getChatDecorator() {
        return (player, plain) -> plain;
    }
    public boolean logIPs() {
        return true;
    }
    public void handleCustomClickAction(Identifier id, Optional<Tag> payload) {
        // plugins observe custom click actions through the native event instead
    }
    public LevelLoadListener getLevelLoadListener() {
        if (loadListener != null) {
            return loadListener;
        }
        return new LevelLoadListener() {
            public void start(LevelLoadListener.Stage stage, int totalChunks) {}

            public void update(LevelLoadListener.Stage stage, int currentChunks, int totalChunks) {}

            public void finish(LevelLoadListener.Stage stage) {}

            public void updateFocus(net.minecraft.resources.ResourceKey<net.minecraft.world.level.Level> dimension, net.minecraft.world.level.ChunkPos chunkPos) {}
        };
    }
    public boolean setAutoSave(boolean enable) {
        for (org.bukkit.World world : org.bukkit.Bukkit.getWorlds()) {
            world.setAutoSave(enable);
        }
        return true;
    }
    public boolean isAutoSave() {
        return true;
    }
    public <T> void onGameRuleChanged(GameRule<T> rule, T value) {

    }
    public GameRules getGlobalGameRules() {
        return overworld().getGameRules();
    }
    public SavedDataStorage getDataStorage() {
        if (dataStorage == null) {
            dataStorage = new SavedDataStorage(Path.of("."), getFixerUpper(), registryAccess());
        }
        return dataStorage;
    }
    public TimerQueue<MinecraftServer> getScheduledEvents() {
        if (scheduledEvents == null) {
            scheduledEvents = new TimerQueue<>();
        }
        return scheduledEvents;
    }
    public GameRules getGameRules() {
        return overworld().getGameRules();
    }
    public boolean acceptsTransfers() {
        return false;
    }
    public void reportChunkLoadFailure(Throwable throwable, RegionStorageInfo storageInfo, ChunkPos pos) {
        org.basaltmc.bukkit.Natives.log(3, "Server", "Failed to load chunk " + pos + ": " + throwable);
    }
    public void reportChunkSaveFailure(Throwable throwable, RegionStorageInfo storageInfo, ChunkPos pos) {
        org.basaltmc.bukkit.Natives.log(3, "Server", "Failed to save chunk " + pos + ": " + throwable);
    }
    protected void warnOnLowDiskSpace() {
    }
    public void sendLowDiskSpaceWarning() {
    }
    public void reportPacketHandlingException(Throwable throwable, PacketType<?> packetType) {
        org.basaltmc.bukkit.Natives.log(3, "Server", "Failed to handle packet " + packetType + ": " + throwable);
    }
    public ServerLinks serverLinks() {
        String[] flat = org.basaltmc.bukkit.MiscNatives.linkList();
        java.util.List<ServerLinks.Entry> entries = new java.util.ArrayList<>();
        for (int i = 0; i + 2 < flat.length; i += 3) {
            java.net.URI url = java.net.URI.create(flat[i + 2]);
            if (flat[i].isEmpty()) {
                entries.add(ServerLinks.Entry.custom(org.basaltmc.bukkit.facade.ChatJson.fromAdventure(org.basaltmc.bukkit.misc.Json.component(flat[i + 1])), url));
                continue;
            }
            entries.add(ServerLinks.KnownLinkType.valueOf(flat[i].toUpperCase(java.util.Locale.ROOT)).create(url));
        }
        return entries.isEmpty() ? ServerLinks.EMPTY : new ServerLinks(entries);
    }
    protected int pauseWhenEmptySeconds() {
        String value = org.basaltmc.bukkit.WorldEnvNatives.setting("pause_when_empty_seconds");
        return value == null ? 60 : Integer.parseInt(value);
    }
    public PacketProcessor packetProcessor() {
        if (packets == null) {
            packets = new PacketProcessor(Thread.currentThread());
        }
        return packets;
    }
    public ServerDebugSubscribers debugSubscribers() {
        if (debug == null) {
            debug = new ServerDebugSubscribers(this);
        }
        return debug;
    }
    public static enum MultiplayerScope {
        OFF, LAN;
        public Component getDisplayName() {
        return Component.literal(org.bukkit.Bukkit.getServer().getName());
        }
        public Component getTooltip() {
            return Component.translatable("menu.multiplayerOptions.network." + name().toLowerCase(java.util.Locale.ROOT) + ".tooltip");
        }
    }
    public static record ServerResourcePackInfo(UUID id, String url, String hash, boolean isRequired, Component prompt) {
    }
    static record ReloadableResources(CloseableResourceManager resourceManager, ReloadableServerResources managers) implements AutoCloseable {
        public void close() {
            resourceManager.close();
        }
    }
    static class TimeProfiler {
        private TimeProfiler(long startNanos, int startTick) {
        }
    }
}
