// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.jsonrpc;

import net.minecraft.core.Holder;
import net.minecraft.server.jsonrpc.api.PlayerDto;
import net.minecraft.server.jsonrpc.internalapi.MinecraftApi;
import net.minecraft.server.jsonrpc.methods.BanlistService;
import net.minecraft.server.jsonrpc.methods.GameRulesService;
import net.minecraft.server.jsonrpc.methods.IpBanlistService;
import net.minecraft.server.jsonrpc.methods.OperatorService;
import net.minecraft.server.jsonrpc.methods.ServerStateService;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.server.notifications.NotificationService;
import net.minecraft.server.players.IpBanListEntry;
import net.minecraft.server.players.NameAndId;
import net.minecraft.server.players.ServerOpListEntry;
import net.minecraft.server.players.UserBanListEntry;
import net.minecraft.world.level.gamerules.GameRule;

public class JsonRpcNotificationService implements NotificationService {
    private final MinecraftApi api;
    private final ManagementServer server;

    public JsonRpcNotificationService(MinecraftApi minecraftApi, ManagementServer managementServer) {
        api = minecraftApi;
        server = managementServer;
    }

    private void announce(Holder.Reference<? extends OutgoingRpcMethod<Void, ?>> method) {
        server.forEachConnection(connection -> connection.sendNotification(method));
    }

    private <P> void announce(Holder.Reference<? extends OutgoingRpcMethod<P, ?>> method, P payload) {
        server.forEachConnection(connection -> connection.sendNotification(method, payload));
    }

    public void playerJoined(ServerPlayer player) {
        announce(OutgoingRpcMethods.PLAYER_JOINED, PlayerDto.from(player));
    }

    public void playerLeft(ServerPlayer player) {
        announce(OutgoingRpcMethods.PLAYER_LEFT, PlayerDto.from(player));
    }

    public void serverStarted() {
        announce(OutgoingRpcMethods.SERVER_STARTED);
    }

    public void serverShuttingDown() {
        announce(OutgoingRpcMethods.SERVER_SHUTTING_DOWN);
    }

    public void serverSaveStarted() {
        announce(OutgoingRpcMethods.SERVER_SAVE_STARTED);
    }

    public void serverSaveCompleted() {
        announce(OutgoingRpcMethods.SERVER_SAVE_COMPLETED);
    }

    public void serverActivityOccured() {
        announce(OutgoingRpcMethods.SERVER_ACTIVITY_OCCURRED);
    }

    public void worldUpgradeStarted() {
        announce(OutgoingRpcMethods.WORLD_UPGRADE_STARTED);
    }

    public void worldUpgradeProgress(float progressPercentage) {
        announce(OutgoingRpcMethods.WORLD_UPGRADE_PROGRESS, progressPercentage);
    }

    public void worldUpgradeFinished() {
        announce(OutgoingRpcMethods.WORLD_UPGRADE_FINISHED);
    }

    public void worldUpgradeFailed(String reason) {
        announce(OutgoingRpcMethods.WORLD_UPGRADE_FAILED, reason);
    }

    public void playerOped(ServerOpListEntry operator) {
        announce(OutgoingRpcMethods.PLAYER_OPED, OperatorService.OperatorDto.from(operator));
    }

    public void playerDeoped(ServerOpListEntry operator) {
        announce(OutgoingRpcMethods.PLAYER_DEOPED, OperatorService.OperatorDto.from(operator));
    }

    public void playerAddedToAllowlist(NameAndId player) {
        announce(OutgoingRpcMethods.PLAYER_ADDED_TO_ALLOWLIST, PlayerDto.from(player));
    }

    public void playerRemovedFromAllowlist(NameAndId player) {
        announce(OutgoingRpcMethods.PLAYER_REMOVED_FROM_ALLOWLIST, PlayerDto.from(player));
    }

    public void ipBanned(IpBanListEntry ban) {
        announce(OutgoingRpcMethods.IP_BANNED, IpBanlistService.IpBanDto.from(ban));
    }

    public void ipUnbanned(String ip) {
        announce(OutgoingRpcMethods.IP_UNBANNED, ip);
    }

    public void playerBanned(UserBanListEntry ban) {
        announce(OutgoingRpcMethods.PLAYER_BANNED, BanlistService.UserBanDto.from(ban));
    }

    public void playerUnbanned(NameAndId player) {
        announce(OutgoingRpcMethods.PLAYER_UNBANNED, PlayerDto.from(player));
    }

    public <T> void onGameRuleChanged(GameRule<T> gameRule, T value) {
        announce(OutgoingRpcMethods.GAMERULE_CHANGED, GameRulesService.getTypedRule(api, gameRule, value));
    }

    public void statusHeartbeat() {
        announce(OutgoingRpcMethods.STATUS_HEARTBEAT, ServerStateService.status(api));
    }
}
