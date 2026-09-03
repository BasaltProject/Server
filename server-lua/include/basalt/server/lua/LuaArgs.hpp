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

    /// Pushes the value at a one-based position of the table at `index`.
    void pushIndex(std::int32_t index, std::int64_t position);

    /// Pushes the value of one field of the table at `index`.
    void pushField(std::int32_t index, std::string_view key);

    /// Drops `count` values from the top of the stack.
    void pop(std::int32_t count);

    /// How long the array part of the table at `index` is, which is what `#` answers.
    [[nodiscard]] std::int64_t length(std::int32_t index) const;

    /// Walks the table at `index`. Starting from an absent key gives the first entry; each call
    /// leaves the key and the value on the stack and answers true, or leaves nothing and answers
    /// false at the end. This is `next`, which is the only way to see a table that is not an array.
    [[nodiscard]] bool nextEntry(std::int32_t index);

    [[nodiscard]] bool isBoolean(std::int32_t index) const;
    /// Whether the number at `index` is an integer rather than a float, which JSON has to keep.
    [[nodiscard]] bool isInteger(std::int32_t index) const;
    [[nodiscard]] bool toBoolean(std::int32_t index) const;
    [[nodiscard]] std::int64_t toInteger(std::int32_t index) const;
    [[nodiscard]] double toNumber(std::int32_t index) const;
    /// The value at `index` as text, whatever it is, the way Lua itself writes it.
    [[nodiscard]] std::string toText(std::int32_t index) const;

    /// Makes the table on top of the stack the metatable of the value at `index`, popping it.
    void setMetatable(std::int32_t index);

    /// Pushes the metatable of the value at `index`, or nil when it has none.
    void pushMetatable(std::int32_t index);

    /// Sets the value on top as a global, popping it. What an installed API is put into place with.
    void setGlobal(std::string_view name);

    /// How many values are on the stack, which is what a host function counts its results by.
    [[nodiscard]] std::int32_t top() const;

    /// Raises a Lua error from inside a host function. Does not return.
    [[noreturn]] void raise(std::string_view message) const;

    [[nodiscard]] lua_State* state() const noexcept { return state_; }

private:
    lua_State* state_;
};

} // namespace basalt::server::lua
