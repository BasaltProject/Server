#include <basalt/server/plugins/PluginHost.hpp>
#include <basalt/server/plugins/PluginMiscApi.hpp>
#include <basalt/server/plugins/PluginObjects.hpp>
#include <basalt/wire/lua/LuaCall.hpp>
#include <basalt/wire/nbt/NbtIO.hpp>
#include <basalt/wire/nbt/Snbt.hpp>

#include <array>
#include <cstdint>
#include <string>

namespace basalt::server::plugins::pluginApi {
namespace {
using wire::lua::LuaArgs;

constexpr std::array<std::string_view, 16> kTeamColors{
    "black",     "dark_blue", "dark_green", "dark_aqua", "dark_red", "dark_purple",  "gold",   "gray",
    "dark_gray", "blue",      "green",      "aqua",      "red",      "light_purple", "yellow", "white"};

/// Plugin text goes to the server as MiniMessage, which the server reads when the text starts with "mini:".
[[nodiscard]] std::string textAt(LoadedPlugin& plugin, LuaArgs& args, std::int32_t index) {
    return "mini:" + plugin.text.resolve(args.checkString(index), &args, index + 1);
}

[[nodiscard]] std::int32_t slotOf(LuaArgs& args, std::int32_t index) {
    if (args.isNumber(index)) {
        return static_cast<std::int32_t>(args.toInteger(index));
    }
    const std::string name = args.checkString(index);
    if (name == "list") {
        return 0;
    }
    if (name == "sidebar") {
        return 1;
    }
    if (name == "below_name") {
        return 2;
    }
    constexpr std::string_view kTeamPrefix = "sidebar.team.";
    if (name.starts_with(kTeamPrefix)) {
        for (std::size_t color = 0; color < kTeamColors.size(); ++color) {
            if (kTeamColors[color] == std::string_view{name}.substr(kTeamPrefix.size())) {
                return static_cast<std::int32_t>(color) + 3;
            }
        }
    }
    args.raise("'" + name + "' is not a display slot: list, sidebar, below_name or sidebar.team.<color>");
}

[[nodiscard]] std::optional<std::string> stringField(LuaArgs& lua, std::int32_t index, std::string_view key) {
    lua.pushField(index, key);
    std::optional<std::string> value;
    if (lua.isString(-1)) {
        value = lua.toText(-1);
    }
    lua.pop(1);
    return value;
}

[[nodiscard]] std::optional<bool> flagField(LuaArgs& lua, std::int32_t index, std::string_view key) {
    lua.pushField(index, key);
    std::optional<bool> value;
    if (lua.isBoolean(-1)) {
        value = lua.toBoolean(-1);
    }
    lua.pop(1);
    return value;
}

[[nodiscard]] std::optional<double> numberField(LuaArgs& lua, std::int32_t index, std::string_view key) {
    lua.pushField(index, key);
    std::optional<double> value;
    if (lua.isNumber(-1)) {
        value = lua.toNumber(-1);
    }
    lua.pop(1);
    return value;
}

void pushNames(LuaArgs& args, const std::vector<std::string>& names) {
    args.pushTable();
    std::int64_t position = 1;
    for (const std::string& name : names) {
        args.pushString(name);
        args.setIndex(position++);
    }
}

[[nodiscard]] std::vector<std::string> namesField(LuaArgs& lua, std::int32_t index, std::string_view key) {
    std::vector<std::string> out;
    lua.pushField(index, key);
    const std::int32_t at = lua.top();
    if (lua.isString(at)) {
        out.push_back(lua.toText(at));
    } else if (lua.isTable(at)) {
        const std::int64_t count = lua.length(at);
        for (std::int64_t position = 1; position <= count; ++position) {
            lua.pushIndex(at, position);
            out.push_back(lua.toText(-1));
            lua.pop(1);
        }
    }
    lua.pop(1);
    return out;
}

void pushSnbt(LuaArgs& args, const std::optional<std::vector<std::uint8_t>>& bytes) {
    if (bytes.has_value()) {
        const std::expected<wire::nbt::AnonymousTag, wire::nbt::NbtError> read =
            wire::nbt::nbtIO::readAnonymous(*bytes);
        if (read.has_value()) {
            args.pushString(wire::nbt::snbt::stringify(read->tag));
            return;
        }
    }
    args.pushNil();
}

[[nodiscard]] std::optional<std::vector<std::uint8_t>> snbtBytes(LuaArgs& args, std::int32_t index) {
    const std::expected<wire::nbt::NbtTag, wire::nbt::NbtError> parsed =
        wire::nbt::snbt::parse(args.checkString(index));
    if (!parsed.has_value()) {
        return std::nullopt;
    }
    return wire::nbt::nbtIO::writeAnonymous(*parsed);
}

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
    lua.setGlobal("access");
}

void installServerTables(LuaArgs& lua, PluginHost& host) {
    PluginHost* server = &host;
    const auto control = [server]() -> PluginServerControl& { return server->services().serverControl(); };

    lua.pushTable();
    wire::lua::pushHostFunction(lua, "tick.rate", [control](LuaArgs& args) {
        args.pushNumber(control().tickRate());
        return 1;
    });
    lua.setField("rate");
    wire::lua::pushHostFunction(lua, "tick.set_rate", [control](LuaArgs& args) {
        control().setTickRate(static_cast<float>(args.checkNumber(1)));
        return 0;
    });
    lua.setField("set_rate");
    wire::lua::pushHostFunction(lua, "tick.frozen", [control](LuaArgs& args) {
        if (!args.isNil(1)) {
            control().setFrozen(args.toBoolean(1));
            return 0;
        }
        args.pushBoolean(control().frozen());
        return 1;
    });
    lua.setField("frozen");
    wire::lua::pushHostFunction(lua, "tick.step", [control](LuaArgs& args) {
        args.pushBoolean(control().step(static_cast<std::int32_t>(args.checkInteger(1))));
        return 1;
    });
    lua.setField("step");
    wire::lua::pushHostFunction(lua, "tick.sprint", [control](LuaArgs& args) {
        args.pushBoolean(control().sprint(static_cast<std::int32_t>(args.checkInteger(1))));
        return 1;
    });
    lua.setField("sprint");
    wire::lua::pushHostFunction(lua, "tick.stop_sprint", [control](LuaArgs& args) {
        args.pushBoolean(control().stopSprinting());
        return 1;
    });
    lua.setField("stop_sprint");
    wire::lua::pushHostFunction(lua, "tick.pace", [control](LuaArgs& args) {
        const std::array<double, 4> pace = control().pace();
        args.pushTable();
        for (std::size_t index = 0; index < pace.size(); ++index) {
            args.pushNumber(pace[index]);
            args.setIndex(static_cast<std::int64_t>(index) + 1);
        }
        return 1;
    });
    lua.setField("pace");
    lua.setGlobal("tick");

    lua.pushTable();
    wire::lua::pushHostFunction(lua, "server_links.list", [control](LuaArgs& args) {
        args.pushTable();
        std::int64_t position = 1;
        for (const PluginServerLink& link : control().links()) {
            args.pushTable();
            args.pushString(link.kind);
            args.setField("kind");
            args.pushString(link.url);
            args.setField("url");
            args.setIndex(position++);
        }
        return 1;
    });
    lua.setField("list");
    wire::lua::pushHostFunction(lua, "server_links.set", [control](LuaArgs& args) {
        if (!args.isTable(1)) {
            args.raise("server_links.set takes a list of { kind = ..., url = ... }");
        }
        std::vector<PluginServerLink> links;
        const std::int64_t count = args.length(1);
        for (std::int64_t position = 1; position <= count; ++position) {
            args.pushIndex(1, position);
            links.push_back(PluginServerLink{.kind = stringField(args, -1, "kind").value_or(""),
                                             .label = stringField(args, -1, "label").value_or(""),
                                             .url = stringField(args, -1, "url").value_or("")});
            args.pop(1);
        }
        control().setLinks(std::move(links));
        return 0;
    });
    lua.setField("set");
    lua.setGlobal("server_links");

    lua.pushTable();
    wire::lua::pushHostFunction(lua, "datapacks.list", [server](LuaArgs& args) {
        args.pushTable();
        std::int64_t position = 1;
        for (const PluginDatapack& pack : server->services().datapacks().packs()) {
            args.pushTable();
            args.pushString(pack.id);
            args.setField("id");
            args.pushBoolean(pack.enabled);
            args.setField("enabled");
            args.pushBoolean(pack.required);
            args.setField("required");
            args.pushString(pack.compatibility);
            args.setField("compatibility");
            args.pushString(pack.source);
            args.setField("source");
            args.setIndex(position++);
        }
        return 1;
    });
    lua.setField("list");
    wire::lua::pushHostFunction(lua, "datapacks.refresh", [server](LuaArgs& args) {
        static_cast<void>(args);
        server->services().datapacks().refresh();
        return 0;
    });
    lua.setField("refresh");
    lua.setGlobal("datapacks");

    lua.pushTable();
    wire::lua::pushHostFunction(lua, "advancements.ids", [server](LuaArgs& args) {
        pushNames(args, server->services().advancements().ids());
        return 1;
    });
    lua.setField("ids");
    wire::lua::pushHostFunction(lua, "advancements.info", [server](LuaArgs& args) {
        const std::optional<PluginAdvancement> info =
            server->services().advancements().advancement(args.checkString(1));
        if (!info.has_value()) {
            args.pushNil();
            return 1;
        }
        args.pushTable();
        args.pushString(info->id);
        args.setField("id");
        if (info->parent.has_value()) {
            args.pushString(*info->parent);
            args.setField("parent");
        }
        args.pushString(info->root);
        args.setField("root");
        pushNames(args, info->criteria);
        args.setField("criteria");
        pushNames(args, info->children);
        args.setField("children");
        args.pushString(info->frame);
        args.setField("frame");
        args.pushString(info->iconItem);
        args.setField("icon");
        return 1;
    });
    lua.setField("info");
    for (const bool granting : {true, false}) {
        wire::lua::pushHostFunction(
            lua, granting ? "advancements.grant" : "advancements.revoke", [server, granting](LuaArgs& args) {
                const std::shared_ptr<PluginPlayerRef> player = playerFrom(args, 1, server->services().players());
                if (player == nullptr) {
                    args.raise("advancements.grant(player, id, criterion) needs an online player");
                }
                PluginAdvancements& all = server->services().advancements();
                args.pushBoolean(granting ? all.grant(player->uuid(), args.checkString(2), args.checkString(3))
                                          : all.revoke(player->uuid(), args.checkString(2), args.checkString(3)));
                return 1;
            });
        lua.setField(granting ? "grant" : "revoke");
    }
    wire::lua::pushHostFunction(lua, "advancements.progress", [server](LuaArgs& args) {
        const std::shared_ptr<PluginPlayerRef> player = playerFrom(args, 1, server->services().players());
        args.pushTable();
        if (player != nullptr) {
            for (const auto& [criterion, when] :
                 server->services().advancements().obtained(player->uuid(), args.checkString(2))) {
                args.pushInteger(when);
                args.setField(criterion);
            }
        }
        return 1;
    });
    lua.setField("progress");
    lua.setGlobal("advancements");

    lua.pushTable();
    wire::lua::pushHostFunction(lua, "loot.tables", [server](LuaArgs& args) {
        pushNames(args, server->services().loot().tables());
        return 1;
    });
    lua.setField("tables");
    wire::lua::pushHostFunction(lua, "loot.roll", [server](LuaArgs& args) {
        PluginLootRoll roll;
        const std::string table = args.checkString(1);
        if (!args.isTable(2)) {
            args.raise("loot.roll(table, { world = ..., x = ..., y = ..., z = ... }) takes a table");
        }
        roll.world = stringField(args, 2, "world").value_or(std::string{});
        roll.x = numberField(args, 2, "x").value_or(0.0);
        roll.y = numberField(args, 2, "y").value_or(0.0);
        roll.z = numberField(args, 2, "z").value_or(0.0);
        roll.luck = static_cast<float>(numberField(args, 2, "luck").value_or(0.0));
        roll.seed = static_cast<std::int64_t>(numberField(args, 2, "seed").value_or(0.0));
        roll.looter = stringField(args, 2, "looter").value_or(std::string{});
        const std::optional<std::vector<PluginLootItem>> items = server->services().loot().roll(table, roll);
        if (!items.has_value()) {
            args.pushNil();
            return 1;
        }
        args.pushTable();
        std::int64_t position = 1;
        for (const PluginLootItem& item : *items) {
            args.pushTable();
            args.pushString(item.item);
            args.setField("item");
            args.pushInteger(item.count);
            args.setField("count");
            args.pushString(item.components);
            args.setField("components");
            args.setIndex(position++);
        }
        return 1;
    });
    lua.setField("roll");
    wire::lua::pushHostFunction(lua, "loot.enchant", [server](LuaArgs& args) {
        if (!args.isTable(1)) {
            args.raise("loot.enchant({ item = ..., levels = ..., world = ... }) takes a table");
        }
        PluginLootItem given;
        given.item = stringField(args, 1, "item").value_or(std::string{});
        given.count = static_cast<std::int32_t>(numberField(args, 1, "count").value_or(1.0));
        given.components = stringField(args, 1, "components").value_or(std::string{});
        PluginLootEnchant how;
        how.world = stringField(args, 1, "world").value_or(std::string{});
        how.options = stringField(args, 1, "options").value_or(how.options);
        how.levels = static_cast<std::int32_t>(numberField(args, 1, "levels").value_or(1.0));
        how.seed = static_cast<std::int64_t>(numberField(args, 1, "seed").value_or(0.0));
        const std::optional<PluginLootItem> made = server->services().loot().enchant(given, how);
        if (!made.has_value()) {
            args.pushNil();
            return 1;
        }
        args.pushTable();
        args.pushString(made->item);
        args.setField("item");
        args.pushInteger(made->count);
        args.setField("count");
        args.pushString(made->components);
        args.setField("components");
        return 1;
    });
    lua.setField("enchant");
    lua.setGlobal("loot");

    lua.pushTable();
    wire::lua::pushHostFunction(lua, "maps.create", [server](LuaArgs& args) {
        args.pushInteger(server->services().maps().create(
            args.checkString(1), static_cast<std::int32_t>(args.checkInteger(2)),
            static_cast<std::int32_t>(args.checkInteger(3)), static_cast<std::int32_t>(args.integerOr(4, 0))));
        return 1;
    });
    lua.setField("create");
    wire::lua::pushHostFunction(lua, "maps.info", [server](LuaArgs& args) {
        const std::optional<PluginMap> map =
            server->services().maps().map(static_cast<std::int32_t>(args.checkInteger(1)));
        if (!map.has_value()) {
            args.pushNil();
            return 1;
        }
        args.pushTable();
        args.pushString(map->dimension);
        args.setField("dimension");
        args.pushInteger(map->scale);
        args.setField("scale");
        args.pushInteger(map->centerX);
        args.setField("center_x");
        args.pushInteger(map->centerZ);
        args.setField("center_z");
        args.pushBoolean(map->locked);
        args.setField("locked");
        return 1;
    });
    lua.setField("info");
    wire::lua::pushHostFunction(lua, "maps.explorer", [server](LuaArgs& args) {
        if (!args.isTable(1)) {
            args.raise(
                "maps.explorer({ world = ..., x = ..., y = ..., z = ..., destination = { ... } }) takes a table");
        }
        PluginExploration request;
        request.destination = namesField(args, 1, "destination");
        request.decoration = stringField(args, 1, "decoration").value_or(request.decoration);
        request.zoom = static_cast<std::int32_t>(numberField(args, 1, "zoom").value_or(request.zoom));
        request.radius = static_cast<std::int32_t>(numberField(args, 1, "radius").value_or(request.radius));
        request.skipExisting = flagField(args, 1, "skip_existing").value_or(request.skipExisting);
        const std::optional<std::vector<std::uint8_t>> made = server->services().maps().explorerMap(
            stringField(args, 1, "world").value_or(std::string{}), numberField(args, 1, "x").value_or(0.0),
            numberField(args, 1, "y").value_or(0.0), numberField(args, 1, "z").value_or(0.0), request);
        pushSnbt(args, made);
        return 1;
    });
    lua.setField("explorer");
    lua.setGlobal("maps");

    lua.pushTable();
    wire::lua::pushHostFunction(lua, "structures.ids", [server](LuaArgs& args) {
        pushNames(args, server->services().structures().ids());
        return 1;
    });
    lua.setField("ids");
    wire::lua::pushHostFunction(lua, "structures.read", [server](LuaArgs& args) {
        pushSnbt(args, server->services().structures().read(args.checkString(1)));
        return 1;
    });
    lua.setField("read");
    wire::lua::pushHostFunction(lua, "structures.write", [server](LuaArgs& args) {
        const std::optional<std::vector<std::uint8_t>> bytes = snbtBytes(args, 2);
        args.pushBoolean(bytes.has_value() &&
                         server->services().structures().store(args.checkString(1), *bytes, args.toBoolean(3)));
        return 1;
    });
    lua.setField("write");
    wire::lua::pushHostFunction(lua, "structures.remove", [server](LuaArgs& args) {
        args.pushBoolean(server->services().structures().remove(args.checkString(1), args.toBoolean(2)));
        return 1;
    });
    lua.setField("remove");
    wire::lua::pushHostFunction(lua, "structures.file", [server](LuaArgs& args) {
        args.pushString(server->services().structures().file(args.checkString(1)));
        return 1;
    });
    lua.setField("file");
    wire::lua::pushHostFunction(lua, "structures.capture", [server](LuaArgs& args) {
        if (!args.isTable(1)) {
            args.raise("structures.capture({ world = ..., x = ..., y = ..., z = ..., size_x = ... }) takes a table");
        }
        const auto number = [&args](std::string_view key) {
            return static_cast<std::int32_t>(numberField(args, 1, key).value_or(0.0));
        };
        pushSnbt(args, server->services().structures().capture(stringField(args, 1, "world").value_or(std::string{}),
                                                               number("x"), number("y"), number("z"), number("size_x"),
                                                               number("size_y"), number("size_z"),
                                                               flagField(args, 1, "entities").value_or(false)));
        return 1;
    });
    lua.setField("capture");
    wire::lua::pushHostFunction(lua, "structures.place", [server](LuaArgs& args) {
        const std::optional<std::vector<std::uint8_t>> bytes = snbtBytes(args, 1);
        if (!bytes.has_value() || !args.isTable(2)) {
            args.pushBoolean(false);
            return 1;
        }
        PluginStructurePlacement placement;
        placement.world = stringField(args, 2, "world").value_or(std::string{});
        placement.x = static_cast<std::int32_t>(numberField(args, 2, "x").value_or(0.0));
        placement.y = static_cast<std::int32_t>(numberField(args, 2, "y").value_or(0.0));
        placement.z = static_cast<std::int32_t>(numberField(args, 2, "z").value_or(0.0));
        placement.rotation = static_cast<std::int32_t>(numberField(args, 2, "rotation").value_or(0.0));
        placement.mirror = static_cast<std::int32_t>(numberField(args, 2, "mirror").value_or(0.0));
        placement.integrity = static_cast<float>(numberField(args, 2, "integrity").value_or(1.0));
        placement.seed = static_cast<std::int64_t>(numberField(args, 2, "seed").value_or(0.0));
        placement.entities = flagField(args, 2, "entities").value_or(true);
        placement.palette = static_cast<std::int32_t>(numberField(args, 2, "palette").value_or(-1.0));
        args.pushBoolean(server->services().structures().place(*bytes, placement));
        return 1;
    });
    lua.setField("place");
    lua.setGlobal("structures");
}
} // namespace

void installMisc(LuaArgs& lua, LoadedPlugin& plugin, PluginHost& host) {
    installScoreboards(lua, plugin, host);
    installBossBars(lua, plugin, host);
    installAccess(lua, host);
    installServerTables(lua, host);
    installSettings(lua, host);
}
} // namespace basalt::server::plugins::pluginApi
