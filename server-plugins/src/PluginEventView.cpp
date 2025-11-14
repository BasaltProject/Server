#include <basalt/server/plugins/PluginEventView.hpp>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

#include <utility>
#include <vector>

namespace basalt::server::plugins {
namespace {

[[nodiscard]] std::string joined(const std::vector<std::string>& values, std::string_view separator) {
    std::string out;
    for (const std::string& value : values) {
        if (!out.empty()) {
            out += separator;
        }
        out += value;
    }
    return out;
}

[[nodiscard]] bool carries(const PluginEventType& type, std::string_view field) { return type.indexOf(field) >= 0; }

} // namespace

/// The one thing allowed to reach a view's privates from a C callback.
struct ViewAccess {
    [[nodiscard]] static PluginEventView* of(lua_State* state) {
        return static_cast<PluginEventView*>(lua_touserdata(state, lua_upvalueindex(1)));
    }

    /// Refuses to answer once the handler has returned, or once the event has been fired again.
    static void check(lua_State* state, PluginEventView& view) {
        if (view.generation_ != view.event_->generation() || !view.event_->live()) {
            luaL_error(state, "this %s event is over: it cannot be read or changed after the handler returns",
                       view.event_->type().name().c_str());
        }
    }

    static void pushValue(lua::LuaArgs& lua, const PluginEventValue& value) {
        if (const bool* flag = std::get_if<bool>(&value)) {
            lua.pushBoolean(*flag);
            return;
        }
        if (const std::int64_t* whole = std::get_if<std::int64_t>(&value)) {
            lua.pushInteger(*whole);
            return;
        }
        if (const double* real = std::get_if<double>(&value)) {
            lua.pushNumber(*real);
            return;
        }
        if (const std::string* text = std::get_if<std::string>(&value)) {
            lua.pushString(*text);
            return;
        }
        lua.pushNil();
    }

    [[nodiscard]] static PluginEventValue valueOf(lua::LuaArgs& lua, std::int32_t index) {
        if (lua.isNil(index)) {
            return PluginEventValue{};
        }
        if (lua.isBoolean(index)) {
            return lua.toBoolean(index);
        }
        if (lua.isInteger(index)) {
            return lua.toInteger(index);
        }
        if (lua.isNumber(index)) {
            return lua.toNumber(index);
        }
        return lua.toText(index);
    }

    static int cancel(lua_State* state) {
        PluginEventView& view = *of(state);
        check(state, view);
        if (!view.event_->type().cancellable()) {
            return luaL_error(state, "a %s event cannot be cancelled: it is telling you, not asking",
                              view.event_->type().name().c_str());
        }
        // Both `e:cancel()` and `e:cancel(message)` land here, and `e.cancel()` too: the first
        // argument is the table when it was called with a colon.
        lua::LuaArgs lua{state};
        std::int32_t first = 1;
        if (lua.isTable(1)) {
            first = 2;
        }
        std::optional<std::string> written;
        if (lua.count() >= first && lua.isString(first)) {
            written = lua.checkString(first);
            if (view.resolve_) {
                written = view.resolve_(*written);
            }
        }
        view.onCancel_(std::move(written));
        return 0;
    }

    static int index(lua_State* state) {
        PluginEventView& view = *of(state);
        lua::LuaArgs lua{state};
        const std::string key = lua.checkString(2);
        if (key == "cancel") {
            lua_pushvalue(state, lua_upvalueindex(2));
            return 1;
        }
        if (key == "name") {
            lua.pushString(view.event_->type().name());
            return 1;
        }
        if (key == "cancelled") {
            check(state, view);
            lua.pushBoolean(view.event_->cancelled());
            return 1;
        }
        check(state, view);
        if (view.objects_ && view.objects_(lua, key)) {
            return 1;
        }
        const std::int32_t at = view.event_->type().indexOf(key);
        if (at < 0) {
            const PluginEventType& type = view.event_->type();
            std::string extras;
            if (carries(type, "player_name")) {
                extras += " (and 'player')";
            }
            if (carries(type, "entity_id")) {
                extras += " (and 'entity')";
            }
            if (carries(type, "x")) {
                extras += " (and 'pos')";
            }
            return luaL_error(state, "a %s event has no '%s'; it has %s%s", type.name().c_str(), key.c_str(),
                              joined(type.fields(), ", ").c_str(), extras.c_str());
        }
        pushValue(lua, view.event_->get(at));
        return 1;
    }

    static int newIndex(lua_State* state) {
        PluginEventView& view = *of(state);
        check(state, view);
        lua::LuaArgs lua{state};
        const std::string key = lua.checkString(2);
        const PluginEventType& type = view.event_->type();
        if (!type.isMutable(key)) {
            std::vector<std::string> allowed;
            for (const std::string& field : type.fields()) {
                if (type.isMutable(field)) {
                    allowed.push_back(field);
                }
            }
            const std::string names = allowed.empty() ? "nothing" : joined(allowed, ", ");
            return luaL_error(state, "'%s' cannot be changed on a %s event; it lets you change %s", key.c_str(),
                              type.name().c_str(), names.c_str());
        }
        view.event_->set(key, valueOf(lua, 3));
        return 0;
    }
};

PluginEventView::PluginEventView(lua::LuaSandbox& sandbox, PluginEvent& event, OnCancel onCancel, Objects objects,
                                 Resolve resolve)
    : sandbox_(&sandbox), event_(&event), onCancel_(std::move(onCancel)), objects_(std::move(objects)),
      resolve_(std::move(resolve)) {}

PluginEventView::~PluginEventView() {
    if (tableRef_ != 0) {
        luaL_unref(sandbox_->state(), LUA_REGISTRYINDEX, tableRef_);
    }
}

std::unique_ptr<PluginEventView> PluginEventView::create(lua::LuaSandbox& sandbox, PluginEvent& event,
                                                         OnCancel onCancel, Objects objects, Resolve resolve) {
    std::unique_ptr<PluginEventView> view{
        new PluginEventView{sandbox, event, std::move(onCancel), std::move(objects), std::move(resolve)}};
    lua_State* state = sandbox.state();

    lua_newtable(state);
    lua_newtable(state);
    // The cancel function is an upvalue of `__index` rather than a field of the table: nothing of
    // the event is in the table itself, so both metamethods always run.
    lua_pushlightuserdata(state, view.get());
    lua_pushcclosure(state, &ViewAccess::cancel, 1);
    lua_pushlightuserdata(state, view.get());
    lua_pushvalue(state, -2);
    lua_pushcclosure(state, &ViewAccess::index, 2);
    lua_setfield(state, -3, "__index");
    // The cancel closure itself is no longer needed on the stack.
    lua_pop(state, 1);
    lua_pushlightuserdata(state, view.get());
    lua_pushcclosure(state, &ViewAccess::newIndex, 1);
    lua_setfield(state, -2, "__newindex");
    lua_setmetatable(state, -2);
    view->tableRef_ = luaL_ref(state, LUA_REGISTRYINDEX);
    return view;
}

void PluginEventView::bind() { generation_ = event_->generation(); }

void PluginEventView::push(lua::LuaArgs& lua) const { lua_rawgeti(lua.state(), LUA_REGISTRYINDEX, tableRef_); }

} // namespace basalt::server::plugins
