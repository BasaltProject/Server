#include "WasmHud.hpp"

#include <array>

namespace basalt::server::plugins::wasm {
namespace hud {
namespace {
constexpr std::array<std::string_view, 16> kTeamColors{
    "black",     "dark_blue", "dark_green", "dark_aqua", "dark_red", "dark_purple",  "gold",   "gray",
    "dark_gray", "blue",      "green",      "aqua",      "red",      "light_purple", "yellow", "white"};

[[nodiscard]] std::optional<std::int32_t> slotOf(std::string_view name) {
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
            if (kTeamColors[color] == name.substr(kTeamPrefix.size())) {
                return static_cast<std::int32_t>(color) + 3;
            }
        }
    }
    return std::nullopt;
}

[[nodiscard]] PluginScoreboards& service(State& state) { return state.host->services().scoreboards(); }

[[nodiscard]] std::int32_t boardOf(State& state, std::int32_t handle, std::int32_t& id) {
    Board* board = nullptr;
    const std::int32_t status = stateOfHud(state).boards.find(handle, board);
    if (status == abi::kOk) {
        id = board->id;
    }
    return status;
}

/// Two strings from guest memory, or nothing when either range is bad.
struct Names {
    std::string_view first;
    std::string_view second;
};

[[nodiscard]] std::optional<Names> namesAt(const abi::GuestMemory& memory, std::int32_t a, std::int32_t alen,
                                           std::int32_t b, std::int32_t blen) {
    const std::optional<std::string_view> first = memory.string(a, alen);
    const std::optional<std::string_view> second = memory.string(b, blen);
    if (!first || !second) {
        return std::nullopt;
    }
    return Names{*first, *second};
}

void scoreboardMain(void* user, abi::Call& c) {
    State& state = stateOf(user);
    HudState& hud = stateOfHud(state);
    Board* found = nullptr;
    if (hud.boards.find(hud.mainBoard, found) != abi::kOk) {
        hud.mainBoard = hud.boards.add(Board{PluginScoreboards::kMain, {}});
    }
    c.ret(hud.mainBoard);
}

void scoreboardCreate(void* user, abi::Call& c) {
    State& state = stateOf(user);
    HudState& hud = stateOfHud(state);
    const std::int32_t id = service(state).create();
    if (id < 0) {
        c.ret(abi::kErrInvalid);
        return;
    }
    PluginHost* host = state.host;
    const std::int32_t handle = hud.boards.add(Board{id, {}});
    if (handle == 0) {
        service(state).release(id);
        c.ret(abi::kErrInvalid);
        return;
    }
    Board* board = nullptr;
    static_cast<void>(hud.boards.find(handle, board));
    board->registration = state.plugin->registrations.add("scoreboard " + std::to_string(id),
                                                          [host, id] { host->services().scoreboards().release(id); });
    c.ret(handle);
}

void scoreboardRelease(void* user, abi::Call& c) {
    State& state = stateOf(user);
    HudState& hud = stateOfHud(state);
    Board* board = nullptr;
    const std::int32_t status = hud.boards.find(c.i32(0), board);
    if (status != abi::kOk) {
        c.ret(status);
        return;
    }
    if (board->id == PluginScoreboards::kMain) {
        c.ret(abi::kErrInvalid);
        return;
    }
    PluginHandle registration = board->registration;
    static_cast<void>(hud.boards.remove(c.i32(0)));
    registration.remove();
    c.ret(abi::kOk);
}

void scoreboardList(void* user, abi::Call& c) {
    State& state = stateOf(user);
    std::int32_t id = 0;
    const std::int32_t status = boardOf(state, c.i32(0), id);
    const std::optional<std::string_view> what = c.memory().string(c.i32(1), c.i32(2));
    if (status != abi::kOk || !what) {
        c.ret(status != abi::kOk ? status : abi::kErrInvalid);
        return;
    }
    std::vector<std::string> names;
    if (*what == "objectives") {
        for (const PluginObjective& objective : service(state).objectives(id)) {
            names.push_back(objective.name);
        }
    } else if (*what == "entries") {
        names = service(state).entries(id);
    } else if (*what == "teams") {
        for (const PluginTeam& team : service(state).teams(id)) {
            names.push_back(team.name);
        }
    } else {
        c.ret(abi::kErrInvalid);
        return;
    }
    c.ret(putValue(c.memory(), c.i32(3), c.i32(4), stringList(names)));
}

void objectiveAdd(void* user, abi::Call& c) {
    State& state = stateOf(user);
    std::int32_t id = 0;
    std::int32_t status = boardOf(state, c.i32(0), id);
    const std::optional<std::string_view> name = c.memory().string(c.i32(1), c.i32(2));
    const std::expected<Value, std::int32_t> spec = readSpec(c.memory(), c.i32(3), c.i32(4));
    if (status == abi::kOk && (!name || name->empty() || !spec)) {
        status = abi::kErrInvalid;
    }
    PluginObjective objective;
    if (status == abi::kOk) {
        std::optional<std::string> criterion;
        std::optional<std::string> title;
        std::optional<bool> hearts;
        status = takeString(*spec, "criterion", criterion);
        status = status != abi::kOk ? status : takeText(state, *spec, "title", title, "mini:");
        status = status != abi::kOk ? status : takeBool(*spec, "hearts", hearts);
        objective.name = std::string{*name};
        objective.criterion = criterion.value_or("dummy");
        objective.displayName = title.value_or(jsonLiteral(*name));
        objective.hearts = hearts.value_or(false);
    }
    if (status == abi::kOk) {
        status = service(state).addObjective(id, objective) ? abi::kOk : abi::kErrInvalid;
    }
    c.ret(status);
}

void objectiveRemove(void* user, abi::Call& c) {
    State& state = stateOf(user);
    std::int32_t id = 0;
    const std::int32_t status = boardOf(state, c.i32(0), id);
    const std::optional<std::string_view> name = c.memory().string(c.i32(1), c.i32(2));
    if (status != abi::kOk || !name) {
        c.ret(status != abi::kOk ? status : abi::kErrInvalid);
        return;
    }
    c.ret(service(state).removeObjective(id, *name) ? abi::kOk : abi::kErrAbsent);
}

void displaySet(void* user, abi::Call& c) {
    State& state = stateOf(user);
    std::int32_t id = 0;
    const std::int32_t status = boardOf(state, c.i32(0), id);
    const std::optional<Names> names = namesAt(c.memory(), c.i32(1), c.i32(2), c.i32(3), c.i32(4));
    if (status != abi::kOk || !names) {
        c.ret(status != abi::kOk ? status : abi::kErrInvalid);
        return;
    }
    const std::optional<std::int32_t> slot = slotOf(names->first);
    if (!slot) {
        c.ret(abi::kErrInvalid);
        return;
    }
    if (names->second.empty()) {
        service(state).display(id, *slot, std::nullopt);
    } else if (!service(state).objective(id, names->second).has_value()) {
        c.ret(abi::kErrAbsent);
        return;
    } else {
        service(state).display(id, *slot, std::string{names->second});
    }
    c.ret(abi::kOk);
}

void displayGet(void* user, abi::Call& c) {
    State& state = stateOf(user);
    std::int32_t id = 0;
    const std::int32_t status = boardOf(state, c.i32(0), id);
    const std::optional<std::string_view> name = c.memory().string(c.i32(1), c.i32(2));
    const std::optional<std::int32_t> slot = name ? slotOf(*name) : std::nullopt;
    if (status != abi::kOk || !slot) {
        c.ret(status != abi::kOk ? status : abi::kErrInvalid);
        return;
    }
    const std::optional<std::string> shown = service(state).displayed(id, *slot);
    c.ret(shown ? c.memory().put(c.i32(3), c.i32(4), *shown) : abi::kErrAbsent);
}

void scoreGet(void* user, abi::Call& c) {
    State& state = stateOf(user);
    std::int32_t id = 0;
    const std::int32_t status = boardOf(state, c.i32(0), id);
    const std::optional<Names> names = namesAt(c.memory(), c.i32(1), c.i32(2), c.i32(3), c.i32(4));
    if (status != abi::kOk || !names) {
        c.ret(status != abi::kOk ? status : abi::kErrInvalid);
        return;
    }
    const std::optional<std::int32_t> score = service(state).score(id, names->first, names->second);
    if (!score) {
        c.ret(abi::kErrAbsent);
        return;
    }
    c.ret(c.memory().write(c.i32(5), *score) ? abi::kOk : abi::kErrInvalid);
}

void scoreSet(void* user, abi::Call& c) {
    State& state = stateOf(user);
    std::int32_t id = 0;
    const std::int32_t status = boardOf(state, c.i32(0), id);
    const std::optional<Names> names = namesAt(c.memory(), c.i32(1), c.i32(2), c.i32(3), c.i32(4));
    if (status != abi::kOk || !names || names->first.empty()) {
        c.ret(status != abi::kOk ? status : abi::kErrInvalid);
        return;
    }
    c.ret(service(state).setScore(id, names->first, names->second, c.i32(5)) ? abi::kOk : abi::kErrInvalid);
}

void scoreReset(void* user, abi::Call& c) {
    State& state = stateOf(user);
    std::int32_t id = 0;
    const std::int32_t status = boardOf(state, c.i32(0), id);
    const std::optional<Names> names = namesAt(c.memory(), c.i32(1), c.i32(2), c.i32(3), c.i32(4));
    if (status != abi::kOk || !names) {
        c.ret(status != abi::kOk ? status : abi::kErrInvalid);
        return;
    }
    service(state).resetScore(id, names->first,
                              names->second.empty() ? std::nullopt : std::optional{std::string{names->second}});
    c.ret(abi::kOk);
}

void teamAdd(void* user, abi::Call& c) {
    State& state = stateOf(user);
    std::int32_t id = 0;
    const std::int32_t status = boardOf(state, c.i32(0), id);
    const std::optional<std::string_view> name = c.memory().string(c.i32(1), c.i32(2));
    if (status != abi::kOk || !name || name->empty()) {
        c.ret(status != abi::kOk ? status : abi::kErrInvalid);
        return;
    }
    c.ret(service(state).addTeam(id, *name) ? abi::kOk : abi::kErrInvalid);
}

[[nodiscard]] std::int32_t applyTeamSpec(State& state, const Value& spec, PluginTeam& team) {
    std::optional<std::string> text;
    std::optional<std::string> word;
    std::optional<bool> flag;
    std::int32_t status = takeText(state, spec, "display_name", text, "mini:");
    team.displayName = text.value_or(team.displayName);
    text.reset();
    status = status != abi::kOk ? status : takeText(state, spec, "prefix", text, "mini:");
    team.prefix = text.value_or(team.prefix);
    text.reset();
    status = status != abi::kOk ? status : takeText(state, spec, "suffix", text, "mini:");
    team.suffix = text.value_or(team.suffix);
    status = status != abi::kOk ? status : takeString(spec, "color", word);
    if (word) {
        team.color = *word == "none" ? std::string{} : *word;
        word.reset();
    }
    status = status != abi::kOk ? status : takeBool(spec, "friendly_fire", flag);
    team.friendlyFire = flag.value_or(team.friendlyFire);
    flag.reset();
    status = status != abi::kOk ? status : takeBool(spec, "see_invisible", flag);
    team.seeFriendlyInvisibles = flag.value_or(team.seeFriendlyInvisibles);
    status = status != abi::kOk ? status : takeString(spec, "name_tags", word);
    team.nameTagVisibility = word.value_or(team.nameTagVisibility);
    word.reset();
    status = status != abi::kOk ? status : takeString(spec, "death_messages", word);
    team.deathMessageVisibility = word.value_or(team.deathMessageVisibility);
    word.reset();
    status = status != abi::kOk ? status : takeString(spec, "collision", word);
    team.collisionRule = word.value_or(team.collisionRule);
    return status;
}

void teamSet(void* user, abi::Call& c) {
    State& state = stateOf(user);
    std::int32_t id = 0;
    std::int32_t status = boardOf(state, c.i32(0), id);
    const std::optional<std::string_view> name = c.memory().string(c.i32(1), c.i32(2));
    const std::expected<Value, std::int32_t> spec = readSpec(c.memory(), c.i32(3), c.i32(4));
    if (status == abi::kOk && (!name || !spec)) {
        status = abi::kErrInvalid;
    }
    if (status == abi::kOk) {
        std::optional<PluginTeam> team = service(state).team(id, *name);
        if (!team) {
            status = abi::kErrAbsent;
        } else {
            status = applyTeamSpec(state, *spec, *team);
            if (status == abi::kOk) {
                status = service(state).updateTeam(id, *team) ? abi::kOk : abi::kErrInvalid;
            }
        }
    }
    c.ret(status);
}

void teamRemove(void* user, abi::Call& c) {
    State& state = stateOf(user);
    std::int32_t id = 0;
    const std::int32_t status = boardOf(state, c.i32(0), id);
    const std::optional<std::string_view> name = c.memory().string(c.i32(1), c.i32(2));
    if (status != abi::kOk || !name) {
        c.ret(status != abi::kOk ? status : abi::kErrInvalid);
        return;
    }
    c.ret(service(state).removeTeam(id, *name) ? abi::kOk : abi::kErrAbsent);
}

void teamJoin(void* user, abi::Call& c) {
    State& state = stateOf(user);
    std::int32_t id = 0;
    const std::int32_t status = boardOf(state, c.i32(0), id);
    const std::optional<Names> names = namesAt(c.memory(), c.i32(1), c.i32(2), c.i32(3), c.i32(4));
    if (status != abi::kOk || !names || names->second.empty()) {
        c.ret(status != abi::kOk ? status : abi::kErrInvalid);
        return;
    }
    c.ret(service(state).addMember(id, names->first, names->second) ? abi::kOk : abi::kErrAbsent);
}

void teamLeave(void* user, abi::Call& c) {
    State& state = stateOf(user);
    std::int32_t id = 0;
    const std::int32_t status = boardOf(state, c.i32(0), id);
    const std::optional<Names> names = namesAt(c.memory(), c.i32(1), c.i32(2), c.i32(3), c.i32(4));
    if (status != abi::kOk || !names) {
        c.ret(status != abi::kOk ? status : abi::kErrInvalid);
        return;
    }
    c.ret(service(state).removeMember(id, names->first, names->second) ? abi::kOk : abi::kErrAbsent);
}

void teamOf(void* user, abi::Call& c) {
    State& state = stateOf(user);
    std::int32_t id = 0;
    const std::int32_t status = boardOf(state, c.i32(0), id);
    const std::optional<std::string_view> entry = c.memory().string(c.i32(1), c.i32(2));
    if (status != abi::kOk || !entry) {
        c.ret(status != abi::kOk ? status : abi::kErrInvalid);
        return;
    }
    const std::optional<PluginTeam> team = service(state).teamOf(id, *entry);
    c.ret(team ? c.memory().put(c.i32(3), c.i32(4), team->name) : abi::kErrAbsent);
}

void scoreboardShow(void* user, abi::Call& c) {
    State& state = stateOf(user);
    std::shared_ptr<PluginPlayerRef> player;
    std::int32_t status = state.resolvePlayer(c.i32(0), player);
    std::int32_t id = 0;
    if (status == abi::kOk) {
        status = boardOf(state, c.i32(1), id);
    }
    if (status == abi::kOk) {
        service(state).show(player->uuid(), id);
    }
    c.ret(status);
}

void scoreboardShown(void* user, abi::Call& c) {
    State& state = stateOf(user);
    std::shared_ptr<PluginPlayerRef> player;
    const std::int32_t status = state.resolvePlayer(c.i32(0), player);
    if (status != abi::kOk) {
        c.ret(status);
        return;
    }
    HudState& hud = stateOfHud(state);
    const std::int32_t shown = service(state).shown(player->uuid());
    std::int32_t found = 0;
    hud.boards.forEach([&](std::int32_t handle, Board& board) {
        if (found == 0 && board.id == shown) {
            found = handle;
        }
    });
    if (found == 0 && shown == PluginScoreboards::kMain) {
        found = hud.boards.add(Board{PluginScoreboards::kMain, {}});
        hud.mainBoard = found;
    }
    c.ret(found != 0 ? found : abi::kErrAbsent);
}
} // namespace
} // namespace hud

