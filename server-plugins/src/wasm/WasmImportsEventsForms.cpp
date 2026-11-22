#include "WasmEventValues.hpp"
#include "WasmValues.hpp"

#include <basalt/server/plugins/PluginEventTypes.hpp>

#include <cstdlib>

namespace basalt::server::plugins::wasm {
namespace {
constexpr std::size_t kMaxFormFields = 64;
constexpr std::size_t kMaxFormLines = 64;

[[nodiscard]] std::optional<std::string> resolved(State& state, std::string_view source) {
    std::string out;
    if (!renderText(state.plugin->text, source, {}, out)) {
        return std::nullopt;
    }
    return out;
}

/// The strings of a text or a list of texts under `key`, each one resolved; nullopt for a missing lang key.
[[nodiscard]] bool readTexts(State& state, const Value& map, std::string_view key, bool resolve,
                             std::vector<std::string>& out) {
    const Value* found = map.find(key);
    if (found == nullptr) {
        return true;
    }
    const auto add = [&](const Value& item) {
        const std::string* text = item.asString();
        if (text == nullptr || out.size() >= kMaxFormLines) {
            return false;
        }
        const std::optional<std::string> shown = resolve ? resolved(state, *text) : std::optional<std::string>{*text};
        if (!shown) {
            return false;
        }
        out.push_back(*shown);
        return true;
    };
    if (const Value::List* list = found->asList()) {
        for (const Value& item : *list) {
            if (!add(item)) {
                return false;
            }
        }
        return true;
    }
    return add(*found);
}

[[nodiscard]] std::optional<double> numberIn(const Value& map, std::string_view key, double fallback) {
    const Value* found = map.find(key);
    if (found == nullptr) {
        return fallback;
    }
    if (const double* real = found->asFloat()) {
        return *real;
    }
    if (const std::int64_t* whole = found->asInt()) {
        return static_cast<double>(*whole);
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<PluginFormField> readField(State& state, const Value& map) {
    PluginFormField field;
    const std::optional<std::string_view> key = mapText(map, "key");
    if (!key || key->empty()) {
        return std::nullopt;
    }
    field.key = *key;
    const std::optional<std::string> label = resolved(state, mapText(map, "label").value_or(*key));
    if (!label) {
        return std::nullopt;
    }
    field.label = *label;
    const std::string_view kind = mapText(map, "type").value_or("text");
    const Value* initial = map.find("initial");
    if (kind == "text") {
        field.kind = PluginFormField::Kind::Text;
        field.initialText = initial != nullptr && initial->asString() != nullptr ? *initial->asString() : "";
    } else if (kind == "toggle") {
        field.kind = PluginFormField::Kind::Toggle;
        field.initialToggle = initial != nullptr && initial->asBool() != nullptr && *initial->asBool();
    } else if (kind == "slider") {
        field.kind = PluginFormField::Kind::Slider;
        const std::optional<double> low = numberIn(map, "min", 0.0);
        const std::optional<double> high = numberIn(map, "max", 1.0);
        const std::optional<double> step = numberIn(map, "step", 0.0);
        if (!low || !high || !step || *high < *low) {
            return std::nullopt;
        }
        const std::optional<double> start = numberIn(map, "initial", (*low + *high) / 2.0);
        if (!start) {
            return std::nullopt;
        }
        field.min = *low;
        field.max = *high;
        field.step = *step;
        field.initialNumber = *start;
    } else if (kind == "choice") {
        field.kind = PluginFormField::Kind::Choice;
        if (!readTexts(state, map, "options", false, field.options) || field.options.empty()) {
            return std::nullopt;
        }
        field.initialOption =
            initial != nullptr && initial->asString() != nullptr ? *initial->asString() : field.options.front();
    } else {
        return std::nullopt;
    }
    return field;
}

[[nodiscard]] std::optional<PluginForm> readForm(State& state, const Value& map) {
    PluginForm form;
    const std::optional<std::string> title = resolved(state, mapText(map, "title").value_or(""));
    if (!map.asMap() || !title || !readTexts(state, map, "body", true, form.body) ||
        !readTexts(state, map, "buttons", true, form.buttons)) {
        return std::nullopt;
    }
    form.title = *title;
    if (form.buttons.empty()) {
        form.buttons.push_back(resolved(state, "OK").value_or("OK"));
    }
    if (const Value* fields = map.find("fields")) {
        const Value::List* list = fields->asList();
        if (list == nullptr || list->size() > kMaxFormFields) {
            return std::nullopt;
        }
        for (const Value& item : *list) {
            std::optional<PluginFormField> field = item.asMap() != nullptr ? readField(state, item) : std::nullopt;
            if (!field) {
                return std::nullopt;
            }
            form.fields.push_back(std::move(*field));
        }
    }
    return form;
}

[[nodiscard]] Value answerValue(State& state, const std::shared_ptr<PluginPlayerRef>& player, const PluginForm& form,
                                std::size_t button, const std::map<std::string, std::string>& values) {
    Value::Map answers;
    for (const PluginFormField& field : form.fields) {
        const auto found = values.find(field.key);
        if (found == values.end()) {
            continue;
        }
        switch (field.kind) {
        case PluginFormField::Kind::Toggle:
            answers.push_back({field.key, Value{found->second == "true"}});
            break;
        case PluginFormField::Kind::Slider:
            answers.push_back({field.key, Value{std::strtod(found->second.c_str(), nullptr)}});
            break;
        case PluginFormField::Kind::Text:
        case PluginFormField::Kind::Choice:
            answers.push_back({field.key, Value{found->second}});
            break;
        }
    }
    return Value{Value::Map{{"player", Value{static_cast<std::int64_t>(state.playerHandle(player))}},
                            {"button", Value{static_cast<std::int64_t>(button)}},
                            {"values", Value{std::move(answers)}}}};
}

void formOpen(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const abi::GuestMemory memory = c.memory();
    const auto document = readValue(memory, c.i32(1), c.i32(2));
    std::shared_ptr<PluginPlayerRef> player;
    const std::int32_t status = state.resolvePlayer(c.i32(0), player);
    if (status != abi::kOk) {
        c.ret(status);
        return;
    }
    std::optional<PluginForm> form = document ? readForm(state, *document) : std::nullopt;
    if (!form) {
        c.ret(abi::kErrInvalid);
        return;
    }
    EventsArea& area = eventsArea(state);
    const std::string uuid = player->uuid();
    if (const auto open = area.forms.find(uuid); open != area.forms.end()) {
        static_cast<void>(requests(state).remove(open->second));
        area.forms.erase(open);
    }
    const std::int32_t request = beginRequest(state, c.i32(3));
    if (request == 0) {
        c.ret(abi::kErrInvalid);
        return;
    }
    area.forms.insert_or_assign(uuid, request);
    PluginHost* host = state.host;
    const std::shared_ptr<State*> link = state.link;
    PluginHandle handle = host->services().menus().openForm(
        *player, *form,
        [host, link, request, uuid, player, shown = *form](std::size_t button,
                                                           const std::map<std::string, std::string>& values) {
            const std::lock_guard<std::recursive_mutex> turn{host->turnMutex()};
            State* target = *link;
            if (target == nullptr) {
                return;
            }
            EventsArea& pending = eventsArea(*target);
            if (const auto open = pending.forms.find(uuid); open != pending.forms.end() && open->second == request) {
                pending.forms.erase(open);
            }
            static_cast<void>(completeRequest(*target, request, answerValue(*target, player, shown, button, values)));
        });
    static_cast<void>(
        state.plugin->registrations.add("form", [moved = std::move(handle)]() mutable { moved.remove(); }));
    c.ret(request);
}

void formClose(void* user, abi::Call& c) {
    State& state = stateOf(user);
    std::shared_ptr<PluginPlayerRef> player;
    const std::int32_t status = state.resolvePlayer(c.i32(0), player);
    if (status != abi::kOk) {
        c.ret(status);
        return;
    }
    EventsArea& area = eventsArea(state);
    const auto open = area.forms.find(player->uuid());
    if (open == area.forms.end()) {
        c.ret(abi::kErrAbsent);
        return;
    }
    static_cast<void>(requests(state).remove(open->second));
    area.forms.erase(open);
    state.host->services().menus().close(*player);
    c.ret(abi::kOk);
}

void listenForDialogs(State& state) {
    EventsArea& area = eventsArea(state);
    if (area.dialogListener) {
        return;
    }
    area.dialogListener = true;
    PluginHost* host = state.host;
    const std::shared_ptr<State*> link = state.link;
    PluginHandle handle = host->events().add(
        pluginEventTypes::playerCustomClick(), state.plugin->manifest.id, 0, PluginEventPriority::Normal, true, false,
        {}, [host, link](PluginEvent& event) {
            const std::lock_guard<std::recursive_mutex> turn{host->turnMutex()};
            State* target = *link;
            const std::string* uuid = std::get_if<std::string>(&event.get("player_uuid"));
            if (target == nullptr || uuid == nullptr) {
                return;
            }
            EventsArea& pending = eventsArea(*target);
            const auto waiting = pending.dialogs.find(*uuid);
            if (waiting == pending.dialogs.end()) {
                return;
            }
            const std::int32_t request = waiting->second;
            pending.dialogs.erase(waiting);
            const std::shared_ptr<PluginPlayerRef> player = host->services().players().byUuid(*uuid);
            const std::string* id = std::get_if<std::string>(&event.get("id"));
            const std::string* payload = std::get_if<std::string>(&event.get("payload"));
            static_cast<void>(completeRequest(
                *target, request,
                Value{Value::Map{{"player", Value{static_cast<std::int64_t>(target->playerHandle(player))}},
                                 {"id", Value{id == nullptr ? std::string{} : *id}},
                                 {"payload", Value{payload == nullptr ? std::string{} : *payload}}}}));
        });
    static_cast<void>(
        state.plugin->registrations.add("dialog answers", [moved = std::move(handle)]() mutable { moved.remove(); }));
}

void dialogShow(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const std::optional<std::string_view> dialog = c.memory().string(c.i32(1), c.i32(2));
    std::shared_ptr<PluginPlayerRef> player;
    std::int32_t status = state.resolvePlayer(c.i32(0), player);
    if (status == abi::kOk && (!dialog || dialog->empty())) {
        status = abi::kErrInvalid;
    }
    if (status != abi::kOk) {
        c.ret(status);
        return;
    }
    const std::optional<PluginEntityValue> shown =
        player->invoke("show_dialog", std::vector<PluginEntityValue>{std::string{*dialog}});
    const bool* worked = shown ? std::get_if<bool>(&*shown) : nullptr;
    if (worked == nullptr || !*worked) {
        c.ret(abi::kErrAbsent);
        return;
    }
    listenForDialogs(state);
    EventsArea& area = eventsArea(state);
    const std::string uuid = player->uuid();
    if (const auto waiting = area.dialogs.find(uuid); waiting != area.dialogs.end()) {
        static_cast<void>(requests(state).remove(waiting->second));
    }
    const std::int32_t request = beginRequest(state, c.i32(3));
    if (request != 0) {
        area.dialogs.insert_or_assign(uuid, request);
    }
    c.ret(request == 0 ? abi::kErrInvalid : request);
}

void dialogClear(void* user, abi::Call& c) {
    State& state = stateOf(user);
    std::shared_ptr<PluginPlayerRef> player;
    const std::int32_t status = state.resolvePlayer(c.i32(0), player);
    if (status == abi::kOk) {
        static_cast<void>(player->invoke("clear_dialog", {}));
        EventsArea& area = eventsArea(state);
        if (const auto waiting = area.dialogs.find(player->uuid()); waiting != area.dialogs.end()) {
            static_cast<void>(requests(state).remove(waiting->second));
            area.dialogs.erase(waiting);
        }
    }
    c.ret(status);
}

[[nodiscard]] std::optional<std::vector<PluginEntityValue>> windowArguments(std::string_view kind, const Value& spec) {
    const auto whole = [&spec](std::string_view key) -> std::optional<std::int64_t> {
        const Value* found = spec.find(key);
        const std::int64_t* number = found == nullptr ? nullptr : found->asInt();
        return number == nullptr ? std::nullopt : std::optional<std::int64_t>{*number};
    };
    if (kind == "merchant") {
        const std::optional<std::string_view> title = mapText(spec, "title");
        const std::optional<std::string_view> offers = mapText(spec, "offers");
        if (!title || !offers) {
            return std::nullopt;
        }
        return std::vector<PluginEntityValue>{std::string{*title}, std::string{*offers}};
    }
    if (kind == "trades") {
        const std::optional<std::int64_t> entity = whole("entity");
        return entity ? std::optional{std::vector<PluginEntityValue>{*entity}} : std::nullopt;
    }
    if (kind == "sign") {
        const std::optional<std::int64_t> x = whole("x");
        const std::optional<std::int64_t> y = whole("y");
        const std::optional<std::int64_t> z = whole("z");
        const Value* front = spec.find("front");
        if (!x || !y || !z) {
            return std::nullopt;
        }
        return std::vector<PluginEntityValue>{*x, *y, *z,
                                              front == nullptr || front->asBool() == nullptr || *front->asBool()};
    }
    const std::optional<std::string_view> item = mapText(spec, "item");
    return item ? std::optional{std::vector<PluginEntityValue>{std::string{*item}}} : std::nullopt;
}

void playerOpenInventory(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const abi::GuestMemory memory = c.memory();
    const std::optional<std::string_view> kind = memory.string(c.i32(1), c.i32(2));
    std::shared_ptr<PluginPlayerRef> player;
    std::int32_t status = state.resolvePlayer(c.i32(0), player);
    if (status != abi::kOk) {
        c.ret(status);
        return;
    }
    Value spec{Value::Map{}};
    if (c.i32(4) != 0) {
        auto document = readValue(memory, c.i32(3), c.i32(4));
        if (!document || document->asMap() == nullptr) {
            c.ret(abi::kErrInvalid);
            return;
        }
        spec = std::move(*document);
    }
    const bool known = kind && (*kind == "merchant" || *kind == "trades" || *kind == "sign" || *kind == "book");
    const std::optional<std::vector<PluginEntityValue>> arguments = known ? windowArguments(*kind, spec) : std::nullopt;
    if (!arguments) {
        c.ret(abi::kErrInvalid);
        return;
    }
    const std::string operation = *kind == "merchant" ? "open_merchant"
                                  : *kind == "trades" ? "open_trades"
                                  : *kind == "sign"   ? "open_sign"
                                                      : "open_book";
    const std::optional<PluginEntityValue> opened = player->invoke(operation, *arguments);
    const bool* worked = opened ? std::get_if<bool>(&*opened) : nullptr;
    c.ret(opened && (worked == nullptr || *worked) ? abi::kOk : abi::kErrAbsent);
}

void playerCloseWindow(void* user, abi::Call& c) {
    State& state = stateOf(user);
    std::shared_ptr<PluginPlayerRef> player;
    const std::int32_t status = state.resolvePlayer(c.i32(0), player);
    if (status == abi::kOk) {
        state.host->services().menus().close(*player);
    }
    c.ret(status);
}
} // namespace

void addEventsFormsImports(State& state, std::vector<abi::HostImport>& out) {
    State* user = &state;
    out.insert(out.end(), {
                              {"form_open", formOpen, user},
                              {"form_close", formClose, user},
                              {"dialog_show", dialogShow, user},
                              {"dialog_clear", dialogClear, user},
                              {"player_open_inventory", playerOpenInventory, user},
                              {"player_close_window", playerCloseWindow, user},
                          });
}
} // namespace basalt::server::plugins::wasm
