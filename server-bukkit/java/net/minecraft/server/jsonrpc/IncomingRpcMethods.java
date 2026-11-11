// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.jsonrpc;

import net.minecraft.core.Registry;
import net.minecraft.server.jsonrpc.api.Schema;
import net.minecraft.server.jsonrpc.methods.AllowlistService;
import net.minecraft.server.jsonrpc.methods.BanlistService;
import net.minecraft.server.jsonrpc.methods.DiscoveryService;
import net.minecraft.server.jsonrpc.methods.GameRulesService;
import net.minecraft.server.jsonrpc.methods.IpBanlistService;
import net.minecraft.server.jsonrpc.methods.OperatorService;
import net.minecraft.server.jsonrpc.methods.PlayerService;
import net.minecraft.server.jsonrpc.methods.ServerSettingsService;
import net.minecraft.server.jsonrpc.methods.ServerStateService;

public class IncomingRpcMethods {
    private interface Table {
        void fill(Registry<IncomingRpcMethod<?, ?>> target);
    }

    private static final Table[] TABLES = {IncomingRpcMethods::allowlist, IncomingRpcMethods::bans, IncomingRpcMethods::ipBans,
        IncomingRpcMethods::players, IncomingRpcMethods::operators, IncomingRpcMethods::serverState, IncomingRpcMethods::serverSettings,
        IncomingRpcMethods::gameRules};

    private static boolean registered;

    /** The facade's registries start empty, so the management methods are registered the first time they are needed. */
    public static synchronized void ensureRegistered() {
        if (!registered) {
            registered = true;
            bootstrap(net.minecraft.core.registries.BuiltInRegistries.INCOMING_RPC_METHOD);
            OutgoingRpcMethods.SERVER_STARTED.key();
        }
    }

    public static IncomingRpcMethod<?, ?> bootstrap(Registry<IncomingRpcMethod<?, ?>> methodRegistry) {
        for (Table table : TABLES) {
            table.fill(methodRegistry);
        }
        return IncomingRpcMethod.<DiscoveryService.DiscoverResponse>method(api -> DiscoveryService.discover(Schema.getSchemaRegistry()))
            .undiscoverable().allowPreServerInit().notOnMainThread().response("result", Schema.DISCOVERY_SCHEMA)
            .register(methodRegistry, "rpc.discover");
    }

    private static <R> void read(Registry<IncomingRpcMethod<?, ?>> target, String key, String text,
        java.util.function.Function<net.minecraft.server.jsonrpc.internalapi.MinecraftApi, R> fn, String resultName, Schema<R> result) {
        IncomingRpcMethod.method(fn).description(text).response(resultName, result).register(target, key);
    }

    private static <R> void act(Registry<IncomingRpcMethod<?, ?>> target, String key, String text,
        IncomingRpcMethod.ParameterlessRpcMethodFunction<R> fn, String resultName, Schema<R> result) {
        IncomingRpcMethod.method(fn).description(text).response(resultName, result).register(target, key);
    }

    private static <P, R> void call(Registry<IncomingRpcMethod<?, ?>> target, String key, String text,
        IncomingRpcMethod.RpcMethodFunction<P, R> fn, String paramName, Schema<P> param, String resultName, Schema<R> result) {
        IncomingRpcMethod.method(fn).description(text).param(paramName, param).response(resultName, result).register(target, key);
    }


    private static void allowlist(Registry<IncomingRpcMethod<?, ?>> target) {
        read(target, "allowlist", "Get the allowlist",
            AllowlistService::get, "allowlist", Schema.PLAYER_SCHEMA.asArray());
        call(target, "allowlist/set", "Set the allowlist",
            AllowlistService::set, "players", Schema.PLAYER_SCHEMA.asArray(), "allowlist", Schema.PLAYER_SCHEMA.asArray());
        call(target, "allowlist/add", "Add players to allowlist",
            AllowlistService::add, "add", Schema.PLAYER_SCHEMA.asArray(), "allowlist", Schema.PLAYER_SCHEMA.asArray());
        call(target, "allowlist/remove", "Remove players from allowlist",
            AllowlistService::remove, "remove", Schema.PLAYER_SCHEMA.asArray(), "allowlist", Schema.PLAYER_SCHEMA.asArray());
        act(target, "allowlist/clear", "Clear all players in allowlist",
            AllowlistService::clear, "allowlist", Schema.PLAYER_SCHEMA.asArray());
    }

