#include "WasmState.hpp"

#include <cstdint>
#include <memory>

namespace basalt::server::plugins::wasm {
namespace {
using wire::common::toml::TomlValue;

void log(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const std::optional<std::string_view> message = c.memory().string(c.i32(1), c.i32(2));
    if (!message) {
        return;
    }
    const std::int32_t level = c.i32(0);
    const wire::common::Logger& out = state.plugin->log;
    if (level <= 0) {
        out.debug([&message] { return std::string{*message}; });
    } else if (level == 1) {
        out.info([&message] { return std::string{*message}; });
    } else if (level == 2) {
        out.warn([&message] { return std::string{*message}; });
    } else {
        out.error([&message] { return std::string{*message}; });
    }
}

[[nodiscard]] const TomlValue* configAt(State& state, const abi::Call& c) {
    const std::optional<std::string_view> key = c.memory().string(c.i32(0), c.i32(1));
    return key ? configValue(state.plugin->config, *key) : nullptr;
}

void configI64(void* user, abi::Call& c) {
    const TomlValue* value = configAt(stateOf(user), c);
    const std::int64_t* found = value != nullptr ? value->get<std::int64_t>() : nullptr;
    c.ret(found != nullptr ? *found : c.i64(2));
}

void configF64(void* user, abi::Call& c) {
    const TomlValue* value = configAt(stateOf(user), c);
    double result = c.f64(2);
    if (value != nullptr) {
        if (const double* real = value->get<double>()) {
            result = *real;
        } else if (const std::int64_t* whole = value->get<std::int64_t>()) {
            result = static_cast<double>(*whole);
        }
    }
    c.ret(result);
}

void configBool(void* user, abi::Call& c) {
    const TomlValue* value = configAt(stateOf(user), c);
    const bool* found = value != nullptr ? value->get<bool>() : nullptr;
    c.ret(found != nullptr ? (*found ? 1 : 0) : c.i32(2));
}

void configStr(void* user, abi::Call& c) {
    const abi::GuestMemory memory = c.memory();
    if (!memory.string(c.i32(0), c.i32(1))) {
        c.ret(abi::kErrInvalid);
        return;
    }
    const TomlValue* value = configAt(stateOf(user), c);
    const std::string* text = value != nullptr ? value->get<std::string>() : nullptr;
    c.ret(text == nullptr ? abi::kErrAbsent : memory.put(c.i32(2), c.i32(3), *text));
}

void translate(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const abi::GuestMemory memory = c.memory();
    const std::optional<std::string_view> key = memory.string(c.i32(0), c.i32(1));
    const std::optional<ArgumentList> arguments = readArguments(memory, c.i32(2), c.i32(3));
    if (!key || !arguments) {
        c.ret(abi::kErrInvalid);
        return;
    }
    const std::string* found = state.plugin->text.find(*key);
    if (found == nullptr) {
        c.ret(abi::kErrAbsent);
        return;
    }
    substitute(*found, arguments->view(), state.scratch);
    c.ret(memory.put(c.i32(4), c.i32(5), state.scratch));
}

[[nodiscard]] std::string_view canonicalEventName(std::string_view name) {
    if (name == "player.leave") {
        return "player.quit";
    }
    return name;
}

void dispatchEvent(PluginHost& host, const std::shared_ptr<State*>& link, std::int32_t token, PluginEvent& event) {
    const std::lock_guard<std::recursive_mutex> turn{host.turnMutex()};
    State* state = *link;
    if (state == nullptr) {
        return;
    }
    if (!host.onPluginThread()) {
        if (!state->warnedAboutThread) {
            state->warnedAboutThread = true;
            state->plugin->log.warn([&event] {
                return "a " + event.type().name() + " was fired from a thread plugins do not run on, so it was skipped";
            });
        }
        return;
    }
    Context context;
    context.kind = Context::Kind::Event;
    context.event = &event;
    static_cast<void>(state->run(token, context, event.type().name()));
}

void eventOn(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const std::optional<std::string_view> given = c.memory().string(c.i32(0), c.i32(1));
    const std::int32_t priority = c.i32(2);
    const std::int32_t token = c.i32(3);
    if (!given || priority < 0 || priority > 4) {
        c.ret(abi::kErrInvalid);
        return;
    }
    const std::string_view name = canonicalEventName(*given);
    if (name == "plugin.enable" || name == "plugin.disable") {
        auto& list = name == "plugin.enable" ? state.enable : state.disable;
        list.emplace_back(priority, token);
        c.ret(abi::kOk);
        return;
    }
    const PluginEventType* type = state.host->events().typeOf(name);
    if (type == nullptr) {
        c.ret(abi::kErrInvalid);
        return;
    }
    PluginHost* host = state.host;
    const std::shared_ptr<State*> link = state.link;
    PluginHandle handle = host->events().add(
        *type, state.plugin->manifest.id, 0, static_cast<PluginEventPriority>(priority), false, false, {},
        [host, link, token](PluginEvent& event) { dispatchEvent(*host, link, token, event); });
    static_cast<void>(state.plugin->registrations.add("on(" + std::string{name} + ")",
                                                      [moved = std::move(handle)]() mutable { moved.remove(); }));
    c.ret(abi::kOk);
}

[[nodiscard]] std::string_view fieldNameOf(std::string_view field) {
    if (field == "name") {
        return "player_name";
    }
    if (field == "uuid") {
        return "player_uuid";
    }
    return field;
}

[[nodiscard]] const PluginEventValue* fieldValue(const PluginEvent& event, std::string_view field) {
    const std::string_view real = fieldNameOf(field);
    return event.type().indexOf(real) >= 0 ? &event.get(real) : nullptr;
}

void eventPlayer(void* user, abi::Call& c) {
    State& state = stateOf(user);
    Context* context = nullptr;
    const std::int32_t status = state.check(c.i32(0), Context::Kind::Event, context);
    if (status != abi::kOk) {
        c.ret(status);
        return;
    }
    if (context->event == nullptr || context->event->type().indexOf("player_uuid") < 0) {
        c.ret(0);
        return;
    }
    std::shared_ptr<PluginPlayerRef> player;
    if (const auto* uuid = std::get_if<std::string>(&context->event->get("player_uuid"))) {
        player = state.host->services().players().byUuid(*uuid);
    }
    if (player == nullptr && context->event->type().indexOf("player_name") >= 0) {
        if (const auto* name = std::get_if<std::string>(&context->event->get("player_name"))) {
            player = state.host->services().players().byName(*name);
        }
    }
    c.ret(player == nullptr ? 0 : state.playerHandle(player));
}

void eventFieldStr(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const abi::GuestMemory memory = c.memory();
    const std::optional<std::string_view> field = memory.string(c.i32(1), c.i32(2));
    Context* context = nullptr;
    const std::int32_t status = state.check(c.i32(0), Context::Kind::Event, context);
    if (status != abi::kOk || !field) {
        c.ret(status != abi::kOk ? status : abi::kErrInvalid);
        return;
    }
    const PluginEventValue* value = context->event == nullptr ? nullptr : fieldValue(*context->event, *field);
    if (value == nullptr || std::holds_alternative<std::monostate>(*value)) {
        c.ret(abi::kErrAbsent);
        return;
    }
    const std::string* text = std::get_if<std::string>(value);
    c.ret(text == nullptr ? abi::kErrInvalid : memory.put(c.i32(3), c.i32(4), *text));
}

[[nodiscard]] std::int64_t onlineCount(State& state, const PluginEvent& event) {
    const std::string* leaving = event.type().name() == "player.quit" && event.type().indexOf("player_uuid") >= 0
                                     ? std::get_if<std::string>(&event.get("player_uuid"))
                                     : nullptr;
    std::int64_t count = 0;
    for (const std::shared_ptr<PluginPlayerRef>& player : state.host->services().players().online()) {
        if (player->online() && (leaving == nullptr || player->uuid() != *leaving)) {
            ++count;
        }
    }
    return count;
}

void eventFieldI64(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const abi::GuestMemory memory = c.memory();
    const std::optional<std::string_view> field = memory.string(c.i32(1), c.i32(2));
    Context* context = nullptr;
    const std::int32_t status = state.check(c.i32(0), Context::Kind::Event, context);
    if (status != abi::kOk || !field) {
        c.ret(status != abi::kOk ? status : abi::kErrInvalid);
        return;
    }
    if (context->event == nullptr) {
        c.ret(abi::kErrAbsent);
        return;
    }
    std::int64_t number = 0;
    if (*field == "online" && context->event->type().indexOf("player_uuid") >= 0) {
        number = onlineCount(state, *context->event);
    } else {
        const PluginEventValue* value = fieldValue(*context->event, *field);
        if (value == nullptr || std::holds_alternative<std::monostate>(*value)) {
            c.ret(abi::kErrAbsent);
            return;
        }
        if (const auto* whole = std::get_if<std::int64_t>(value)) {
            number = *whole;
        } else if (const auto* flag = std::get_if<bool>(value)) {
            number = *flag ? 1 : 0;
        } else {
            c.ret(abi::kErrInvalid);
            return;
        }
    }
    c.ret(memory.write(c.i32(3), number) ? abi::kOk : abi::kErrInvalid);
}

void eventCancel(void* user, abi::Call& c) {
    State& state = stateOf(user);
    Context* context = nullptr;
    const std::int32_t status = state.check(c.i32(0), Context::Kind::Event, context);
    if (status != abi::kOk) {
        c.ret(status);
        return;
    }
    if (context->event == nullptr || !context->event->type().cancellable()) {
        c.ret(abi::kErrInvalid);
        return;
    }
    if (c.i32(1) != 0) {
        context->event->cancel();
    } else {
        context->event->restore();
    }
    c.ret(abi::kOk);
}

void fireTask(PluginHost& host, const std::shared_ptr<State*>& link, std::int32_t token,
              const std::shared_ptr<std::int32_t>& slot, bool once) {
    const std::lock_guard<std::recursive_mutex> turn{host.turnMutex()};
    State* state = *link;
    if (state == nullptr) {
        return;
    }
    Context context;
    context.kind = Context::Kind::Task;
    static_cast<void>(state->run(token, context, "task"));
    if (once) {
        state->tasks.cancel(*slot);
    }
}

[[nodiscard]] std::int32_t scheduleTask(State& state, std::int32_t delay, std::int32_t period, std::int32_t token) {
    PluginHost* host = state.host;
    const std::shared_ptr<State*> link = state.link;
    const auto slot = std::make_shared<std::int32_t>(0);
    const bool once = period <= 0;
    std::function<void()> body = [host, link, token, slot, once] { fireTask(*host, link, token, slot, once); };
    PluginHandle handle = once ? host->services().tasks().after(delay, std::move(body))
                               : host->services().tasks().every(delay, period, std::move(body));
    PluginHandle registered = state.plugin->registrations.add(
        once ? "later(" + std::to_string(delay) + ")" : "every(" + std::to_string(period) + ")",
        [moved = std::move(handle)]() mutable { moved.remove(); });
    *slot = state.tasks.add(std::move(registered));
    return *slot;
}

void taskAfter(void* user, abi::Call& c) {
    c.ret(c.i32(0) < 0 ? abi::kErrInvalid : scheduleTask(stateOf(user), c.i32(0), 0, c.i32(1)));
}

void taskEvery(void* user, abi::Call& c) {
    c.ret(c.i32(0) < 0 || c.i32(1) <= 0 ? abi::kErrInvalid : scheduleTask(stateOf(user), c.i32(0), c.i32(1), c.i32(2)));
}

void taskCancel(void* user, abi::Call& c) { c.ret(stateOf(user).tasks.cancel(c.i32(0)) ? abi::kOk : abi::kErrAbsent); }
} // namespace

void addCoreImports(State& state, std::vector<abi::HostImport>& out) {
    State* user = &state;
    out.insert(out.end(), {
                              {"log", log, user},
                              {"config_i64", configI64, user},
                              {"config_f64", configF64, user},
                              {"config_bool", configBool, user},
                              {"config_str", configStr, user},
                              {"translate", translate, user},
                              {"event_on", eventOn, user},
                              {"event_player", eventPlayer, user},
                              {"event_field_str", eventFieldStr, user},
                              {"event_field_i64", eventFieldI64, user},
                              {"event_cancel", eventCancel, user},
                              {"task_after", taskAfter, user},
                              {"task_every", taskEvery, user},
                              {"task_cancel", taskCancel, user},
                          });
}
} // namespace basalt::server::plugins::wasm
