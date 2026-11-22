#include "WasmServerSupport.hpp"

#include <algorithm>

namespace basalt::server::plugins::wasm {
namespace {
using namespace server;

[[nodiscard]] std::string keyOf(const PluginCommandSpec& spec) {
    std::string key;
    for (const std::string& literal : spec.literals) {
        key += key.empty() ? literal : " " + literal;
    }
    return key;
}

void dispatch(PluginHost& host, const std::shared_ptr<State*>& link, std::int32_t token, const std::string& why,
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

[[nodiscard]] std::vector<std::string> complete(PluginHost& host, const std::shared_ptr<State*>& link,
                                                std::int32_t token, PluginCommandCall& call, std::string_view input) {
    if (insideImport() || !host.onPluginThread()) {
        return {};
    }
    const std::lock_guard<std::recursive_mutex> turn{host.turnMutex()};
    State* live = *link;
    if (live == nullptr || live->depth > 0) {
        return {};
    }
    const Value partial{std::string{input}};
    std::string joined;
    Context context;
    context.kind = Context::Kind::Command;
    context.command = &call;
    context.value = &partial;
    context.result = &joined;
    static_cast<void>(live->run(token, context, "completion"));
    std::vector<std::string> candidates;
    for (std::size_t at = 0; at < joined.size();) {
        const std::size_t end = joined.find('\n', at);
        candidates.push_back(joined.substr(at, end == std::string::npos ? std::string::npos : end - at));
        at = end == std::string::npos ? joined.size() : end + 1;
    }
    return candidates;
}

[[nodiscard]] bool add(State& state, std::string origin, PluginCommandSpec spec, std::int32_t token,
                       std::int32_t completeToken) {
    const std::string key = keyOf(spec);
    if (state.plugin->commandNamed(key) != nullptr) {
        return false;
    }
    PluginHost* host = state.host;
    LoadedPlugin* plugin = state.plugin;
    const std::shared_ptr<State*> link = state.link;
    plugin->commands.emplace_back(key, spec);
    const std::string why = "/" + spec.name();
    auto run = [host, link, token, why](PluginCommandCall& call) { dispatch(*host, link, token, why, call); };
    PluginHandle handle = completeToken < 0
                              ? host->services().commands().registerCommand(plugin->manifest.id, spec, std::move(run))
                              : host->services().commands().registerCompletingCommand(
                                    plugin->manifest.id, spec, std::move(run),
                                    [host, link, completeToken](PluginCommandCall& call, std::string_view input) {
                                        return complete(*host, link, completeToken, call, input);
                                    });
    PluginHandle registered =
        plugin->registrations.add("command /" + spec.usage(), [plugin, key, moved = std::move(handle)]() mutable {
            moved.remove();
            std::erase_if(plugin->commands, [&key](const auto& held) { return held.first == key; });
        });
    area(state).commands.push_back(CommandEntry{.name = spec.name(),
                                                .origin = std::move(origin),
                                                .spec = std::move(spec),
                                                .token = token,
                                                .completeToken = completeToken,
                                                .registration = std::move(registered)});
    return true;
}

void commandRegisterEx(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const abi::GuestMemory memory = c.memory();
    const std::optional<std::string_view> spec = memory.string(c.i32(0), c.i32(1));
    const std::optional<std::string_view> permission = memory.string(c.i32(2), c.i32(3));
    if (!spec || !permission) {
        c.ret(abi::kErrInvalid);
        return;
    }
    const std::expected<PluginCommandSpec, wire::plugins::ManifestError> parsed = PluginCommandSpec::parse(
        *spec, permission->empty() ? std::nullopt : std::optional<std::string>{std::string{*permission}});
    if (!parsed.has_value()) {
        state.plugin->log.warn([&parsed] { return "command_register_ex: " + parsed.error().message; });
        c.ret(abi::kErrInvalid);
        return;
    }
    c.ret(add(state, {}, *parsed, c.i32(4), c.i32(5)) ? abi::kOk : abi::kErrInvalid);
}

void commandAlias(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const abi::GuestMemory memory = c.memory();
    const std::optional<std::string_view> name = memory.string(c.i32(0), c.i32(1));
    const std::optional<std::string_view> alias = memory.string(c.i32(2), c.i32(3));
    if (!name || !alias || alias->empty() || alias->find(' ') != std::string_view::npos) {
        c.ret(abi::kErrInvalid);
        return;
    }
    std::vector<CommandEntry> sources;
    for (const CommandEntry& entry : area(state).commands) {
        if (entry.name == *name && entry.origin.empty()) {
            sources.push_back(CommandEntry{.name = entry.name,
                                           .origin = {},
                                           .spec = entry.spec,
                                           .token = entry.token,
                                           .completeToken = entry.completeToken,
                                           .registration = {}});
        }
    }
    if (sources.empty()) {
        c.ret(abi::kErrAbsent);
        return;
    }
    for (const CommandEntry& source : sources) {
        PluginCommandSpec spec = source.spec;
        spec.literals.front() = std::string{*alias};
        if (!spec.permission) {
            spec.permission = "basalt.plugin." + state.plugin->manifest.id + "." + source.name;
        }
        if (state.plugin->commandNamed(keyOf(spec)) != nullptr) {
            c.ret(abi::kErrInvalid);
            return;
        }
    }
    for (const CommandEntry& source : sources) {
        PluginCommandSpec spec = source.spec;
        spec.literals.front() = std::string{*alias};
        if (!spec.permission) {
            spec.permission = "basalt.plugin." + state.plugin->manifest.id + "." + source.name;
        }
        static_cast<void>(add(state, std::string{*name}, std::move(spec), source.token, source.completeToken));
    }
    c.ret(abi::kOk);
}

void commandUnregister(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const std::optional<std::string_view> name = c.memory().string(c.i32(0), c.i32(1));
    if (!name || name->empty()) {
        c.ret(abi::kErrInvalid);
        return;
    }
    std::vector<CommandEntry>& all = area(state).commands;
    std::vector<PluginHandle> doomed;
    std::erase_if(all, [&](CommandEntry& entry) {
        if (entry.name != *name && entry.origin != *name) {
            return false;
        }
        doomed.push_back(std::move(entry.registration));
        return true;
    });
    for (PluginHandle& handle : doomed) {
        handle.remove();
    }
    c.ret(doomed.empty() ? abi::kErrAbsent : static_cast<std::int32_t>(doomed.size()));
}

void commandRun(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const std::optional<std::string_view> line = c.memory().string(c.i32(0), c.i32(1));
    if (!state.capable("commands")) {
        c.ret(abi::kErrDenied);
        return;
    }
    std::shared_ptr<PluginPlayerRef> player;
    const std::int32_t status = c.i32(2) == 0 ? abi::kOk : state.resolvePlayer(c.i32(2), player);
    if (status != abi::kOk || !line || line->empty()) {
        c.ret(status != abi::kOk ? status : abi::kErrInvalid);
        return;
    }
    static_cast<void>(runSoon(state, "command_run", [text = std::string{*line}, player](State& live) {
        PluginCommands& commands = live.host->services().commands();
        static_cast<void>(player == nullptr ? commands.runAsConsole(text) : commands.runAsPlayer(*player, text));
    }));
    c.ret(1);
}

[[nodiscard]] std::int32_t completionContext(State& state, std::int32_t id, Context*& context) {
    const std::int32_t status = state.check(id, Context::Kind::Command, context);
    return status != abi::kOk ? status
                              : (context->value != nullptr && context->result != nullptr ? abi::kOk : abi::kErrInvalid);
}

void commandCompleteInput(void* user, abi::Call& c) {
    Context* context = nullptr;
    const std::int32_t status = completionContext(stateOf(user), c.i32(0), context);
    c.ret(status != abi::kOk ? status : c.memory().put(c.i32(1), c.i32(2), textOf(context->value)));
}

void commandCompleteAdd(void* user, abi::Call& c) {
    const std::optional<std::string_view> text = c.memory().string(c.i32(1), c.i32(2));
    Context* context = nullptr;
    std::int32_t status = completionContext(stateOf(user), c.i32(0), context);
    if (status == abi::kOk && (!text || text->empty() || text->find('\n') != std::string_view::npos)) {
        status = abi::kErrInvalid;
    }
    if (status == abi::kOk) {
        context->result->append(context->result->empty() ? "" : "\n");
        context->result->append(*text);
    }
    c.ret(status);
}

[[nodiscard]] PluginCommandCall::Argument argumentOf(State& state, abi::Call& c, std::int32_t& status) {
    Context* context = nullptr;
    status = state.check(c.i32(0), Context::Kind::Command, context);
    const std::optional<std::string_view> name = c.memory().string(c.i32(1), c.i32(2));
    if (status != abi::kOk || !name) {
        status = status != abi::kOk ? status : abi::kErrInvalid;
        return {};
    }
    PluginCommandCall::Argument value = context->command->argument(*name);
    status = std::holds_alternative<std::monostate>(value) ? abi::kErrAbsent : abi::kOk;
    return value;
}

void commandArgF64(void* user, abi::Call& c) {
    std::int32_t status = abi::kOk;
    const PluginCommandCall::Argument value = argumentOf(stateOf(user), c, status);
    if (status != abi::kOk) {
        c.ret(status);
        return;
    }
    double number = 0.0;
    if (const auto* real = std::get_if<double>(&value)) {
        number = *real;
    } else if (const auto* whole = std::get_if<std::int64_t>(&value)) {
        number = static_cast<double>(*whole);
    } else {
        c.ret(abi::kErrInvalid);
        return;
    }
    c.ret(c.memory().write(c.i32(3), number) ? abi::kOk : abi::kErrInvalid);
}

void commandArgBool(void* user, abi::Call& c) {
    std::int32_t status = abi::kOk;
    const PluginCommandCall::Argument value = argumentOf(stateOf(user), c, status);
    if (status != abi::kOk) {
        c.ret(status);
        return;
    }
    const auto* flag = std::get_if<bool>(&value);
    c.ret(flag != nullptr && c.memory().write(c.i32(3), std::int32_t{*flag ? 1 : 0}) ? abi::kOk : abi::kErrInvalid);
}

void commandSource(void* user, abi::Call& c) {
    State& state = stateOf(user);
    Context* context = nullptr;
    const std::int32_t status = state.check(c.i32(0), Context::Kind::Command, context);
    if (status != abi::kOk) {
        c.ret(status);
        return;
    }
    const PluginCommandCall& call = *context->command;
    const std::array<double, 3> at = call.position();
    c.ret(returnValue(c, 1,
                      mapOf({{"name", Value{call.source()}},
                             {"console", Value{call.isConsole()}},
                             {"kind", Value{call.kind()}},
                             {"world", Value{call.world()}},
                             {"x", Value{at[0]}},
                             {"y", Value{at[1]}},
                             {"z", Value{at[2]}},
                             {"caller", Value{call.caller()}}})));
}

void commandSourcePlayer(void* user, abi::Call& c) {
    State& state = stateOf(user);
    Context* context = nullptr;
    const std::int32_t status = state.check(c.i32(0), Context::Kind::Command, context);
    if (status != abi::kOk) {
        c.ret(status);
        return;
    }
    const std::shared_ptr<PluginPlayerRef> player =
        context->command->isConsole() ? nullptr : state.host->services().players().byName(context->command->source());
    c.ret(player != nullptr && player->online() ? state.playerHandle(player) : 0);
}
} // namespace

void addServerCommandsImports(State& state, std::vector<abi::HostImport>& out) {
    void* user = &state;
    out.insert(out.end(), {
                              {"command_register_ex", commandRegisterEx, user},
                              {"command_complete_input", commandCompleteInput, user},
                              {"command_complete_add", commandCompleteAdd, user},
                              {"command_alias", commandAlias, user},
                              {"command_unregister", commandUnregister, user},
                              {"command_run", commandRun, user},
                              {"command_arg_f64", commandArgF64, user},
                              {"command_arg_bool", commandArgBool, user},
                              {"command_source", commandSource, user},
                              {"command_source_player", commandSourcePlayer, user},
                          });
}
} // namespace basalt::server::plugins::wasm
