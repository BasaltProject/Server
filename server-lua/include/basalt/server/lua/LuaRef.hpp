#pragma once

#include <cstdint>
#include <utility>

struct lua_State;

namespace basalt::server::lua {

class LuaArgs;
class LuaSandbox;

/// One value kept alive outside the stack: a plugin's handler, held until it is unregistered. A
/// registry reference has to be released or the value is never collected, and owning it here is what
/// makes an unregistered handler actually go away.
///
/// A reference belongs to the state that made it: moving one between sandboxes is a bug this cannot
/// catch.
class LuaRef {
public:
    LuaRef() = default;

    LuaRef(const LuaRef&) = delete;
    LuaRef& operator=(const LuaRef&) = delete;

    LuaRef(LuaRef&& other) noexcept
        : state_(std::exchange(other.state_, nullptr)), ref_(std::exchange(other.ref_, 0)) {}

    LuaRef& operator=(LuaRef&& other) noexcept;

    ~LuaRef();

    /// Takes the value on top of the stack, popping it.
    [[nodiscard]] static LuaRef take(lua_State* state);

    /// Takes the value at `index` without disturbing the stack.
    [[nodiscard]] static LuaRef of(lua_State* state, std::int32_t index);

    [[nodiscard]] bool valid() const noexcept { return state_ != nullptr; }

    /// Pushes the value back onto its own state's stack. Pushes nil when there is nothing held.
    void push() const;

    /// The same, for a call that is building its arguments through the facade.
    void push(LuaArgs& lua) const;

    [[nodiscard]] lua_State* state() const noexcept { return state_; }

    /// Drops the reference now rather than at destruction, for a handler that was unregistered.
    void release();

private:
    LuaRef(lua_State* state, std::int32_t ref) : state_(state), ref_(ref) {}

    lua_State* state_ = nullptr;
    std::int32_t ref_ = 0;
};

} // namespace basalt::server::lua
