// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.jsonrpc;

import net.minecraft.core.Holder;
import net.minecraft.server.jsonrpc.api.PlayerDto;
import net.minecraft.server.jsonrpc.api.Schema;
import net.minecraft.server.jsonrpc.methods.BanlistService;
import net.minecraft.server.jsonrpc.methods.GameRulesService;
import net.minecraft.server.jsonrpc.methods.IpBanlistService;
import net.minecraft.server.jsonrpc.methods.OperatorService;
import net.minecraft.server.jsonrpc.methods.ServerStateService;

public class OutgoingRpcMethods {
    public static final Holder.Reference<OutgoingRpcMethod<Void, Void>> SERVER_STARTED = event("server/started", "Server started", false);
    public static final Holder.Reference<OutgoingRpcMethod<Void, Void>> SERVER_SHUTTING_DOWN = event("server/stopping", "Server shutting down", false);
    public static final Holder.Reference<OutgoingRpcMethod<Void, Void>> SERVER_SAVE_STARTED = event("server/saving", "Server save started", false);
    public static final Holder.Reference<OutgoingRpcMethod<Void, Void>> SERVER_SAVE_COMPLETED = event("server/saved", "Server save completed", false);
    public static final Holder.Reference<OutgoingRpcMethod<Void, Void>> SERVER_ACTIVITY_OCCURRED = event("server/activity",
        "Server activity occurred. Rate limited to 1 notification per 30 seconds", false);
    public static final Holder.Reference<OutgoingRpcMethod<Void, Void>> WORLD_UPGRADE_STARTED = event("world/upgrade_started", "World upgrade started", true);
    public static final Holder.Reference<OutgoingRpcMethod<Float, Void>> WORLD_UPGRADE_PROGRESS = with("world/upgrade_progress",
        "World upgrade progress. Rate limited to 1 notification per second", true, "progress", Schema.NUMBER_SCHEMA);
    public static final Holder.Reference<OutgoingRpcMethod<Void, Void>> WORLD_UPGRADE_FINISHED = event("world/upgrade_finished", "World upgrade finished", true);
    public static final Holder.Reference<OutgoingRpcMethod<String, Void>> WORLD_UPGRADE_FAILED = with("world/upgrade_failed",
        "World upgrade failed", true, "reason", Schema.STRING_SCHEMA);
    public static final Holder.Reference<OutgoingRpcMethod<PlayerDto, Void>> PLAYER_JOINED = with("players/joined", "Player joined", false,
        "player", Schema.PLAYER_SCHEMA.asRef());
    public static final Holder.Reference<OutgoingRpcMethod<PlayerDto, Void>> PLAYER_LEFT = with("players/left", "Player left", false,
        "player", Schema.PLAYER_SCHEMA.asRef());
    public static final Holder.Reference<OutgoingRpcMethod<OperatorService.OperatorDto, Void>> PLAYER_OPED = with("operators/added",
        "Player was oped", false, "player", Schema.OPERATOR_SCHEMA.asRef());
    public static final Holder.Reference<OutgoingRpcMethod<OperatorService.OperatorDto, Void>> PLAYER_DEOPED = with("operators/removed",
        "Player was deoped", false, "player", Schema.OPERATOR_SCHEMA.asRef());
    public static final Holder.Reference<OutgoingRpcMethod<PlayerDto, Void>> PLAYER_ADDED_TO_ALLOWLIST = with("allowlist/added",
        "Player was added to allowlist", false, "player", Schema.PLAYER_SCHEMA.asRef());
    public static final Holder.Reference<OutgoingRpcMethod<PlayerDto, Void>> PLAYER_REMOVED_FROM_ALLOWLIST = with("allowlist/removed",
        "Player was removed from allowlist", false, "player", Schema.PLAYER_SCHEMA.asRef());
    public static final Holder.Reference<OutgoingRpcMethod<IpBanlistService.IpBanDto, Void>> IP_BANNED = with("ip_bans/added",
        "Ip was added to ip ban list", false, "player", Schema.IP_BAN_SCHEMA.asRef());
    public static final Holder.Reference<OutgoingRpcMethod<String, Void>> IP_UNBANNED = with("ip_bans/removed",
        "Ip was removed from ip ban list", false, "player", Schema.STRING_SCHEMA);
    public static final Holder.Reference<OutgoingRpcMethod<BanlistService.UserBanDto, Void>> PLAYER_BANNED = with("bans/added",
        "Player was added to ban list", false, "player", Schema.PLAYER_BAN_SCHEMA.asRef());
    public static final Holder.Reference<OutgoingRpcMethod<PlayerDto, Void>> PLAYER_UNBANNED = with("bans/removed",
        "Player was removed from ban list", false, "player", Schema.PLAYER_SCHEMA.asRef());
    public static final Holder.Reference<OutgoingRpcMethod<GameRulesService.GameRuleUpdate<?>, Void>> GAMERULE_CHANGED = with("gamerules/updated",
        "Gamerule was changed", false, "gamerule", Schema.TYPED_GAME_RULE_SCHEMA.asRef());
    public static final Holder.Reference<OutgoingRpcMethod<ServerStateService.ServerState, Void>> STATUS_HEARTBEAT = with("server/status",
        "Server status heartbeat, including before the server has spun up", true, "status", Schema.SERVER_STATE_SCHEMA.asRef());

    private static Holder.Reference<OutgoingRpcMethod<Void, Void>> event(String key, String text, boolean preInit) {
        OutgoingRpcMethod.OutgoingRpcMethodBuilder<Void, Void> builder = OutgoingRpcMethod.notification().description(text);
        return (preInit ? builder.allowPreServerInit() : builder).register(key);
    }

    private static <P> Holder.Reference<OutgoingRpcMethod<P, Void>> with(String key, String text, boolean preInit, String paramName, Schema<P> schema) {
        OutgoingRpcMethod.OutgoingRpcMethodBuilder<P, Void> builder = OutgoingRpcMethod.<P>notificationWithParams().param(paramName, schema).description(text);
        return (preInit ? builder.allowPreServerInit() : builder).register(key);
    }
}
