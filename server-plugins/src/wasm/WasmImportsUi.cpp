#include "WasmMenus.hpp"
#include "WasmState.hpp"

#include <algorithm>
#include <charconv>

namespace basalt::server::plugins::wasm {
namespace {
void dispatchCommand(PluginHost& host, const std::shared_ptr<State*>& link, std::int32_t token, const std::string& why,
                     PluginCommandCall& call) {
    const std::lock_guard<std::recursive_mutex> turn{host.turnMutex()};
    State* state = *link;
    if (state == nullptr) {
        return;
    }
    Context context;
    context.kind = Context::Kind::Command;
    context.command = &call;
    static_cast<void>(state->run(token, context, why));
}

void commandRegister(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const abi::GuestMemory memory = c.memory();
    const std::optional<std::string_view> spec = memory.string(c.i32(0), c.i32(1));
    const std::optional<std::string_view> permission = memory.string(c.i32(2), c.i32(3));
    const std::int32_t token = c.i32(4);
    if (!spec || !permission) {
        c.ret(abi::kErrInvalid);
        return;
    }
    const std::expected<PluginCommandSpec, wire::plugins::ManifestError> parsed =
        PluginCommandSpec::parse(*spec, std::optional<std::string>{std::string{*permission}});
    if (!parsed.has_value()) {
        state.plugin->log.warn([&parsed] { return "command_register: " + parsed.error().message; });
        c.ret(abi::kErrInvalid);
        return;
    }
    std::string key;
    for (const std::string& literal : parsed->literals) {
        key += key.empty() ? literal : " " + literal;
    }
    if (state.plugin->commandNamed(key) != nullptr) {
        c.ret(abi::kErrInvalid);
        return;
    }
    PluginHost* host = state.host;
    LoadedPlugin* plugin = state.plugin;
    const std::shared_ptr<State*> link = state.link;
    state.plugin->commands.emplace_back(key, *parsed);
    PluginHandle handle = host->services().commands().registerCommand(
        plugin->manifest.id, *parsed, [host, link, token, why = "/" + parsed->name()](PluginCommandCall& call) {
            dispatchCommand(*host, link, token, why, call);
        });
    static_cast<void>(
        plugin->registrations.add("command /" + parsed->usage(), [plugin, key, moved = std::move(handle)]() mutable {
            moved.remove();
            std::erase_if(plugin->commands, [&key](const auto& held) { return held.first == key; });
        }));
    c.ret(abi::kOk);
}

[[nodiscard]] std::int32_t commandContext(State& state, std::int32_t id, Context*& context) {
    return state.check(id, Context::Kind::Command, context);
}

void commandSourceName(void* user, abi::Call& c) {
    Context* context = nullptr;
    const std::int32_t status = commandContext(stateOf(user), c.i32(0), context);
    c.ret(status != abi::kOk ? status : c.memory().put(c.i32(1), c.i32(2), context->command->source()));
}

void commandIsConsole(void* user, abi::Call& c) {
    Context* context = nullptr;
    const std::int32_t status = commandContext(stateOf(user), c.i32(0), context);
    c.ret(status != abi::kOk ? status : (context->command->isConsole() ? 1 : 0));
}

void commandReply(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const abi::GuestMemory memory = c.memory();
    const std::optional<std::string_view> text = memory.string(c.i32(1), c.i32(2));
    const std::optional<ArgumentList> arguments = readArguments(memory, c.i32(3), c.i32(4));
    Context* context = nullptr;
    std::int32_t status = commandContext(state, c.i32(0), context);
    if (status == abi::kOk && (!text || !arguments)) {
        status = abi::kErrInvalid;
    }
    if (status == abi::kOk) {
        if (renderText(state.plugin->text, *text, arguments->view(), state.scratch)) {
            context->command->reply(state.scratch);
        } else {
            status = abi::kErrAbsent;
        }
    }
    c.ret(status);
}

[[nodiscard]] PluginCommandCall::Argument argumentOf(State& state, abi::Call& c, std::int32_t& status) {
    Context* context = nullptr;
    status = commandContext(state, c.i32(0), context);
    const std::optional<std::string_view> name = c.memory().string(c.i32(1), c.i32(2));
    if (status != abi::kOk || !name) {
        status = status != abi::kOk ? status : abi::kErrInvalid;
        return {};
    }
    PluginCommandCall::Argument value = context->command->argument(*name);
    status = std::holds_alternative<std::monostate>(value) ? abi::kErrAbsent : abi::kOk;
    return value;
}

void commandArgStr(void* user, abi::Call& c) {
    std::int32_t status = abi::kOk;
    const PluginCommandCall::Argument value = argumentOf(stateOf(user), c, status);
    if (status != abi::kOk) {
        c.ret(status);
        return;
    }
    std::string text;
    if (const auto* word = std::get_if<std::string>(&value)) {
        text = *word;
    } else if (const auto* whole = std::get_if<std::int64_t>(&value)) {
        text = std::to_string(*whole);
    } else if (const auto* real = std::get_if<double>(&value)) {
        char digits[40];
        const auto [end, error] = std::to_chars(digits, digits + sizeof digits, *real);
        text.assign(digits, error == std::errc{} ? end : digits);
    } else {
        text = std::get<bool>(value) ? "true" : "false";
    }
    c.ret(c.memory().put(c.i32(3), c.i32(4), text));
}

void commandArgI64(void* user, abi::Call& c) {
    std::int32_t status = abi::kOk;
    const PluginCommandCall::Argument value = argumentOf(stateOf(user), c, status);
    if (status != abi::kOk) {
        c.ret(status);
        return;
    }
    const auto* whole = std::get_if<std::int64_t>(&value);
    c.ret(whole != nullptr && c.memory().write(c.i32(3), *whole) ? abi::kOk : abi::kErrInvalid);
}

void menuCreate(void* user, abi::Call& c) {
    const std::optional<std::string_view> title = c.memory().string(c.i32(0), c.i32(1));
    if (!title || c.i32(2) < 1 || c.i32(2) > PluginMenus::kMaxRows) {
        c.ret(abi::kErrInvalid);
        return;
    }
    c.ret(stateOf(user).menus->create(std::string{*title}, c.i32(2)));
}

void redraw(State& state, Menu& menu) {
    for (const auto& [uuid, handle] : menu.opened) {
        const std::shared_ptr<PluginPlayerRef> player = state.host->services().players().byUuid(uuid);
        if (player != nullptr) {
            state.host->services().menus().refresh(*player, menu.items);
        }
    }
}

[[nodiscard]] std::vector<std::string> loreLines(std::string_view lore) {
    std::vector<std::string> lines;
    std::size_t at = 0;
    while (at <= lore.size() && !lore.empty()) {
        const std::size_t end = lore.find('\n', at);
        lines.emplace_back(lore.substr(at, end == std::string_view::npos ? std::string_view::npos : end - at));
        if (end == std::string_view::npos) {
            break;
        }
        at = end + 1;
    }
    return lines;
}

void menuSet(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const abi::GuestMemory memory = c.memory();
    const std::optional<std::string_view> item = memory.string(c.i32(2), c.i32(3));
    const std::optional<std::string_view> name = memory.string(c.i32(4), c.i32(5));
    const std::optional<std::string_view> lore = memory.string(c.i32(6), c.i32(7));
    Menu* menu = state.menus->find(c.i32(0));
    if (menu == nullptr || !item || !name || !lore || item->empty() || c.i32(1) < 0 ||
        c.i32(1) >= menu->rows * PluginMenus::kColumns) {
        c.ret(abi::kErrInvalid);
        return;
    }
    PluginItem entry{.id = std::string{*item}};
    if (!name->empty()) {
        entry.name = std::string{*name};
    }
    entry.lore = loreLines(*lore);
    menu->items.insert_or_assign(c.i32(1), std::move(entry));
    menu->tokens.insert_or_assign(c.i32(1), c.i32(8));
    redraw(state, *menu);
    c.ret(abi::kOk);
}

void clicked(PluginHost& host, const std::shared_ptr<State*>& link, std::int32_t menuHandle,
             const std::shared_ptr<PluginPlayerRef>& player, std::int32_t slot) {
    const std::lock_guard<std::recursive_mutex> turn{host.turnMutex()};
    State* state = *link;
    if (state == nullptr) {
        return;
    }
    const Menu* menu = state->menus->find(menuHandle);
    const auto token =
        menu == nullptr ? std::map<std::int32_t, std::int32_t>::const_iterator{} : menu->tokens.find(slot);
    if (menu == nullptr || token == menu->tokens.end()) {
        return;
    }
    Context context;
    context.kind = Context::Kind::MenuClick;
    context.player = state->playerHandle(player);
    static_cast<void>(state->run(token->second, context, "menu click"));
}

void menuOpen(void* user, abi::Call& c) {
    State& state = stateOf(user);
    Menu* menu = state.menus->find(c.i32(0));
    if (menu == nullptr) {
        c.ret(abi::kErrInvalid);
        return;
    }
    std::shared_ptr<PluginPlayerRef> player;
    const std::int32_t status = state.resolvePlayer(c.i32(1), player);
    if (status != abi::kOk) {
        c.ret(status);
        return;
    }
    const std::string uuid = player->uuid();
    if (const auto opened = menu->opened.find(uuid); opened != menu->opened.end()) {
        opened->second.remove();
    }
    PluginHost* host = state.host;
    const std::shared_ptr<State*> link = state.link;
    const std::int32_t handle = c.i32(0);
    menu->opened.insert_or_assign(uuid, host->services().menus().open(*player, menu->title, menu->rows, menu->items,
                                                                      [host, link, handle, player](std::int32_t slot) {
                                                                          clicked(*host, link, handle, player, slot);
                                                                      }));
    c.ret(abi::kOk);
}

void menuClickPlayer(void* user, abi::Call& c) {
    Context* context = nullptr;
    const std::int32_t status = stateOf(user).check(c.i32(0), Context::Kind::MenuClick, context);
    c.ret(status != abi::kOk ? status : context->player);
}
} // namespace

void addUiImports(State& state, std::vector<abi::HostImport>& out) {
    State* user = &state;
    out.insert(out.end(), {
                              {"command_register", commandRegister, user},
                              {"command_source_name", commandSourceName, user},
                              {"command_is_console", commandIsConsole, user},
                              {"command_reply", commandReply, user},
                              {"command_arg_str", commandArgStr, user},
                              {"command_arg_i64", commandArgI64, user},
                              {"menu_create", menuCreate, user},
                              {"menu_set", menuSet, user},
                              {"menu_open", menuOpen, user},
                              {"menu_click_player", menuClickPlayer, user},
                          });
}
} // namespace basalt::server::plugins::wasm
