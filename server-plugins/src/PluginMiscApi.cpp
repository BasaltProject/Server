#include "PluginMiscShared.hpp"

namespace basalt::server::plugins::pluginApi {
namespace {
using namespace misc;

void installScoreboards(LuaArgs& lua, LoadedPlugin& plugin, PluginHost& host) {
    LoadedPlugin* target = &plugin;
    PluginHost* server = &host;
    const auto boards = [server]() -> PluginScoreboards& { return server->services().scoreboards(); };

    lua.pushTable();
    wire::lua::pushHostFunction(lua, "scoreboards.main", [](LuaArgs& args) {
        args.pushInteger(PluginScoreboards::kMain);
        return 1;
    });
    lua.setField("main");
    wire::lua::pushHostFunction(lua, "scoreboards.create", [boards](LuaArgs& args) {
        args.pushInteger(boards().create());
        return 1;
    });
    lua.setField("create");
    wire::lua::pushHostFunction(lua, "scoreboards.release", [boards](LuaArgs& args) {
        boards().release(static_cast<std::int32_t>(args.checkInteger(1)));
        return 0;
    });
    lua.setField("release");
    wire::lua::pushHostFunction(lua, "scoreboards.objectives", [boards](LuaArgs& args) {
        std::vector<std::string> names;
        for (const PluginObjective& objective : boards().objectives(static_cast<std::int32_t>(args.checkInteger(1)))) {
            names.push_back(objective.name);
        }
        pushNames(args, names);
        return 1;
    });
    lua.setField("objectives");
    wire::lua::pushHostFunction(lua, "scoreboards.objective_add", [boards, target](LuaArgs& args) {
        PluginObjective objective;
        const auto board = static_cast<std::int32_t>(args.checkInteger(1));
        objective.name = args.checkString(2);
        objective.criterion = args.stringOrNull(3).value_or("dummy");
        objective.displayName = args.isString(4) ? textAt(*target, args, 4) : "\"" + objective.name + "\"";
        objective.hearts = args.booleanOr(5, false);
        args.pushBoolean(boards().addObjective(board, objective));
        return 1;
    });
    lua.setField("objective_add");
    wire::lua::pushHostFunction(lua, "scoreboards.objective_remove", [boards](LuaArgs& args) {
        args.pushBoolean(
            boards().removeObjective(static_cast<std::int32_t>(args.checkInteger(1)), args.checkString(2)));
        return 1;
    });
    lua.setField("objective_remove");
    wire::lua::pushHostFunction(lua, "scoreboards.display", [boards](LuaArgs& args) {
        const auto board = static_cast<std::int32_t>(args.checkInteger(1));
        const std::int32_t slot = slotOf(args, 2);
        if (args.isNil(3)) {
            const std::optional<std::string> shown = boards().displayed(board, slot);
            if (shown.has_value()) {
                args.pushString(*shown);
            } else {
                args.pushNil();
            }
            return 1;
        }
        boards().display(board, slot, args.isBoolean(3) ? std::nullopt : std::optional{args.checkString(3)});
        return 0;
    });
    lua.setField("display");
    wire::lua::pushHostFunction(lua, "scoreboards.score", [boards](LuaArgs& args) {
        const std::optional<std::int32_t> value =
            boards().score(static_cast<std::int32_t>(args.checkInteger(1)), args.checkString(2), args.checkString(3));
        if (value.has_value()) {
            args.pushInteger(*value);
        } else {
            args.pushNil();
        }
        return 1;
    });
    lua.setField("score");
    wire::lua::pushHostFunction(lua, "scoreboards.set_score", [boards](LuaArgs& args) {
        args.pushBoolean(boards().setScore(static_cast<std::int32_t>(args.checkInteger(1)), args.checkString(2),
                                           args.checkString(3), static_cast<std::int32_t>(args.checkInteger(4))));
        return 1;
    });
    lua.setField("set_score");
    wire::lua::pushHostFunction(lua, "scoreboards.reset_score", [boards](LuaArgs& args) {
        boards().resetScore(static_cast<std::int32_t>(args.checkInteger(1)), args.checkString(2), args.stringOrNull(3));
        return 0;
    });
    lua.setField("reset_score");
    wire::lua::pushHostFunction(lua, "scoreboards.entries", [boards](LuaArgs& args) {
        pushNames(args, boards().entries(static_cast<std::int32_t>(args.checkInteger(1))));
        return 1;
    });
    lua.setField("entries");
    wire::lua::pushHostFunction(lua, "scoreboards.teams", [boards](LuaArgs& args) {
        std::vector<std::string> names;
        for (const PluginTeam& team : boards().teams(static_cast<std::int32_t>(args.checkInteger(1)))) {
            names.push_back(team.name);
        }
        pushNames(args, names);
        return 1;
    });
    lua.setField("teams");
    wire::lua::pushHostFunction(lua, "scoreboards.team_add", [boards](LuaArgs& args) {
        args.pushBoolean(boards().addTeam(static_cast<std::int32_t>(args.checkInteger(1)), args.checkString(2)));
        return 1;
    });
    lua.setField("team_add");
    wire::lua::pushHostFunction(lua, "scoreboards.team_remove", [boards](LuaArgs& args) {
        args.pushBoolean(boards().removeTeam(static_cast<std::int32_t>(args.checkInteger(1)), args.checkString(2)));
        return 1;
    });
    lua.setField("team_remove");
    wire::lua::pushHostFunction(lua, "scoreboards.team_set", [boards, target](LuaArgs& args) {
        const auto board = static_cast<std::int32_t>(args.checkInteger(1));
        std::optional<PluginTeam> team = boards().team(board, args.checkString(2));
        if (!team.has_value() || !args.isTable(3)) {
            args.pushBoolean(false);
            return 1;
        }
        const auto text = [&](std::string_view key, std::string& into) {
            args.pushField(3, key);
            if (args.isString(-1)) {
                into = "mini:" + target->text.resolve(args.toText(-1), &args, 0);
            }
            args.pop(1);
        };
        text("display_name", team->displayName);
        text("prefix", team->prefix);
        text("suffix", team->suffix);
        if (const auto color = stringField(args, 3, "color"); color.has_value()) {
            team->color = *color == "none" ? std::string{} : *color;
        }
        if (const auto value = flagField(args, 3, "friendly_fire"); value.has_value()) {
            team->friendlyFire = *value;
        }
        if (const auto value = flagField(args, 3, "see_invisible"); value.has_value()) {
            team->seeFriendlyInvisibles = *value;
        }
        if (const auto value = stringField(args, 3, "name_tags"); value.has_value()) {
            team->nameTagVisibility = *value;
        }
        if (const auto value = stringField(args, 3, "death_messages"); value.has_value()) {
            team->deathMessageVisibility = *value;
        }
        if (const auto value = stringField(args, 3, "collision"); value.has_value()) {
            team->collisionRule = *value;
        }
        args.pushBoolean(boards().updateTeam(board, *team));
        return 1;
    });
    lua.setField("team_set");
    wire::lua::pushHostFunction(lua, "scoreboards.team_join", [boards](LuaArgs& args) {
        args.pushBoolean(boards().addMember(static_cast<std::int32_t>(args.checkInteger(1)), args.checkString(2),
                                            args.checkString(3)));
        return 1;
    });
    lua.setField("team_join");
    wire::lua::pushHostFunction(lua, "scoreboards.team_leave", [boards](LuaArgs& args) {
        args.pushBoolean(boards().removeMember(static_cast<std::int32_t>(args.checkInteger(1)), args.checkString(2),
                                               args.checkString(3)));
        return 1;
    });
    lua.setField("team_leave");
    wire::lua::pushHostFunction(lua, "scoreboards.team_of", [boards](LuaArgs& args) {
        const std::optional<PluginTeam> team =
            boards().teamOf(static_cast<std::int32_t>(args.checkInteger(1)), args.checkString(2));
        if (team.has_value()) {
            args.pushString(team->name);
        } else {
            args.pushNil();
        }
        return 1;
    });
    lua.setField("team_of");
    wire::lua::pushHostFunction(lua, "scoreboards.show", [server, boards](LuaArgs& args) {
        const std::shared_ptr<PluginPlayerRef> player = playerFrom(args, 1, server->services().players());
        if (player == nullptr) {
            args.raise("scoreboards.show(player, board) needs an online player");
        }
        if (args.isNil(2)) {
            args.pushInteger(boards().shown(player->uuid()));
            return 1;
        }
        boards().show(player->uuid(), static_cast<std::int32_t>(args.checkInteger(2)));
        return 0;
    });
    lua.setField("show");
    lua.setGlobal("scoreboards");
}

void installBossBars(LuaArgs& lua, LoadedPlugin& plugin, PluginHost& host) {
    LoadedPlugin* target = &plugin;
    PluginHost* server = &host;
    const auto bars = [server]() -> PluginBossBars& { return server->services().bossBars(); };
    const auto fill = [target](LuaArgs& args, std::int32_t index, PluginBossBar& bar) {
        if (const auto title = stringField(args, index, "title"); title.has_value()) {
            bar.title = "mini:" + target->text.resolve(*title, &args, 0);
        }
        if (const auto color = stringField(args, index, "color"); color.has_value()) {
            bar.color = *color;
        }
        if (const auto style = stringField(args, index, "style"); style.has_value()) {
            bar.style = *style;
        }
        if (const auto value = numberField(args, index, "progress"); value.has_value()) {
            bar.progress = *value;
        }
        if (const auto value = flagField(args, index, "darken_sky"); value.has_value()) {
            bar.darkenSky = *value;
        }
        if (const auto value = flagField(args, index, "boss_music"); value.has_value()) {
            bar.bossMusic = *value;
        }
        if (const auto value = flagField(args, index, "fog"); value.has_value()) {
            bar.fog = *value;
        }
        if (const auto value = flagField(args, index, "visible"); value.has_value()) {
            bar.visible = *value;
        }
    };

    lua.pushTable();
    wire::lua::pushHostFunction(lua, "boss_bars.create", [bars, fill](LuaArgs& args) {
        if (!args.isTable(1)) {
            args.raise("boss_bars.create{ title = ..., color = ..., key = ... } takes a table");
        }
        PluginBossBar bar;
        bar.title = "\"\"";
        fill(args, 1, bar);
        if (const auto key = stringField(args, 1, "key"); key.has_value()) {
            bar.id = *key;
            bar.keyed = true;
        }
        const std::optional<std::string> made = bars().create(bar);
        if (made.has_value()) {
            args.pushString(*made);
        } else {
            args.pushNil();
        }
        return 1;
    });
    lua.setField("create");
    wire::lua::pushHostFunction(lua, "boss_bars.update", [bars, fill](LuaArgs& args) {
        const std::string id = args.checkString(1);
        std::optional<PluginBossBar> bar = bars().get(id);
        if (!bar.has_value() || !args.isTable(2)) {
            args.pushBoolean(false);
            return 1;
        }
        fill(args, 2, *bar);
        args.pushBoolean(bars().update(*bar));
        return 1;
    });
    lua.setField("update");
    wire::lua::pushHostFunction(lua, "boss_bars.remove", [bars](LuaArgs& args) {
        args.pushBoolean(bars().remove(args.checkString(1)));
        return 1;
    });
    lua.setField("remove");
    wire::lua::pushHostFunction(lua, "boss_bars.add_player", [bars, server](LuaArgs& args) {
        const std::shared_ptr<PluginPlayerRef> player = playerFrom(args, 2, server->services().players());
        args.pushBoolean(player != nullptr && bars().addPlayer(args.checkString(1), player->uuid()));
        return 1;
    });
    lua.setField("add_player");
    wire::lua::pushHostFunction(lua, "boss_bars.remove_player", [bars, server](LuaArgs& args) {
        const std::shared_ptr<PluginPlayerRef> player = playerFrom(args, 2, server->services().players());
        args.pushBoolean(player != nullptr && bars().removePlayer(args.checkString(1), player->uuid()));
        return 1;
    });
    lua.setField("remove_player");
    wire::lua::pushHostFunction(lua, "boss_bars.players", [bars](LuaArgs& args) {
        pushNames(args, bars().players(args.checkString(1)));
        return 1;
    });
    lua.setField("players");
    wire::lua::pushHostFunction(lua, "boss_bars.kept", [bars](LuaArgs& args) {
        pushNames(args, bars().kept());
        return 1;
    });
    lua.setField("kept");
    lua.setGlobal("boss_bars");
}

void installAccess(LuaArgs& lua, PluginHost& host) {
    PluginHost* server = &host;
    lua.pushTable();
    const auto uuidOf = [server](LuaArgs& args) {
        if (const std::shared_ptr<PluginPlayerRef> player = playerFrom(args, 1, server->services().players());
            player != nullptr) {
            return player->uuid();
        }
        if (args.isString(1)) {
            if (const std::optional<PluginKnownPlayer> known = server->services().access().playerByName(args.toText(1));
                known.has_value()) {
                return known->uuid;
            }
            return args.toText(1);
        }
        return std::string{};
    };
    wire::lua::pushHostFunction(lua, "access.is_whitelisted", [server, uuidOf](LuaArgs& args) {
        args.pushBoolean(server->services().access().isWhitelisted(uuidOf(args)));
        return 1;
    });
    lua.setField("is_whitelisted");
    wire::lua::pushHostFunction(lua, "access.is_banned", [server, uuidOf](LuaArgs& args) {
        args.pushBoolean(server->services().access().banOf(uuidOf(args), false).has_value());
        return 1;
    });
    lua.setField("is_banned");
    wire::lua::pushHostFunction(lua, "access.is_operator", [server, uuidOf](LuaArgs& args) {
        args.pushBoolean(server->services().access().isOperator(uuidOf(args)));
        return 1;
    });
    lua.setField("is_operator");
    wire::lua::pushHostFunction(lua, "access.known", [server](LuaArgs& args) {
        const std::optional<PluginKnownPlayer> known = server->services().access().playerByName(args.checkString(1));
        if (!known.has_value()) {
            args.pushNil();
            return 1;
        }
        args.pushTable();
        args.pushString(known->uuid);
        args.setField("uuid");
        args.pushString(known->name);
        args.setField("name");
        args.pushInteger(known->firstSeenMillis);
        args.setField("first_seen");
        args.pushInteger(known->lastSeenMillis);
        args.setField("last_seen");
        return 1;
    });
    lua.setField("known");
    wire::lua::pushHostFunction(lua, "access.statistic", [server, uuidOf](LuaArgs& args) {
        args.pushInteger(server->services().access().statistic(uuidOf(args), args.checkString(2)));
        return 1;
    });
    lua.setField("statistic");
    wire::lua::pushHostFunction(lua, "access.set_statistic", [server, uuidOf](LuaArgs& args) {
        args.pushBoolean(
            server->services().access().setStatistic(uuidOf(args), args.checkString(2), args.checkInteger(3)));
        return 1;
    });
    lua.setField("set_statistic");
    wire::lua::pushHostFunction(lua, "access.last_death", [server, uuidOf](LuaArgs& args) {
        const auto spot = server->services().access().lastDeath(uuidOf(args));
        if (!spot.has_value()) {
            args.pushNil();
            return 1;
        }
        args.pushTable();
        args.pushString((*spot)[0]);
        args.setField("dimension");
        for (std::size_t at = 1; at < 4; ++at) {
            args.pushInteger(std::stoll((*spot)[at]));
            args.setField(std::array<const char*, 3>{"x", "y", "z"}[at - 1]);
        }
        return 1;
    });
    lua.setField("last_death");
    lua.setGlobal("access");
}
} // namespace

void installMisc(LuaArgs& lua, LoadedPlugin& plugin, PluginHost& host) {
    installScoreboards(lua, plugin, host);
    installBossBars(lua, plugin, host);
    installAccess(lua, host);
    misc::installServerTables(lua, host);
    installSettings(lua, host);
}
} // namespace basalt::server::plugins::pluginApi
