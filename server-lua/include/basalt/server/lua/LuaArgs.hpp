#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

struct lua_State;

namespace basalt::server::lua {

/// What a host function sees of the call that reached it. The Lua C API stays behind this because
/// `lua.h` is a large header every consumer writing a host function would otherwise carry.
///
/// Arguments are one-based, and every `check` refuses a wrong or missing one the way the standard
/// library does: with an error naming the argument and the function.
class LuaArgs {
public:
    explicit LuaArgs(lua_State* state) : state_(state) {}

    [[nodiscard]] std::int32_t count() const;

    [[nodiscard]] bool isNil(std::int32_t index) const;
    [[nodiscard]] bool isFunction(std::int32_t index) const;
    [[nodiscard]] bool isTable(std::int32_t index) const;
    [[nodiscard]] bool isString(std::int32_t index) const;
    [[nodiscard]] bool isNumber(std::int32_t index) const;

    [[nodiscard]] std::string checkString(std::int32_t index) const;
    [[nodiscard]] std::int64_t checkInteger(std::int32_t index) const;
    [[nodiscard]] double checkNumber(std::int32_t index) const;

    /// A string argument, or nothing when it is absent or not one.
    [[nodiscard]] std::optional<std::string> stringOrNull(std::int32_t index) const;
    /// An integer argument, or the fallback when it is absent.
    [[nodiscard]] std::int64_t integerOr(std::int32_t index, std::int64_t fallback) const;
    [[nodiscard]] double numberOr(std::int32_t index, double fallback) const;
    [[nodiscard]] bool booleanOr(std::int32_t index, bool fallback) const;

    /// The value of one field of a table argument, as text, or nothing when it is absent or nil.
    /// Numbers and booleans come back the way Lua writes them, which is what a message wants.
    [[nodiscard]] std::optional<std::string> fieldAsText(std::int32_t index, std::string_view key) const;

    void pushNil();
    void pushBoolean(bool value);
    void pushInteger(std::int64_t value);
    void pushNumber(double value);
    void pushString(std::string_view value);
    /// A fresh empty table, left on the stack for the caller to fill through `setField`.
    void pushTable();

    /// Sets `key` on the table just below the value on top, popping the value.
    void setField(std::string_view key);

    /// Sets a one-based position on the table just below the value on top, popping the value. Lua
    /// arrays start at one, so this is what a list from the host is written with.
    void setIndex(std::int64_t index);

    /// Raises a Lua error from inside a host function. Does not return.
    [[noreturn]] void raise(std::string_view message) const;

    [[nodiscard]] lua_State* state() const noexcept { return state_; }

private:
    lua_State* state_;
};

} // namespace basalt::server::lua
