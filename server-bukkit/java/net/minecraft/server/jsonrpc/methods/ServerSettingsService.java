// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.jsonrpc.methods;
import net.minecraft.server.jsonrpc.internalapi.MinecraftApi;
import net.minecraft.server.permissions.PermissionLevel;
import net.minecraft.world.Difficulty;
import net.minecraft.world.level.GameType;
public class ServerSettingsService {
    private static boolean flag(String key) {
        return Boolean.parseBoolean(org.basaltmc.bukkit.WorldEnvNatives.setting(key));
    }
    private static int number(String key) {
        return Integer.parseInt(org.basaltmc.bukkit.WorldEnvNatives.setting(key));
    }
    private static void put(String key, Object value) {
        org.basaltmc.bukkit.WorldEnvNatives.setSetting(key, String.valueOf(value));
    }

    public static boolean autosave(MinecraftApi minecraftApi) {
        return net.minecraft.server.MinecraftServer.getServer().isAutoSave();
    }
    public static boolean setAutosave(MinecraftApi minecraftApi, boolean enabled, ClientInfo clientInfo) {
        net.minecraft.server.MinecraftServer.getServer().setAutoSave(enabled);
        return net.minecraft.server.MinecraftServer.getServer().isAutoSave();
    }
    public static Difficulty difficulty(MinecraftApi minecraftApi) {
        return Difficulty.byId(org.bukkit.Bukkit.getWorlds().get(0).getDifficulty().getValue());
    }
    public static Difficulty setDifficulty(MinecraftApi minecraftApi, Difficulty difficulty, ClientInfo clientInfo) {
        for (org.bukkit.World world : org.bukkit.Bukkit.getWorlds()) {
            world.setDifficulty(org.bukkit.Difficulty.getByValue(difficulty.getId()));
        }
        return difficulty;
    }
    public static boolean enforceAllowlist(MinecraftApi minecraftApi) {
        return org.bukkit.Bukkit.isWhitelistEnforced();
    }
    public static boolean setEnforceAllowlist(MinecraftApi minecraftApi, boolean enforce, ClientInfo clientInfo) {
        org.bukkit.Bukkit.setWhitelistEnforced(enforce);
        return enforce;
    }
    public static boolean usingAllowlist(MinecraftApi minecraftApi) {
        return org.bukkit.Bukkit.hasWhitelist();
    }
    public static boolean setUsingAllowlist(MinecraftApi minecraftApi, boolean use, ClientInfo clientInfo) {
        org.bukkit.Bukkit.setWhitelist(use);
        return use;
    }
    public static int maxPlayers(MinecraftApi minecraftApi) {
        return org.bukkit.Bukkit.getMaxPlayers();
    }
    public static int setMaxPlayers(MinecraftApi minecraftApi, int maxPlayers, ClientInfo clientInfo) {
        org.bukkit.Bukkit.setMaxPlayers(maxPlayers);
        return maxPlayers;
    }
    public static int pauseWhenEmpty(MinecraftApi minecraftApi) {
        return number("pause_when_empty_seconds");
    }
    public static int setPauseWhenEmpty(MinecraftApi minecraftApi, int emptySeconds, ClientInfo clientInfo) {
        put("pause_when_empty_seconds", emptySeconds);
        return emptySeconds;
    }
    public static int playerIdleTimeout(MinecraftApi minecraftApi) {
        return net.minecraft.server.MinecraftServer.getServer().playerIdleTimeout();
    }
    public static int setPlayerIdleTimeout(MinecraftApi minecraftApi, int idleTime, ClientInfo clientInfo) {
        net.minecraft.server.MinecraftServer.getServer().setPlayerIdleTimeout(idleTime);
        return idleTime;
    }
    public static boolean allowFlight(MinecraftApi minecraftApi) {
        return flag("allow_flight");
    }
    public static boolean setAllowFlight(MinecraftApi minecraftApi, boolean allow, ClientInfo clientInfo) {
        put("allow_flight", allow);
        return allow;
    }
    public static int spawnProtection(MinecraftApi minecraftApi) {
        return org.bukkit.Bukkit.getSpawnRadius();
    }
    public static int setSpawnProtection(MinecraftApi minecraftApi, int spawnProtection, ClientInfo clientInfo) {
        org.bukkit.Bukkit.setSpawnRadius(spawnProtection);
        return spawnProtection;
    }
    public static String motd(MinecraftApi minecraftApi) {
        return net.minecraft.server.MinecraftServer.getServer().getMotd();
    }
    public static String setMotd(MinecraftApi minecraftApi, String motd, ClientInfo clientInfo) {
        net.minecraft.server.MinecraftServer.getServer().setMotd(motd);
        return motd;
    }
    public static boolean forceGameMode(MinecraftApi minecraftApi) {
        return net.minecraft.server.MinecraftServer.getServer().forceGameMode();
    }
    public static boolean setForceGameMode(MinecraftApi minecraftApi, boolean force, ClientInfo clientInfo) {
        net.minecraft.server.MinecraftServer.getServer().setForceGameMode(force);
        return force;
    }
    public static GameType gameMode(MinecraftApi minecraftApi) {
        return net.minecraft.server.MinecraftServer.getServer().getDefaultGameType();
    }
    public static GameType setGameMode(MinecraftApi minecraftApi, GameType gameMode, ClientInfo clientInfo) {
        net.minecraft.server.MinecraftServer.getServer().setDefaultGameType(gameMode);
        return gameMode;
    }
    public static int viewDistance(MinecraftApi minecraftApi) {
        return org.bukkit.Bukkit.getViewDistance();
    }
    public static int setViewDistance(MinecraftApi minecraftApi, int viewDistance, ClientInfo clientInfo) {
        for (org.bukkit.World world : org.bukkit.Bukkit.getWorlds()) {
            world.setViewDistance(viewDistance);
        }
        return viewDistance;
    }
    public static int simulationDistance(MinecraftApi minecraftApi) {
        return org.bukkit.Bukkit.getSimulationDistance();
    }
    public static int setSimulationDistance(MinecraftApi minecraftApi, int simulationDistance, ClientInfo clientInfo) {
        for (org.bukkit.World world : org.bukkit.Bukkit.getWorlds()) {
            world.setSimulationDistance(simulationDistance);
        }
        return simulationDistance;
    }
    public static boolean acceptTransfers(MinecraftApi minecraftApi) {
        return flag("accept_transfers");
    }
    public static boolean setAcceptTransfers(MinecraftApi minecraftApi, boolean accept, ClientInfo clientInfo) {
        put("accept_transfers", accept);
        return accept;
    }
    public static int statusHeartbeatInterval(MinecraftApi minecraftApi) {
        return number("status_heartbeat_interval");
    }
    public static int setStatusHeartbeatInterval(MinecraftApi minecraftApi, int statusHeartbeatInterval, ClientInfo clientInfo) {
        put("status_heartbeat_interval", statusHeartbeatInterval);
        return number("status_heartbeat_interval");
    }
    public static PermissionLevel operatorUserPermissionLevel(MinecraftApi minecraftApi) {
        return PermissionLevel.byId(number("operator_user_permission_level"));
    }
    public static PermissionLevel setOperatorUserPermissionLevel(MinecraftApi minecraftApi, PermissionLevel level, ClientInfo clientInfo) {
        put("operator_user_permission_level", level.id());
        return level;
    }
    public static boolean hidesOnlinePlayers(MinecraftApi minecraftApi) {
        return flag("hide_online_players");
    }
    public static boolean setHidesOnlinePlayers(MinecraftApi minecraftApi, boolean hide, ClientInfo clientInfo) {
        put("hide_online_players", hide);
        return hide;
    }
    public static boolean repliesToStatus(MinecraftApi minecraftApi) {
        return flag("status_replies");
    }
    public static boolean setRepliesToStatus(MinecraftApi minecraftApi, boolean enable, ClientInfo clientInfo) {
        put("status_replies", enable);
        return enable;
    }
    public static int entityBroadcastRangePercentage(MinecraftApi minecraftApi) {
        return number("entity_broadcast_range");
    }
    public static int setEntityBroadcastRangePercentage(MinecraftApi minecraftApi, int percentage, ClientInfo clientInfo) {
        put("entity_broadcast_range", percentage);
        return percentage;
    }
}
