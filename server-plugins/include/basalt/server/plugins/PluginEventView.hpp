#pragma once

#include <basalt/server/lua/LuaArgs.hpp>
#include <basalt/server/lua/LuaSandbox.hpp>
#include <basalt/server/plugins/PluginEvents.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace basalt::server::plugins {

/// What a handler receives: a table that reads straight from the event rather than a copy of it.
///
/// One of these per event type and lane, made when the first handler is registered and reused for
/// every fire, so dispatching does not build a table. Reading a field goes through `__index` to the
/// event itself; writing one goes through `__newindex`, which refuses anything the type did not
/// declare mutable instead of quietly keeping a value the server will never read.
///
/// It also refuses to work once the handler has returned. That is not tidiness: an event object is
/// reused, so a plugin that stashed one and read it a tick later would be reading somebody else's
/// event, and the value would look perfectly reasonable.
class PluginEventView {
public:
    /// What the server does when a handler cancels: the message is already resolved.
    using OnCancel = std::function<void(std::optional<std::string>)>;

    /// Turns a name the event carries into the object behind it: `player` from `player_name`,
    /// `entity` from `entity_id`. Pushes one value and answers true, or pushes nothing.
    ///
    /// Looked up rather than carried, and only when asked for: an event that fires every tick should
    /// not build an object nobody reads.
    using Objects = std::function<bool(lua::LuaArgs&, std::string_view name)>;

    /// A refusal is text like any other, so `@key` and `{name}` work in it too.
    using Resolve = std::function<std::string(std::string_view message)>;

    PluginEventView(const PluginEventView&) = delete;
    PluginEventView& operator=(const PluginEventView&) = delete;
    ~PluginEventView();

    /// Builds the view and leaves its table in the state's registry. `sandbox` outlives it.
    [[nodiscard]] static std::unique_ptr<PluginEventView>
    create(lua::LuaSandbox& sandbox, PluginEvent& event, OnCancel onCancel, Objects objects = {}, Resolve resolve = {});

    /// Ties the view to this fire; anything held from an earlier one is stale from here on.
    void bind();

    /// Pushes the table a handler is called with.
    void push(lua::LuaArgs& lua) const;

private:
    PluginEventView(lua::LuaSandbox& sandbox, PluginEvent& event, OnCancel onCancel, Objects objects, Resolve resolve);

    friend struct ViewAccess;

    lua::LuaSandbox* sandbox_;
    PluginEvent* event_;
    OnCancel onCancel_;
    Objects objects_;
    Resolve resolve_;
    std::int64_t generation_ = -1;
    /// The table's slot in the registry, which is what keeps it alive between fires.
    std::int32_t tableRef_ = 0;
};

} // namespace basalt::server::plugins
