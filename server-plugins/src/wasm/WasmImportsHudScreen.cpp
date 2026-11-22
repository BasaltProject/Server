#include "WasmHud.hpp"

#include <basalt/server/plugins/PluginEntityOps.hpp>
#include <basalt/server/plugins/PluginPlayerOps.hpp>

#include <algorithm>
#include <array>

namespace basalt::server::plugins::wasm {
namespace hud {
namespace {
constexpr std::array<std::string_view, 8> kHudProperties{
    "tab_header", "tab_footer", "list_name", "listed", "list_order", "score", "waypoint_style", "waypoint_color"};

[[nodiscard]] constexpr const EntityPropertySpec* propertySpec(std::string_view name) {
    for (const EntityPropertySpec& spec : kPlayerProperties) {
        if (spec.name == name) {
            return &spec;
        }
    }
    return nullptr;
}

[[nodiscard]] constexpr bool allPropertiesKnown() {
    for (const std::string_view name : kHudProperties) {
        const EntityPropertySpec* spec = propertySpec(name);
        if (spec == nullptr || !spec->writable) {
            return false;
        }
    }
    return true;
}

static_assert(allPropertiesKnown());

[[nodiscard]] std::optional<PluginEntityValue> propertyValue(State& state, const EntityPropertySpec& spec,
                                                             const Value& value, std::int32_t& status) {
    status = abi::kOk;
    if (value.isNull() && spec.kind == EntityValueKind::Text) {
        return PluginEntityValue{};
    }
    switch (spec.kind) {
    case EntityValueKind::Bool:
        if (const bool* flag = value.asBool()) {
            return PluginEntityValue{*flag};
        }
        break;
    case EntityValueKind::Int:
        if (const std::int64_t* whole = value.asInt()) {
            return PluginEntityValue{*whole};
        }
        break;
    case EntityValueKind::Text:
        if (const std::string* text = value.asString()) {
            std::string rendered;
            status = renderPlain(state.plugin->text, *text, rendered);
            return PluginEntityValue{std::move(rendered)};
        }
        break;
    case EntityValueKind::Real:
    case EntityValueKind::List:
        break;
    }
    status = abi::kErrInvalid;
    return std::nullopt;
}

[[nodiscard]] Value valueOfProperty(const PluginEntityValue& value) {
    if (const bool* flag = std::get_if<bool>(&value)) {
        return Value{*flag};
    }
    if (const std::int64_t* whole = std::get_if<std::int64_t>(&value)) {
        return Value{*whole};
    }
    if (const double* real = std::get_if<double>(&value)) {
        return Value{*real};
    }
    if (const std::string* text = std::get_if<std::string>(&value)) {
        return Value{*text};
    }
    return Value{};
}

void playerSet(void* user, abi::Call& c) {
    State& state = stateOf(user);
    std::shared_ptr<PluginPlayerRef> player;
    std::int32_t status = state.resolvePlayer(c.i32(0), player);
    const std::expected<Value, std::int32_t> spec = readSpec(c.memory(), c.i32(1), c.i32(2));
    if (status == abi::kOk && !spec) {
        status = abi::kErrInvalid;
    }
    std::vector<std::pair<std::string_view, PluginEntityValue>> changes;
    for (std::size_t at = 0; status == abi::kOk && spec->asMap() != nullptr && at < spec->asMap()->size(); ++at) {
        const wire::wasm::MapEntry& entry = (*spec->asMap())[at];
        const EntityPropertySpec* known = propertySpec(entry.key);
        const bool inGroup = std::ranges::find(kHudProperties, entry.key) != kHudProperties.end();
        if (known == nullptr || !inGroup) {
            status = abi::kErrInvalid;
            break;
        }
        const std::optional<PluginEntityValue> converted = propertyValue(state, *known, entry.value, status);
        if (converted) {
            changes.emplace_back(known->name, *converted);
        }
    }
    for (std::size_t at = 0; status == abi::kOk && at < changes.size(); ++at) {
        status = player->setProperty(changes[at].first, changes[at].second) ? abi::kOk : abi::kErrInvalid;
    }
    c.ret(status == abi::kErrAbsent ? abi::kErrInvalid : status);
}

void playerGet(void* user, abi::Call& c) {
    State& state = stateOf(user);
    std::shared_ptr<PluginPlayerRef> player;
    const std::int32_t status = state.resolvePlayer(c.i32(0), player);
    if (status != abi::kOk) {
        c.ret(status);
        return;
    }
    Value::Map map;
    for (const std::string_view name : kHudProperties) {
        if (const std::optional<PluginEntityValue> value = player->property(name)) {
            map.push_back({std::string{name}, valueOfProperty(*value)});
        }
    }
    c.ret(putValue(c.memory(), c.i32(1), c.i32(2), Value{std::move(map)}));
}

void tabEntry(void* user, abi::Call& c) {
    State& state = stateOf(user);
    std::shared_ptr<PluginPlayerRef> viewer;
    const std::int32_t status = state.resolvePlayer(c.i32(0), viewer);
    if (status != abi::kOk) {
        c.ret(status);
        return;
    }
    const std::optional<PluginEntityValue> done =
        viewer->invoke("list_player", {PluginEntityValue{std::int64_t{c.i32(1)}}, PluginEntityValue{c.i32(2) != 0}});
    const bool* applied = done ? std::get_if<bool>(&*done) : nullptr;
    c.ret(applied != nullptr && *applied ? abi::kOk : abi::kErrInvalid);
}

[[nodiscard]] std::int32_t readContent(State& state, const Value& spec, PluginSurfaceContent& out) {
    std::optional<std::string> title;
    std::int32_t status = takeString(spec, "title", title);
    if (status == abi::kOk && title) {
        status = renderPlain(state.plugin->text, *title, out.title);
    }
    const Value* lines = spec.find("lines");
    if (status == abi::kOk && lines != nullptr) {
        const Value::List* list = lines->asList();
        if (list == nullptr) {
            return abi::kErrInvalid;
        }
        for (const Value& item : *list) {
            const std::string* text = item.asString();
            std::string rendered;
            if (text == nullptr || renderPlain(state.plugin->text, *text, rendered) != abi::kOk) {
                return abi::kErrInvalid;
            }
            out.lines.push_back(std::move(rendered));
        }
    }
    return status == abi::kErrAbsent ? abi::kErrInvalid : status;
}

[[nodiscard]] std::optional<PluginSurfaceContent> askRender(PluginHost& host, const std::shared_ptr<State*>& link,
                                                            std::int32_t token, std::string_view surface,
                                                            PluginPlayerRef& player) {
    const std::lock_guard<std::recursive_mutex> turn{host.turnMutex()};
    State* state = *link;
    if (state == nullptr || !host.onPluginThread()) {
        return std::nullopt;
    }
    HudState& hud = stateOfHud(*state);
    if (hud.answering) {
        return std::nullopt;
    }
    hud.answering = true;
    hud.answer.reset();
    Value::Map map;
    map.push_back({"surface", Value{std::string{surface}}});
    map.push_back({"player", Value{player.name()}});
    const Value value{std::move(map)};
    Context context;
    context.kind = Context::Kind::Completion;
    context.value = &value;
    const bool ran = state->run(token, context, "surface render");
    hud.answering = false;
    std::optional<PluginSurfaceContent> answer = std::move(hud.answer);
    hud.answer.reset();
    return ran ? answer : std::nullopt;
}

void surfaceClaim(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const std::optional<std::string_view> name = c.memory().string(c.i32(0), c.i32(1));
    const std::expected<Value, std::int32_t> spec = readSpec(c.memory(), c.i32(3), c.i32(4));
    const std::optional<PluginSurface> surface = name ? surfaceOf(*name) : std::nullopt;
    const std::int32_t token = c.i32(5);
    if (!surface || !spec || token < 0) {
        c.ret(abi::kErrInvalid);
        return;
    }
    const std::shared_ptr<ClaimContent> content = std::make_shared<ClaimContent>();
    PluginSurfaceContent fixed;
    if (readContent(state, *spec, fixed) != abi::kOk) {
        c.ret(abi::kErrInvalid);
        return;
    }
    content->fixed = std::move(fixed);
    PluginHost* host = state.host;
    PluginSurfaceClaim claim{.pluginId = state.plugin->manifest.id, .surface = *surface, .priority = c.i32(2)};
    claim.render = [host, link = state.link, content, token,
                    label = std::string{*name}](PluginPlayerRef& player) -> std::optional<PluginSurfaceContent> {
        if (token != 0) {
            return askRender(*host, link, token, label, player);
        }
        const std::lock_guard<std::mutex> hold{content->lock};
        const bool empty = !content->fixed || (content->fixed->title.empty() && content->fixed->lines.empty());
        return empty ? std::nullopt : content->fixed;
    };
    HudState& hud = stateOfHud(state);
    const std::int32_t handle = hud.claims.add(Claim{content, {}});
    if (handle == 0) {
        c.ret(abi::kErrInvalid);
        return;
    }
    PluginHandle placed = host->surfaces().claim(std::move(claim));
    Claim* held = nullptr;
    static_cast<void>(hud.claims.find(handle, held));
    held->registration =
        state.plugin->registrations.add(std::string{*name} + "(priority " + std::to_string(c.i32(2)) + ")",
                                        [moved = std::move(placed)]() mutable { moved.remove(); });
    c.ret(handle);
}

void surfaceUpdate(void* user, abi::Call& c) {
    State& state = stateOf(user);
    Claim* held = nullptr;
    std::int32_t status = stateOfHud(state).claims.find(c.i32(0), held);
    const std::expected<Value, std::int32_t> spec = readSpec(c.memory(), c.i32(1), c.i32(2));
    PluginSurfaceContent fixed;
    if (status == abi::kOk) {
        status = spec ? readContent(state, *spec, fixed) : abi::kErrInvalid;
    }
    if (status == abi::kOk) {
        const std::lock_guard<std::mutex> hold{held->content->lock};
        held->content->fixed = std::move(fixed);
    }
    c.ret(status);
}

void surfaceRelease(void* user, abi::Call& c) {
    State& state = stateOf(user);
    HudState& hud = stateOfHud(state);
    Claim* held = nullptr;
    const std::int32_t status = hud.claims.find(c.i32(0), held);
    if (status != abi::kOk) {
        c.ret(status);
        return;
    }
    PluginHandle registration = held->registration;
    static_cast<void>(hud.claims.remove(c.i32(0)));
    registration.remove();
    c.ret(abi::kOk);
}

void surfaceAnswer(void* user, abi::Call& c) {
    State& state = stateOf(user);
    HudState& hud = stateOfHud(state);
    Context* context = nullptr;
    std::int32_t status = state.check(c.i32(0), Context::Kind::Completion, context);
    if (status == abi::kOk && !hud.answering) {
        status = abi::kErrInvalid;
    }
    const std::expected<Value, std::int32_t> spec = readSpec(c.memory(), c.i32(1), c.i32(2));
    PluginSurfaceContent content;
    if (status == abi::kOk) {
        status = spec ? readContent(state, *spec, content) : abi::kErrInvalid;
    }
    if (status == abi::kOk) {
        hud.answer = std::move(content);
    }
    c.ret(status);
}
} // namespace
} // namespace hud

void addHudScreenImports(State& state, std::vector<abi::HostImport>& out) {
    State* user = &state;
    out.insert(out.end(), {
                              {"hud_player_set", hud::playerSet, user},
                              {"hud_player_get", hud::playerGet, user},
                              {"hud_tab_entry", hud::tabEntry, user},
                              {"hud_surface_claim", hud::surfaceClaim, user},
                              {"hud_surface_update", hud::surfaceUpdate, user},
                              {"hud_surface_release", hud::surfaceRelease, user},
                              {"hud_surface_answer", hud::surfaceAnswer, user},
                          });
}
} // namespace basalt::server::plugins::wasm
