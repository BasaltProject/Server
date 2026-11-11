// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.jsonrpc.internalapi;
import net.minecraft.server.jsonrpc.methods.ServerSettingsService;
import net.minecraft.server.permissions.PermissionLevel;
import net.minecraft.server.jsonrpc.JsonRpcLogger;
import net.minecraft.server.jsonrpc.methods.ClientInfo;
import net.minecraft.server.notifications.NotificationManager;
import net.minecraft.server.permissions.LevelBasedPermissionSet;
import net.minecraft.world.Difficulty;
import net.minecraft.world.level.GameType;
public class MinecraftServerSettingsServiceImpl implements MinecraftServerSettingsService {
    public MinecraftServerSettingsServiceImpl(NotificationManager notificationManager, JsonRpcLogger jsonrpcLogger) {
        
    }
    public boolean isAutoSave() {
        return ServerSettingsService.autosave(null);
    }
    public boolean setAutoSave(boolean enabled, ClientInfo clientInfo) {
        return ServerSettingsService.setAutosave(null, enabled, clientInfo);
    }
    public Difficulty getDifficulty() {
        return ServerSettingsService.difficulty(null);
    }
    public Difficulty setDifficulty(Difficulty difficulty, ClientInfo clientInfo) {
        return ServerSettingsService.setDifficulty(null, difficulty, clientInfo);
    }
    public boolean isEnforceWhitelist() {
        return ServerSettingsService.enforceAllowlist(null);
    }
    public boolean setEnforceWhitelist(boolean enforce, ClientInfo clientInfo) {
        return ServerSettingsService.setEnforceAllowlist(null, enforce, clientInfo);
    }
    public boolean isUsingWhitelist() {
        return ServerSettingsService.usingAllowlist(null);
    }
    public boolean setUsingWhitelist(boolean use, ClientInfo clientInfo) {
        return ServerSettingsService.setUsingAllowlist(null, use, clientInfo);
    }
    public int getMaxPlayers() {
        return ServerSettingsService.maxPlayers(null);
    }
    public int setMaxPlayers(int maxPlayers, ClientInfo clientInfo) {
        return ServerSettingsService.setMaxPlayers(null, maxPlayers, clientInfo);
    }
    public int getPauseWhenEmptySeconds() {
        return ServerSettingsService.pauseWhenEmpty(null);
    }
    public int setPauseWhenEmptySeconds(int emptySeconds, ClientInfo clientInfo) {
        return ServerSettingsService.setPauseWhenEmpty(null, emptySeconds, clientInfo);
    }
    public int getPlayerIdleTimeout() {
        return ServerSettingsService.playerIdleTimeout(null);
    }
    public int setPlayerIdleTimeout(int idleTime, ClientInfo clientInfo) {
        return ServerSettingsService.setPlayerIdleTimeout(null, idleTime, clientInfo);
    }
    public boolean allowFlight() {
        return ServerSettingsService.allowFlight(null);
    }
    public boolean setAllowFlight(boolean allow, ClientInfo clientInfo) {
        return ServerSettingsService.setAllowFlight(null, allow, clientInfo);
    }
    public int getSpawnProtectionRadius() {
        return ServerSettingsService.spawnProtection(null);
    }
    public int setSpawnProtectionRadius(int spawnProtection, ClientInfo clientInfo) {
        return ServerSettingsService.setSpawnProtection(null, spawnProtection, clientInfo);
    }
    public String getMotd() {
        return ServerSettingsService.motd(null);
    }
    public String setMotd(String motd, ClientInfo clientInfo) {
        return ServerSettingsService.setMotd(null, motd, clientInfo);
    }
    public boolean forceGameMode() {
        return ServerSettingsService.forceGameMode(null);
    }
    public boolean setForceGameMode(boolean force, ClientInfo clientInfo) {
        return ServerSettingsService.setForceGameMode(null, force, clientInfo);
    }
    public GameType getGameMode() {
        return ServerSettingsService.gameMode(null);
    }
    public GameType setGameMode(GameType gameMode, ClientInfo clientInfo) {
        return ServerSettingsService.setGameMode(null, gameMode, clientInfo);
    }
    public int getViewDistance() {
        return ServerSettingsService.viewDistance(null);
    }
    public int setViewDistance(int viewDistance, ClientInfo clientInfo) {
        return ServerSettingsService.setViewDistance(null, viewDistance, clientInfo);
    }
    public int getSimulationDistance() {
        return ServerSettingsService.simulationDistance(null);
    }
    public int setSimulationDistance(int simulationDistance, ClientInfo clientInfo) {
        return ServerSettingsService.setSimulationDistance(null, simulationDistance, clientInfo);
    }
    public boolean acceptsTransfers() {
        return ServerSettingsService.acceptTransfers(null);
    }
    public boolean setAcceptsTransfers(boolean accept, ClientInfo clientInfo) {
        return ServerSettingsService.setAcceptTransfers(null, accept, clientInfo);
    }
    public int getStatusHeartbeatInterval() {
        return ServerSettingsService.statusHeartbeatInterval(null);
    }
    public int setStatusHeartbeatInterval(int statusHeartbeatInterval, ClientInfo clientInfo) {
        return ServerSettingsService.setStatusHeartbeatInterval(null, statusHeartbeatInterval, clientInfo);
    }
    public LevelBasedPermissionSet getOperatorUserPermissions() {
        return LevelBasedPermissionSet.forLevel(ServerSettingsService.operatorUserPermissionLevel(null));
    }
    public LevelBasedPermissionSet setOperatorUserPermissions(LevelBasedPermissionSet permissions, ClientInfo clientInfo) {
        return LevelBasedPermissionSet.forLevel(ServerSettingsService.setOperatorUserPermissionLevel(null, permissions.level(), clientInfo));

    }
    public boolean hidesOnlinePlayers() {
        return ServerSettingsService.hidesOnlinePlayers(null);
    }
    public boolean setHidesOnlinePlayers(boolean hide, ClientInfo clientInfo) {
        return ServerSettingsService.setHidesOnlinePlayers(null, hide, clientInfo);
    }
    public boolean repliesToStatus() {
        return ServerSettingsService.repliesToStatus(null);
    }
    public boolean setRepliesToStatus(boolean enable, ClientInfo clientInfo) {
        return ServerSettingsService.setRepliesToStatus(null, enable, clientInfo);
    }
    public int getEntityBroadcastRangePercentage() {
        return ServerSettingsService.entityBroadcastRangePercentage(null);
    }
    public int setEntityBroadcastRangePercentage(int percentage, ClientInfo clientInfo) {
        return ServerSettingsService.setEntityBroadcastRangePercentage(null, percentage, clientInfo);
    }
}