    private static void bans(Registry<IncomingRpcMethod<?, ?>> target) {
        read(target, "bans", "Get the ban list",
            BanlistService::get, "banlist", Schema.PLAYER_BAN_SCHEMA.asArray());
        call(target, "bans/set", "Set the banlist",
            BanlistService::set, "bans", Schema.PLAYER_BAN_SCHEMA.asArray(), "banlist", Schema.PLAYER_BAN_SCHEMA.asArray());
        call(target, "bans/add", "Add players to ban list",
            BanlistService::add, "add", Schema.PLAYER_BAN_SCHEMA.asArray(), "banlist", Schema.PLAYER_BAN_SCHEMA.asArray());
        call(target, "bans/remove", "Remove players from ban list",
            BanlistService::remove, "remove", Schema.PLAYER_SCHEMA.asArray(), "banlist", Schema.PLAYER_BAN_SCHEMA.asArray());
        act(target, "bans/clear", "Clear all players in ban list",
            BanlistService::clear, "banlist", Schema.PLAYER_BAN_SCHEMA.asArray());
    }

    private static void ipBans(Registry<IncomingRpcMethod<?, ?>> target) {
        read(target, "ip_bans", "Get the ip ban list",
            IpBanlistService::get, "banlist", Schema.IP_BAN_SCHEMA.asArray());
        call(target, "ip_bans/set", "Set the ip banlist",
            IpBanlistService::set, "banlist", Schema.IP_BAN_SCHEMA.asArray(), "banlist", Schema.IP_BAN_SCHEMA.asArray());
        call(target, "ip_bans/add", "Add ip to ban list",
            IpBanlistService::add, "add", Schema.INCOMING_IP_BAN_SCHEMA.asArray(), "banlist", Schema.IP_BAN_SCHEMA.asArray());
        call(target, "ip_bans/remove", "Remove ip from ban list",
            IpBanlistService::remove, "ip", Schema.STRING_SCHEMA.asArray(), "banlist", Schema.IP_BAN_SCHEMA.asArray());
        act(target, "ip_bans/clear", "Clear all ips in ban list",
            IpBanlistService::clear, "banlist", Schema.IP_BAN_SCHEMA.asArray());
    }

    private static void players(Registry<IncomingRpcMethod<?, ?>> target) {
        read(target, "players", "Get all connected players",
            PlayerService::get, "players", Schema.PLAYER_SCHEMA.asArray());
        call(target, "players/kick", "Kick players",
            PlayerService::kick, "kick", Schema.KICK_PLAYER_SCHEMA.asArray(), "kicked", Schema.PLAYER_SCHEMA.asArray());
    }

    private static void operators(Registry<IncomingRpcMethod<?, ?>> target) {
        read(target, "operators", "Get all oped players",
            OperatorService::get, "operators", Schema.OPERATOR_SCHEMA.asArray());
        call(target, "operators/set", "Set all oped players",
            OperatorService::set, "operators", Schema.OPERATOR_SCHEMA.asArray(), "operators", Schema.OPERATOR_SCHEMA.asArray());
        call(target, "operators/add", "Op players",
            OperatorService::add, "add", Schema.OPERATOR_SCHEMA.asArray(), "operators", Schema.OPERATOR_SCHEMA.asArray());
        call(target, "operators/remove", "Deop players",
            OperatorService::remove, "remove", Schema.PLAYER_SCHEMA.asArray(), "operators", Schema.OPERATOR_SCHEMA.asArray());
        act(target, "operators/clear", "Deop all players",
            OperatorService::clear, "operators", Schema.OPERATOR_SCHEMA.asArray());
    }

    private static void serverState(Registry<IncomingRpcMethod<?, ?>> target) {
        read(target, "server/status", "Get server status",
            ServerStateService::status, "status", Schema.SERVER_STATE_SCHEMA.asRef());
        call(target, "server/save", "Save server state",
            ServerStateService::save, "flush", Schema.BOOL_SCHEMA, "saving", Schema.BOOL_SCHEMA);
        act(target, "server/stop", "Stop server",
            ServerStateService::stop, "stopping", Schema.BOOL_SCHEMA);
        call(target, "server/system_message", "Send a system message",
            ServerStateService::systemMessage, "message", Schema.SYSTEM_MESSAGE_SCHEMA.asRef(), "sent", Schema.BOOL_SCHEMA);
    }