void addHudImports(State& state, std::vector<abi::HostImport>& out) {
    State* user = &state;
    out.insert(out.end(), {
                              {"hud_scoreboard_main", hud::scoreboardMain, user},
                              {"hud_scoreboard_create", hud::scoreboardCreate, user},
                              {"hud_scoreboard_release", hud::scoreboardRelease, user},
                              {"hud_scoreboard_list", hud::scoreboardList, user},
                              {"hud_objective_add", hud::objectiveAdd, user},
                              {"hud_objective_remove", hud::objectiveRemove, user},
                              {"hud_display_set", hud::displaySet, user},
                              {"hud_display_get", hud::displayGet, user},
                              {"hud_score_get", hud::scoreGet, user},
                              {"hud_score_set", hud::scoreSet, user},
                              {"hud_score_reset", hud::scoreReset, user},
                              {"hud_team_add", hud::teamAdd, user},
                              {"hud_team_set", hud::teamSet, user},
                              {"hud_team_remove", hud::teamRemove, user},
                              {"hud_team_join", hud::teamJoin, user},
                              {"hud_team_leave", hud::teamLeave, user},
                              {"hud_team_of", hud::teamOf, user},
                              {"hud_scoreboard_show", hud::scoreboardShow, user},
                              {"hud_scoreboard_shown", hud::scoreboardShown, user},
                          });
}
} // namespace basalt::server::plugins::wasm
