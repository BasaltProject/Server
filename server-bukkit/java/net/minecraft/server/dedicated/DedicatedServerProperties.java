// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.dedicated;

import com.google.gson.JsonObject;
import java.nio.file.Path;
import java.util.Arrays;
import java.util.Optional;
import java.util.Properties;
import java.util.concurrent.TimeUnit;
import net.minecraft.core.HolderLookup;
import net.minecraft.core.RegistryAccess;
import net.minecraft.core.registries.Registries;
import net.minecraft.server.MinecraftServer;
import net.minecraft.server.permissions.LevelBasedPermissionSet;
import net.minecraft.server.permissions.PermissionLevel;
import net.minecraft.util.Mth;
import net.minecraft.world.Difficulty;
import net.minecraft.world.level.DataPackConfig;
import net.minecraft.world.level.GameType;
import net.minecraft.world.level.levelgen.WorldDimensions;
import net.minecraft.world.level.levelgen.WorldOptions;

public class DedicatedServerProperties extends Settings<DedicatedServerProperties> {
    private static String nativeText(String key, String fallback) {
        try {
            String value = org.basaltmc.bukkit.WorldEnvNatives.setting(key);
            return value == null ? fallback : value;
        } catch (Throwable unbound) {
            return fallback;
        }
    }
    private static boolean nativeFlag(String key, boolean fallback) {
        return Boolean.parseBoolean(nativeText(key, String.valueOf(fallback)));
    }

    public static final String MANAGEMENT_SERVER_TLS_ENABLED_KEY = "management-server-tls-enabled";
    public static final String MANAGEMENT_SERVER_TLS_KEYSTORE_KEY = "management-server-tls-keystore";
    public static final String MANAGEMENT_SERVER_TLS_KEYSTORE_PASSWORD_KEY = "management-server-tls-keystore-password";
    public final boolean onlineMode = get("online-mode", true);
    public final boolean preventProxyConnections = get("prevent-proxy-connections", false);
    public final String serverIp = get("server-ip", "");
    public final MutableValue<Boolean> allowFlight = getMutable("allow-flight", false);
    public final MutableValue<String> motd = getMutable("motd", "A Minecraft Server");
    public final boolean codeOfConduct = get("enable-code-of-conduct", false);
    public final String bugReportLink = get("bug-report-link", "");
    public final MutableValue<Boolean> forceGameMode = getMutable("force-gamemode", false);
    public final MutableValue<Boolean> enforceWhitelist = getMutable("enforce-whitelist", false);
    public final MutableValue<Difficulty> difficulty = getMutable("difficulty", dispatchNumberOrString(Difficulty::byId, Difficulty::byName), Difficulty::getSerializedName, Difficulty.EASY);
    public final MutableValue<GameType> gameMode = getMutable("gamemode", dispatchNumberOrString(GameType::byId, GameType::byName), GameType::getName, GameType.SURVIVAL);
    public final String levelName = get("level-name", "world");
    public final int serverPort = get("server-port", 25565);
    public final boolean managementServerEnabled = get("management-server-enabled", nativeFlag("management_enabled", false));
    public final String managementServerHost = get("management-server-host", nativeText("management_host", "localhost"));
    public final int managementServerPort = get("management-server-port", Integer.parseInt(nativeText("management_port", "0")));
    public final String managementServerSecret = get("management-server-secret", net.minecraft.server.jsonrpc.security.SecurityConfig.generateSecretKey());
    public final boolean managementServerTlsEnabled = get(MANAGEMENT_SERVER_TLS_ENABLED_KEY, nativeFlag("management_tls", true));
    public final String managementServerTlsKeystore = get(MANAGEMENT_SERVER_TLS_KEYSTORE_KEY, "");
    public final String managementServerTlsKeystorePassword = get(MANAGEMENT_SERVER_TLS_KEYSTORE_PASSWORD_KEY, "");
    public final String managementServerAllowedOrigins = get("management-server-allowed-origins", "");
    public final Boolean announcePlayerAchievements = getLegacyBoolean("announce-player-achievements");
    public final boolean enableQuery = get("enable-query", false);
    public final int queryPort = get("query.port", 25565);
    public final boolean enableRcon = get("enable-rcon", false);
    public final int rconPort = get("rcon.port", 25575);
    public final String rconPassword = get("rcon.password", "");
    public final boolean hardcore = get("hardcore", false);
    public final boolean useNativeTransport = get("use-native-transport", true);
    public final MutableValue<Integer> spawnProtection = getMutable("spawn-protection", 16);
    public final MutableValue<LevelBasedPermissionSet> opPermissions = getMutable("op-permission-level", DedicatedServerProperties::deserializePermission, DedicatedServerProperties::serializePermission, LevelBasedPermissionSet.OWNER);
    public final LevelBasedPermissionSet functionPermissions = get("function-permission-level", DedicatedServerProperties::deserializePermission, DedicatedServerProperties::serializePermission, LevelBasedPermissionSet.GAMEMASTER);
    public final long maxTickTime = get("max-tick-time", TimeUnit.MINUTES.toMillis(1L));
    public final int maxChainedNeighborUpdates = get("max-chained-neighbor-updates", 1000000);
    public final int rateLimitPacketsPerSecond = get("rate-limit", 0);
    public final int commandSpamThresholdSeconds = get("command-spam-threshold-seconds", 10);
    public final int chatSpamThresholdSeconds = get("chat-spam-threshold-seconds", 10);
    public final MutableValue<Integer> viewDistance = getMutable("view-distance", 10);
    public final MutableValue<Integer> simulationDistance = getMutable("simulation-distance", 10);
    public final MutableValue<Integer> maxPlayers = getMutable("max-players", 20);
    public final int networkCompressionThreshold = get("network-compression-threshold", 256);
    public final boolean broadcastRconToOps = get("broadcast-rcon-to-ops", true);
    public final boolean broadcastConsoleToOps = get("broadcast-console-to-ops", true);
    public final int maxWorldSize = get("max-world-size", value -> Mth.clamp(value, 1, 29999984), 29999984);
    public final boolean syncChunkWrites = get("sync-chunk-writes", true);
    public final String regionFileComression = get("region-file-compression", "deflate");
    public final boolean enableJmxMonitoring = get("enable-jmx-monitoring", false);
    public final MutableValue<Boolean> enableStatus = getMutable("enable-status", true);
    public final MutableValue<Boolean> hideOnlinePlayers = getMutable("hide-online-players", false);
    public final MutableValue<Integer> entityBroadcastRangePercentage = getMutable("entity-broadcast-range-percentage", text -> Mth.clamp(Integer.parseInt(text), 10, 1000), 100);
    public final String textFilteringConfig = get("text-filtering-config", "");
    public final int textFilteringVersion = get("text-filtering-version", 0);
    public final Optional<MinecraftServer.ServerResourcePackInfo> serverResourcePackInfo = Optional.empty();
    public final DataPackConfig initialDataPackConfiguration = datapacks();
    public final MutableValue<Integer> playerIdleTimeout = getMutable("player-idle-timeout", 0);
    public final MutableValue<Integer> statusHeartbeatInterval = getMutable("status-heartbeat-interval", 0);
    public final MutableValue<Boolean> whiteList = getMutable("white-list", true);
    public final boolean enforceSecureProfile = get("enforce-secure-profile", true);
    public final boolean logIPs = get("log-ips", true);
    public final MutableValue<Integer> pauseWhenEmptySeconds = getMutable("pause-when-empty-seconds", 60);
    public final WorldOptions worldOptions = new WorldOptions(seed(), get("generate-structures", true), false);
    public final MutableValue<Boolean> acceptsTransfers = getMutable("accepts-transfers", false);