    private static void serverSettings(Registry<IncomingRpcMethod<?, ?>> target) {
        read(target, "serversettings/autosave", "Get whether automatic world saving is enabled on the server",
            ServerSettingsService::autosave, "enabled", Schema.BOOL_SCHEMA);
        call(target, "serversettings/autosave/set", "Enable or disable automatic world saving on the server",
            ServerSettingsService::setAutosave, "enable", Schema.BOOL_SCHEMA, "enabled", Schema.BOOL_SCHEMA);
        read(target, "serversettings/difficulty", "Get the current difficulty level of the server",
            ServerSettingsService::difficulty, "difficulty", Schema.DIFFICULTY_SCHEMA.asRef());
        call(target, "serversettings/difficulty/set", "Set the difficulty level of the server",
            ServerSettingsService::setDifficulty, "difficulty", Schema.DIFFICULTY_SCHEMA.asRef(), "difficulty", Schema.DIFFICULTY_SCHEMA.asRef());
        read(target, "serversettings/enforce_allowlist", "Get whether allowlist enforcement is enabled (kicks players immediately when removed from allowlist)",
            ServerSettingsService::enforceAllowlist, "enforced", Schema.BOOL_SCHEMA);
        call(target, "serversettings/enforce_allowlist/set", "Enable or disable allowlist enforcement (when enabled, players are kicked immediately upon removal from allowlist)",
            ServerSettingsService::setEnforceAllowlist, "enforce", Schema.BOOL_SCHEMA, "enforced", Schema.BOOL_SCHEMA);
        read(target, "serversettings/use_allowlist", "Get whether the allowlist is enabled on the server",
            ServerSettingsService::usingAllowlist, "used", Schema.BOOL_SCHEMA);
        call(target, "serversettings/use_allowlist/set", "Enable or disable the allowlist on the server (controls whether only allowlisted players can join)",
            ServerSettingsService::setUsingAllowlist, "use", Schema.BOOL_SCHEMA, "used", Schema.BOOL_SCHEMA);
        read(target, "serversettings/max_players", "Get the maximum number of players allowed to connect to the server",
            ServerSettingsService::maxPlayers, "max", Schema.INT_SCHEMA);
        call(target, "serversettings/max_players/set", "Set the maximum number of players allowed to connect to the server",
            ServerSettingsService::setMaxPlayers, "max", Schema.INT_SCHEMA, "max", Schema.INT_SCHEMA);
        read(target, "serversettings/pause_when_empty_seconds", "Get the number of seconds before the game is automatically paused when no players are online",
            ServerSettingsService::pauseWhenEmpty, "seconds", Schema.INT_SCHEMA);
        call(target, "serversettings/pause_when_empty_seconds/set", "Set the number of seconds before the game is automatically paused when no players are online",
            ServerSettingsService::setPauseWhenEmpty, "seconds", Schema.INT_SCHEMA, "seconds", Schema.INT_SCHEMA);
        read(target, "serversettings/player_idle_timeout", "Get the number of seconds before idle players are automatically kicked from the server",
            ServerSettingsService::playerIdleTimeout, "seconds", Schema.INT_SCHEMA);
        call(target, "serversettings/player_idle_timeout/set", "Set the number of seconds before idle players are automatically kicked from the server",
            ServerSettingsService::setPlayerIdleTimeout, "seconds", Schema.INT_SCHEMA, "seconds", Schema.INT_SCHEMA);
        read(target, "serversettings/allow_flight", "Get whether flight is allowed for players in Survival mode",
            ServerSettingsService::allowFlight, "allowed", Schema.BOOL_SCHEMA);
        call(target, "serversettings/allow_flight/set", "Allow or disallow flight for players in Survival mode",
            ServerSettingsService::setAllowFlight, "allow", Schema.BOOL_SCHEMA, "allowed", Schema.BOOL_SCHEMA);
        read(target, "serversettings/motd", "Get the server's message of the day displayed to players",
            ServerSettingsService::motd, "message", Schema.STRING_SCHEMA);
        call(target, "serversettings/motd/set", "Set the server's message of the day displayed to players",
            ServerSettingsService::setMotd, "message", Schema.STRING_SCHEMA, "message", Schema.STRING_SCHEMA);
        read(target, "serversettings/spawn_protection_radius", "Get the spawn protection radius in blocks (only operators can edit within this area)",
            ServerSettingsService::spawnProtection, "radius", Schema.INT_SCHEMA);
        call(target, "serversettings/spawn_protection_radius/set", "Set the spawn protection radius in blocks (only operators can edit within this area)",
            ServerSettingsService::setSpawnProtection, "radius", Schema.INT_SCHEMA, "radius", Schema.INT_SCHEMA);
        read(target, "serversettings/force_game_mode", "Get whether players are forced to use the server's default game mode",
            ServerSettingsService::forceGameMode, "forced", Schema.BOOL_SCHEMA);
        call(target, "serversettings/force_game_mode/set", "Enable or disable forcing players to use the server's default game mode",
            ServerSettingsService::setForceGameMode, "force", Schema.BOOL_SCHEMA, "forced", Schema.BOOL_SCHEMA);
        read(target, "serversettings/game_mode", "Get the server's default game mode",
            ServerSettingsService::gameMode, "mode", Schema.GAME_TYPE_SCHEMA.asRef());
        call(target, "serversettings/game_mode/set", "Set the server's default game mode",
            ServerSettingsService::setGameMode, "mode", Schema.GAME_TYPE_SCHEMA.asRef(), "mode", Schema.GAME_TYPE_SCHEMA.asRef());
        read(target, "serversettings/view_distance", "Get the server's view distance in chunks",
            ServerSettingsService::viewDistance, "distance", Schema.INT_SCHEMA);
        call(target, "serversettings/view_distance/set", "Set the server's view distance in chunks",
            ServerSettingsService::setViewDistance, "distance", Schema.INT_SCHEMA, "distance", Schema.INT_SCHEMA);
        read(target, "serversettings/simulation_distance", "Get the server's simulation distance in chunks",
            ServerSettingsService::simulationDistance, "distance", Schema.INT_SCHEMA);
        call(target, "serversettings/simulation_distance/set", "Set the server's simulation distance in chunks",
            ServerSettingsService::setSimulationDistance, "distance", Schema.INT_SCHEMA, "distance", Schema.INT_SCHEMA);
        read(target, "serversettings/accept_transfers", "Get whether the server accepts player transfers from other servers",
            ServerSettingsService::acceptTransfers, "accepted", Schema.BOOL_SCHEMA);
        call(target, "serversettings/accept_transfers/set", "Enable or disable accepting player transfers from other servers",
            ServerSettingsService::setAcceptTransfers, "accept", Schema.BOOL_SCHEMA, "accepted", Schema.BOOL_SCHEMA);
        read(target, "serversettings/status_heartbeat_interval", "Get the interval in seconds between server status heartbeats",
            ServerSettingsService::statusHeartbeatInterval, "seconds", Schema.INT_SCHEMA);
        call(target, "serversettings/status_heartbeat_interval/set", "Set the interval in seconds between server status heartbeats",
            ServerSettingsService::setStatusHeartbeatInterval, "seconds", Schema.INT_SCHEMA, "seconds", Schema.INT_SCHEMA);
        read(target, "serversettings/operator_user_permission_level", "Get default operator permission level",
            ServerSettingsService::operatorUserPermissionLevel, "level", Schema.PERMISSION_LEVEL_SCHEMA);
        call(target, "serversettings/operator_user_permission_level/set", "Set default operator permission level",
            ServerSettingsService::setOperatorUserPermissionLevel, "level", Schema.PERMISSION_LEVEL_SCHEMA, "level", Schema.PERMISSION_LEVEL_SCHEMA);
        read(target, "serversettings/hide_online_players", "Get whether the server hides online player information from status queries",
            ServerSettingsService::hidesOnlinePlayers, "hidden", Schema.BOOL_SCHEMA);
        call(target, "serversettings/hide_online_players/set", "Enable or disable hiding online player information from status queries",
            ServerSettingsService::setHidesOnlinePlayers, "hide", Schema.BOOL_SCHEMA, "hidden", Schema.BOOL_SCHEMA);
        read(target, "serversettings/status_replies", "Get whether the server responds to connection status requests",
            ServerSettingsService::repliesToStatus, "enabled", Schema.BOOL_SCHEMA);
        call(target, "serversettings/status_replies/set", "Enable or disable the server responding to connection status requests",
            ServerSettingsService::setRepliesToStatus, "enable", Schema.BOOL_SCHEMA, "enabled", Schema.BOOL_SCHEMA);
        read(target, "serversettings/entity_broadcast_range", "Get the entity broadcast range as a percentage",
            ServerSettingsService::entityBroadcastRangePercentage, "percentage_points", Schema.INT_SCHEMA);
        call(target, "serversettings/entity_broadcast_range/set", "Set the entity broadcast range as a percentage",
            ServerSettingsService::setEntityBroadcastRangePercentage, "percentage_points", Schema.INT_SCHEMA, "percentage_points", Schema.INT_SCHEMA);
    }

    private static void gameRules(Registry<IncomingRpcMethod<?, ?>> target) {
        read(target, "gamerules", "Get the available game rule keys and their current values",
            GameRulesService::get, "gamerules", Schema.TYPED_GAME_RULE_SCHEMA.asRef().asArray());
        call(target, "gamerules/update", "Update game rule value",
            GameRulesService::update, "gamerule", Schema.UNTYPED_GAME_RULE_SCHEMA.asRef(), "gamerule", Schema.TYPED_GAME_RULE_SCHEMA.asRef());
    }
}
