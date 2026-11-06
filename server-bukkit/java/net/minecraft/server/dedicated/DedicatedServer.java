// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.dedicated;

import net.minecraft.core.RegistryAccess;
import net.minecraft.server.MinecraftServer;
import net.minecraft.server.ServerInterface;
import com.mojang.datafixers.DataFixer;
import java.io.IOException;
import java.nio.file.Path;
import java.util.Map;
import java.util.Optional;
import java.util.function.BooleanSupplier;
import net.minecraft.SystemReport;
import net.minecraft.commands.CommandSourceStack;
import net.minecraft.core.BlockPos;
import net.minecraft.server.ServerLinks;
import net.minecraft.server.Services;
import net.minecraft.server.WorldStem;
import net.minecraft.server.jsonrpc.ManagementServer;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.server.network.TextFilter;
import net.minecraft.server.notifications.NotificationManager;
import net.minecraft.server.packs.repository.PackRepository;
import net.minecraft.server.permissions.LevelBasedPermissionSet;
import net.minecraft.server.permissions.PermissionSet;
import net.minecraft.server.players.NameAndId;
import net.minecraft.util.debugchart.SampleLogger;
import net.minecraft.world.Difficulty;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.level.GameType;
import net.minecraft.world.level.gamerules.GameRules;
import net.minecraft.world.level.storage.LevelStorageSource;

public class DedicatedServer extends MinecraftServer implements ServerInterface {
    private static final DedicatedServer kInstance = new DedicatedServer();

    private DedicatedServer() {}

    private DedicatedServerProperties properties;
    private DedicatedServerSettings settings;
    public static DedicatedServer instance() {
        return kInstance;
    }

    public static MinecraftServer getServer() {
        return kInstance;
    }

    @Override
    public RegistryAccess.Frozen registryAccess() {
        return super.registryAccess();
    }
    public DedicatedServer(Thread serverThread, LevelStorageSource.LevelStorageAccess levelStorageSource, PackRepository packRepository, WorldStem worldStem, Optional<GameRules> gameRules, DedicatedServerSettings settings, DataFixer fixerUpper, Services services, ManagementServer jsonRpcServer, NotificationManager notificationManager) {
        super(serverThread, levelStorageSource, packRepository, worldStem, gameRules, java.net.Proxy.NO_PROXY, fixerUpper, services, null, true, notificationManager);
        this.settings = settings;
        this.properties = settings == null ? null : settings.getProperties();
    }
    public void setAllowFlight(boolean allowed) {
    }
    public DedicatedServerProperties getProperties() {
        DedicatedServerProperties current = properties;
        if (current == null) {
            java.util.Properties values = new java.util.Properties();
            org.bukkit.Server server = org.bukkit.Bukkit.getServer();
            values.setProperty("motd", server.getMotd());
            values.setProperty("server-ip", server.getIp());
            values.setProperty("server-port", Integer.toString(server.getPort()));
            values.setProperty("max-players", Integer.toString(server.getMaxPlayers()));
            values.setProperty("view-distance", Integer.toString(server.getViewDistance()));
            values.setProperty("simulation-distance", Integer.toString(server.getSimulationDistance()));
            values.setProperty("online-mode", Boolean.toString(server.getOnlineMode()));
            values.setProperty("allow-flight", Boolean.toString(server.getAllowFlight()));
            values.setProperty("white-list", Boolean.toString(server.hasWhitelist()));
            values.setProperty("enforce-whitelist", Boolean.toString(server.isWhitelistEnforced()));
            values.setProperty("hardcore", Boolean.toString(server.isHardcore()));
            values.setProperty("gamemode", server.getDefaultGameMode().name().toLowerCase(java.util.Locale.ROOT));
            values.setProperty("spawn-protection", Integer.toString(server.getSpawnRadius()));
            if (!server.getWorlds().isEmpty()) {
                values.setProperty("level-name", server.getWorlds().get(0).getName());
                values.setProperty("difficulty", server.getWorlds().get(0).getDifficulty().name().toLowerCase(java.util.Locale.ROOT));
                values.setProperty("generate-structures", Boolean.toString(server.getWorlds().get(0).canGenerateStructures()));
            }
            current = new DedicatedServerProperties(values);
            properties = current;
        }
        return current;
    }
    public void setDifficulty(Difficulty difficulty) {
        setDifficulty(difficulty, false);
    }
    public int viewDistance() {
        return org.bukkit.Bukkit.getServer().getViewDistance();
    }
    public void setViewDistance(int viewDistance) {
    }
    public int simulationDistance() {
        return org.bukkit.Bukkit.getServer().getSimulationDistance();
    }
    public void setSimulationDistance(int simulationDistance) {
    }
    public void handleConsoleInput(String msg, CommandSourceStack source) {
    }
    public void handleConsoleInputs() {
    }
    public int getMaxPlayers() {
        return org.bukkit.Bukkit.getServer().getMaxPlayers();
    }
    public void setMaxPlayers(int maxPlayers) {
    }
    public String getServerIp() {
        return org.bukkit.Bukkit.getServer().getIp();
    }
    public int getServerPort() {
        return org.bukkit.Bukkit.getServer().getPort();
    }
    public String getServerName() {
        return org.bukkit.Bukkit.getServer().getName();
    }
    public void showGui() {
    }
    public int spawnProtectionRadius() {
        String value = org.basaltmc.bukkit.WorldEnvNatives.setting("spawn_protection");
        return value == null ? 16 : Integer.parseInt(value);
    }
    public void setSpawnProtectionRadius(int spawnProtectionRadius) {
        org.basaltmc.bukkit.WorldEnvNatives.setSetting("spawn_protection", String.valueOf(spawnProtectionRadius));
    }
    public void setRepliesToStatus(boolean enable) {
    }
    public void setHidesOnlinePlayers(boolean hide) {
        org.basaltmc.bukkit.WorldEnvNatives.setSetting("hide_online_players", String.valueOf(hide));
    }
    public void setOperatorUserPermissions(LevelBasedPermissionSet permissions) {
        properties = getProperties().opPermissions.update(registryAccess(), permissions);
    }
    public int statusHeartbeatInterval() {
        return 0;
    }
    public boolean setStatusHeartbeatInterval(int statusHeartbeatInterval) {
        return false;
    }
    protected boolean convertOldUsers() {
        return true;
    }
    public long getMaxTickLength() {
        return 60000L;
    }
    public String getPluginNames() {
        return java.util.Arrays.stream(org.bukkit.Bukkit.getServer().getPluginManager().getPlugins()).map(org.bukkit.plugin.Plugin::getName).collect(java.util.stream.Collectors.joining(", "));
    }
    public String runCommand(String command) {
        org.bukkit.Bukkit.getServer().dispatchCommand(org.bukkit.Bukkit.getServer().getConsoleSender(), command);
        return "";
    }
    public int entityBroadcastRangePercentage() {
        return 100;
    }
    public void setEntityBroadcastRangePercentage(int range) {
    }
    public String getLevelIdName() {
        return "world";
    }
    public GameType gameMode() {
        return GameType.byId(org.bukkit.Bukkit.getDefaultGameMode().getValue());
    }
    public void setGameMode(GameType gameMode) {
        org.basaltmc.bukkit.WorldEnvNatives.setSetting("gamemode", gameMode.getName());
    }
    public void setAcceptsTransfers(boolean acceptTransfers) {
    }
    public void setPauseWhenEmptySeconds(int seconds) {
        org.basaltmc.bukkit.WorldEnvNatives.setSetting("pause_when_empty_seconds", String.valueOf(seconds));
    }
}