    public DedicatedServerProperties(Properties settings) {
        super(settings);
    }

    private long seed() {
        String text = get("level-seed", "");
        if (text.isEmpty()) {
            return org.bukkit.Bukkit.getWorlds().isEmpty() ? 0L : org.bukkit.Bukkit.getWorlds().get(0).getSeed();
        }
        try {
            return Long.parseLong(text);
        } catch (NumberFormatException notNumeric) {
            return text.hashCode();
        }
    }

    private DataPackConfig datapacks() {
        String enabled = get("initial-enabled-packs", String.join(",", DataPackConfig.DEFAULT.getEnabled()));
        String disabled = get("initial-disabled-packs", String.join(",", DataPackConfig.DEFAULT.getDisabled()));
        return new DataPackConfig(split(enabled), split(disabled));
    }

    private static java.util.List<String> split(String text) {
        return Arrays.stream(text.split(",")).map(String::trim).filter(part -> !part.isEmpty()).toList();
    }

    public static DedicatedServerProperties fromFile(Path file) {
        return new DedicatedServerProperties(loadFromFile(file));
    }

    @Override
    protected DedicatedServerProperties reload(RegistryAccess registryAccess, Properties properties) {
        return new DedicatedServerProperties(properties);
    }

    public static LevelBasedPermissionSet deserializePermission(String value) {
        return LevelBasedPermissionSet.forLevel(PermissionLevel.byId(Integer.parseInt(value.trim())));
    }

    public static String serializePermission(LevelBasedPermissionSet permission) {
        return Integer.toString(permission.level().id());
    }

    public WorldDimensions createDimensions(HolderLookup.Provider registries) {
        return new WorldDimensions(org.basaltmc.bukkit.facade.ServerAccess.registries().lookupOrThrow(Registries.LEVEL_STEM));
    }

    static record WorldDimensionData(JsonObject generatorSettings, String levelType) {
    }
}
