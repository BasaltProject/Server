#include "WasmEventValues.hpp"

#include <algorithm>

namespace basalt::server::plugins::wasm {
namespace {
constexpr std::int32_t kFlagPost = 1;
constexpr std::int32_t kFlagCancelled = 2;
constexpr std::int32_t kMaxEmitDepth = 8;

struct Emission {
    const Value* payload = nullptr;
    std::int32_t heard = 0;
};

thread_local std::vector<Emission*> gEmissions;

[[nodiscard]] bool isPluginEvent(std::string_view name) { return name.find(':') != std::string_view::npos; }

[[nodiscard]] std::string_view fieldNameOf(std::string_view field) {
    if (field == "name") {
        return "player_name";
    }
    if (field == "uuid") {
        return "player_uuid";
    }
    return field;
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

/// The event of a live event ctx, or the ABI error.
[[nodiscard]] std::int32_t eventOf(State& state, std::int32_t id, PluginEvent*& event, Context*& context) {
    const std::int32_t status = state.check(id, Context::Kind::Event, context);
    if (status == abi::kOk) {
        event = context->event;
    }
    return status != abi::kOk ? status : (event == nullptr ? abi::kErrInvalid : abi::kOk);
}

void deliver(PluginHost& host, const std::shared_ptr<State*>& link, std::int32_t token, PluginEvent& event, bool post,
             const Value* payload) {
    const std::lock_guard<std::recursive_mutex> turn{host.turnMutex()};
    State* state = *link;
    if (state == nullptr || state->depth > 0) {
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
    PluginEvent copy{event};
    Context context;
    context.kind = Context::Kind::Event;
    context.event = post ? &copy : &event;
    context.value = payload;
    EventsArea& area = eventsArea(*state);
    area.post = post ? &copy : nullptr;
    const bool ran = state->run(token, context, event.type().name());
    area.post = nullptr;
    if (ran && !gEmissions.empty() && payload != nullptr) {
        ++gEmissions.back()->heard;
    }
}

[[nodiscard]] std::int32_t readFilters(const PluginEventType& type, const abi::GuestMemory& memory, std::int32_t ptr,
                                       std::int32_t len,
                                       std::map<std::string, PluginEventValue, std::less<>>& filters) {
    if (len == 0) {
        return abi::kOk;
    }
    const auto document = readValue(memory, ptr, len);
    if (!document) {
        return document.error();
    }
    const Value::Map* entries = document->asMap();
    if (entries == nullptr) {
        return abi::kErrInvalid;
    }
    for (const MapEntry& entry : *entries) {
        const std::optional<PluginEventValue> expected = eventValueOf(entry.value);
        if (type.indexOf(entry.key) < 0 || !expected || std::holds_alternative<std::monostate>(*expected)) {
            return abi::kErrInvalid;
        }
        filters.emplace(entry.key, *expected);
    }
    return abi::kOk;
}

void eventOnEx(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const abi::GuestMemory memory = c.memory();
    const std::optional<std::string_view> name = memory.string(c.i32(0), c.i32(1));
    const std::int32_t priority = c.i32(2);
    const std::int32_t flags = c.i32(3);
    const std::int32_t token = c.i32(6);
    if (!name || priority < 0 || priority > 4 || (flags & ~(kFlagPost | kFlagCancelled)) != 0) {
        c.ret(abi::kErrInvalid);
        return;
    }
    const bool plugin = isPluginEvent(*name);
    if (plugin && (flags != 0 || c.i32(5) != 0)) {
        c.ret(abi::kErrInvalid);
        return;
    }
    PluginEventBus& bus = state.host->events();
    const PluginEventType* type = bus.typeOf(*name);
    if (type == nullptr && plugin) {
        bus.declare(PluginEventType{std::string{*name}, false, {}});
        type = bus.typeOf(*name);
    }
    if (type == nullptr || ((flags & kFlagCancelled) != 0 && !type->cancellable())) {
        c.ret(abi::kErrInvalid);
        return;
    }
    std::map<std::string, PluginEventValue, std::less<>> filters;
    if (const std::int32_t status = readFilters(*type, memory, c.i32(4), c.i32(5), filters); status != abi::kOk) {
        c.ret(status);
        return;
    }
    PluginHost* host = state.host;
    const std::shared_ptr<State*> link = state.link;
    const bool post = (flags & kFlagPost) != 0;
    PluginHandle handle = bus.add(
        *type, state.plugin->manifest.id, 0, static_cast<PluginEventPriority>(priority), post,
        (flags & kFlagCancelled) != 0, std::move(filters), [host, link, token, post, plugin](PluginEvent& event) {
            const Value* payload = plugin && !gEmissions.empty() ? gEmissions.back()->payload : nullptr;
            deliver(*host, link, token, event, post, payload);
        });
    static_cast<void>(state.plugin->registrations.add("on(" + std::string{*name} + ")",
                                                      [moved = std::move(handle)]() mutable { moved.remove(); }));
    c.ret(abi::kOk);
}

void eventName(void* user, abi::Call& c) {
    PluginEvent* event = nullptr;
    Context* context = nullptr;
    const std::int32_t status = eventOf(stateOf(user), c.i32(0), event, context);
    c.ret(status != abi::kOk ? status : c.memory().put(c.i32(1), c.i32(2), event->type().name()));
}

[[nodiscard]] std::optional<Value> fieldValue(State& state, const PluginEvent& event, std::string_view field) {
    if (field == "online" && event.type().indexOf("player_uuid") >= 0) {
        return Value{onlineCount(state, event)};
    }
    const std::string_view real = fieldNameOf(field);
    if (event.type().indexOf(real) < 0) {
        return std::nullopt;
    }
    return valueOf(event.get(real));
}

void eventGet(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const abi::GuestMemory memory = c.memory();
    const std::optional<std::string_view> field = memory.string(c.i32(1), c.i32(2));
    PluginEvent* event = nullptr;
    Context* context = nullptr;
    const std::int32_t status = eventOf(state, c.i32(0), event, context);
    if (status != abi::kOk || !field) {
        c.ret(status != abi::kOk ? status : abi::kErrInvalid);
        return;
    }
    const std::optional<Value> value = fieldValue(state, *event, *field);
    c.ret(value ? putValue(memory, c.i32(3), c.i32(4), *value) : abi::kErrAbsent);
}

/// What a field holding `current` accepts for `given`, with an integer widened to a decimal on request.
[[nodiscard]] std::optional<PluginEventValue> fitted(const PluginEventValue& current, const PluginEventValue& given) {
    if (std::holds_alternative<std::monostate>(given) || std::holds_alternative<std::monostate>(current) ||
        current.index() == given.index()) {
        return given;
    }
    if (std::holds_alternative<double>(current)) {
        if (const auto* whole = std::get_if<std::int64_t>(&given)) {
            return PluginEventValue{static_cast<double>(*whole)};
        }
    }
    if (std::holds_alternative<std::int64_t>(current)) {
        if (const auto* real = std::get_if<double>(&given);
            real != nullptr && std::trunc(*real) == *real && std::abs(*real) < 9.0e18) {
            return PluginEventValue{static_cast<std::int64_t>(*real)};
        }
    }
    return std::nullopt;
}

void eventSet(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const abi::GuestMemory memory = c.memory();
    const std::optional<std::string_view> field = memory.string(c.i32(1), c.i32(2));
    PluginEvent* event = nullptr;
    Context* context = nullptr;
    std::int32_t status = eventOf(state, c.i32(0), event, context);
    if (status != abi::kOk || !field) {
        c.ret(status != abi::kOk ? status : abi::kErrInvalid);
        return;
    }
    const auto document = readValue(memory, c.i32(3), c.i32(4));
    const std::optional<PluginEventValue> given = document ? eventValueOf(*document) : std::nullopt;
    if (!given || context->value != nullptr || event == eventsArea(state).post || !event->type().isMutable(*field)) {
        c.ret(abi::kErrInvalid);
        return;
    }
    const std::optional<PluginEventValue> stored = fitted(event->get(*field), *given);
    if (!stored) {
        c.ret(abi::kErrInvalid);
        return;
    }
    event->set(*field, *stored);
    c.ret(abi::kOk);
}

void eventFields(void* user, abi::Call& c) {
    State& state = stateOf(user);
    PluginEvent* event = nullptr;
    Context* context = nullptr;
    const std::int32_t status = eventOf(state, c.i32(0), event, context);
    if (status != abi::kOk) {
        c.ret(status);
        return;
    }
    Value::Map map;
    for (const std::string& field : event->type().fields()) {
        map.push_back({field, valueOf(event->get(field))});
    }
    c.ret(putValue(c.memory(), c.i32(1), c.i32(2), Value{std::move(map)}));
}

void eventDescribe(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const abi::GuestMemory memory = c.memory();
    const std::optional<std::string_view> name = memory.string(c.i32(0), c.i32(1));
    if (!name) {
        c.ret(abi::kErrInvalid);
        return;
    }
    const PluginEventType* type = state.host->events().typeOf(*name);
    if (type == nullptr) {
        c.ret(abi::kErrAbsent);
        return;
    }
    Value::List fields;
    for (const std::string& field : type->fields()) {
        fields.emplace_back(Value::Map{{"name", Value{field}}, {"mutable", Value{type->isMutable(field)}}});
    }
    c.ret(putValue(memory, c.i32(2), c.i32(3),
                   Value{Value::Map{{"name", Value{type->name()}},
                                    {"cancellable", Value{type->cancellable()}},
                                    {"custom", Value{isPluginEvent(type->name())}},
                                    {"fields", Value{std::move(fields)}}}}));
}

void eventNames(void* user, abi::Call& c) {
    std::vector<std::string> names;
    for (const PluginEventType* type : stateOf(user).host->events().declared()) {
        if (!isPluginEvent(type->name())) {
            names.push_back(type->name());
        }
    }
    std::ranges::sort(names);
    Value::List list;
    for (std::string& name : names) {
        list.emplace_back(std::move(name));
    }
    c.ret(putValue(c.memory(), c.i32(0), c.i32(1), Value{std::move(list)}));
}

void eventCancelled(void* user, abi::Call& c) {
    PluginEvent* event = nullptr;
    Context* context = nullptr;
    const std::int32_t status = eventOf(stateOf(user), c.i32(0), event, context);
    c.ret(status != abi::kOk ? status : (event->cancelled() ? 1 : 0));
}

void eventIsPost(void* user, abi::Call& c) {
    State& state = stateOf(user);
    PluginEvent* event = nullptr;
    Context* context = nullptr;
    const std::int32_t status = eventOf(state, c.i32(0), event, context);
    c.ret(status != abi::kOk ? status : (event == eventsArea(state).post ? 1 : 0));
}

void eventCancelWith(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const abi::GuestMemory memory = c.memory();
    const std::optional<std::string_view> text = memory.string(c.i32(1), c.i32(2));
    const std::optional<ArgumentList> arguments = readArguments(memory, c.i32(3), c.i32(4));
    PluginEvent* event = nullptr;
    Context* context = nullptr;
    std::int32_t status = eventOf(state, c.i32(0), event, context);
    if (status == abi::kOk &&
        (!text || !arguments || !event->type().cancellable() || event == eventsArea(state).post)) {
        status = abi::kErrInvalid;
    }
    if (status == abi::kOk) {
        if (renderText(state.plugin->text, *text, arguments->view(), state.scratch)) {
            event->cancel(state.scratch);
        } else {
            status = abi::kErrAbsent;
        }
    }
    c.ret(status);
}

void eventCancelMessage(void* user, abi::Call& c) {
    PluginEvent* event = nullptr;
    Context* context = nullptr;
    const std::int32_t status = eventOf(stateOf(user), c.i32(0), event, context);
    if (status != abi::kOk || !event->cancelMessage()) {
        c.ret(status != abi::kOk ? status : abi::kErrAbsent);
        return;
    }
    c.ret(c.memory().put(c.i32(1), c.i32(2), *event->cancelMessage()));
}

void eventEmit(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const abi::GuestMemory memory = c.memory();
    const std::optional<std::string_view> name = memory.string(c.i32(0), c.i32(1));
    const auto payload = readValue(memory, c.i32(2), c.i32(3));
    if (!name || !isPluginEvent(*name) || !payload || gEmissions.size() >= kMaxEmitDepth) {
        c.ret(abi::kErrInvalid);
        return;
    }
    PluginEventBus& bus = state.host->events();
    const PluginEventType* type = bus.typeOf(*name);
    if (type == nullptr || !bus.listening(*type)) {
        c.ret(0);
        return;
    }
    Emission emission{.payload = &*payload};
    gEmissions.push_back(&emission);
    static_cast<void>(bus.fire(bus.event(*type)));
    gEmissions.pop_back();
    c.ret(emission.heard);
}

void eventPayload(void* user, abi::Call& c) {
    PluginEvent* event = nullptr;
    Context* context = nullptr;
    const std::int32_t status = eventOf(stateOf(user), c.i32(0), event, context);
    if (status != abi::kOk || context->value == nullptr) {
        c.ret(status != abi::kOk ? status : abi::kErrInvalid);
        return;
    }
    c.ret(putValue(c.memory(), c.i32(1), c.i32(2), *context->value));
}
} // namespace

void addEventsImports(State& state, std::vector<abi::HostImport>& out) {
    State* user = &state;
    out.insert(out.end(), {
                              {"event_on_ex", eventOnEx, user},
                              {"event_name", eventName, user},
                              {"event_get", eventGet, user},
                              {"event_set", eventSet, user},
                              {"event_fields", eventFields, user},
                              {"event_describe", eventDescribe, user},
                              {"event_names", eventNames, user},
                              {"event_cancelled", eventCancelled, user},
                              {"event_cancel_with", eventCancelWith, user},
                              {"event_cancel_message", eventCancelMessage, user},
                              {"event_is_post", eventIsPost, user},
                              {"event_emit", eventEmit, user},
                              {"event_payload", eventPayload, user},
                          });
}
} // namespace basalt::server::plugins::wasm
