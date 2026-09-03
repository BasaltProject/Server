#pragma once

#include <basalt/server/lua/LuaArgs.hpp>
#include <basalt/server/lua/LuaRef.hpp>
#include <basalt/server/lua/LuaSandbox.hpp>

#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct lua_State;

namespace basalt::server::lua {

/// A call into Lua that is allowed to pause: a plugin runs on its own Lua thread, so a host function
/// can yield and the interpreter unwinds to whoever entered it. That is what lets a plugin write
/// `wait(20)` in the middle of a command.
///
/// Entering through this rather than a plain call is deliberate: a synchronous entry point cannot
/// pause, and a plugin that tries is told so instead of blocking a tick.
class LuaCall : public std::enable_shared_from_this<LuaCall> {
public:
    /// Carries what went wrong, or nothing. Called once, and possibly before `start` returns, which
    /// is the common case: most calls never pause at all.
    using Done = std::function<void(std::optional<LuaFailure>)>;

    /// What a waiting host function is handed: calling it continues the plugin **on the caller's
    /// thread**, so it is called from the owner that is allowed to run this plugin and nowhere else.
    /// Never calling it drops the rest of the turn, which is how an unloaded plugin stops without
    /// anything to kill.
    using Resume = std::function<void()>;

    /// Arranges for the plugin to be continued later. Called while the plugin is paused.
    using Arrange = std::function<void(Resume)>;

    LuaCall(const LuaCall&) = delete;
    LuaCall& operator=(const LuaCall&) = delete;
    ~LuaCall();

    /// Runs the function at the top of the sandbox's stack, popping it, until it finishes or pauses.
    [[nodiscard]] static std::shared_ptr<LuaCall> start(LuaSandbox& sandbox, std::int32_t argumentCount, Done onDone);

    [[nodiscard]] bool finished() const noexcept { return finished_; }

    /// Whether the plugin is paused waiting for something rather than running or done.
    [[nodiscard]] bool paused() const noexcept { return !finished_ && arranged_; }

private:
    explicit LuaCall(LuaSandbox& sandbox, lua_State* thread, std::int32_t threadRef, Done onDone);

    friend struct CallAccess;

    void step(std::int32_t argumentCount);

    LuaSandbox* sandbox_;
    lua_State* thread_;
    /// The thread's slot in the registry, which is what keeps it from being collected mid-pause.
    std::int32_t threadRef_;
    Done onDone_;
    bool finished_ = false;
    bool arranged_ = false;
};

/// Installs a global the plugin can call and that answers straight away.
void installHostFunction(LuaSandbox& sandbox, const std::string& name, std::function<std::int32_t(LuaArgs&)> body);

/// Installs a global the plugin may wait on. Called from somewhere that cannot pause (a `table.sort`
/// comparator, a metamethod, anything the interpreter reached through its synchronous path) it says
/// so rather than blocking a tick.
void installWaitingFunction(LuaSandbox& sandbox, const std::string& name, LuaCall::Arrange arrange);

/// One synchronous call of a held value, which cannot pause.
///
/// For a placeholder or a surface: the server renders those in the middle of drawing something, so
/// there is nowhere to pause to. `pushArguments` leaves its values on the stack and answers how many
/// it left; the results are left on the stack for the caller to read and pop.
[[nodiscard]] std::expected<std::int32_t, LuaFailure>
callSync(LuaSandbox& sandbox, const LuaRef& function, const std::function<std::int32_t(LuaArgs&)>& pushArguments,
         std::int32_t resultCount);

} // namespace basalt::server::lua
