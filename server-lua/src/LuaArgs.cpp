#include <basalt/server/lua/LuaArgs.hpp>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

#include <cstdlib>

namespace basalt::server::lua {

std::int32_t LuaArgs::count() const { return lua_gettop(state_); }

bool LuaArgs::isNil(std::int32_t index) const { return lua_isnoneornil(state_, index); }

bool LuaArgs::isFunction(std::int32_t index) const { return lua_isfunction(state_, index); }

bool LuaArgs::isTable(std::int32_t index) const { return lua_istable(state_, index); }

bool LuaArgs::isString(std::int32_t index) const { return lua_isstring(state_, index) != 0; }

bool LuaArgs::isNumber(std::int32_t index) const { return lua_isnumber(state_, index) != 0; }

std::string LuaArgs::checkString(std::int32_t index) const {
    std::size_t length = 0;
    const char* text = luaL_checklstring(state_, index, &length);
    return std::string{text, length};
}

std::int64_t LuaArgs::checkInteger(std::int32_t index) const {
    return static_cast<std::int64_t>(luaL_checkinteger(state_, index));
}

double LuaArgs::checkNumber(std::int32_t index) const { return static_cast<double>(luaL_checknumber(state_, index)); }

std::optional<std::string> LuaArgs::stringOrNull(std::int32_t index) const {
    if (lua_isstring(state_, index) == 0) {
        return std::nullopt;
    }
    std::size_t length = 0;
    const char* text = lua_tolstring(state_, index, &length);
    return std::string{text, length};
}

std::int64_t LuaArgs::integerOr(std::int32_t index, std::int64_t fallback) const {
    return static_cast<std::int64_t>(luaL_optinteger(state_, index, static_cast<lua_Integer>(fallback)));
}

double LuaArgs::numberOr(std::int32_t index, double fallback) const {
    return static_cast<double>(luaL_optnumber(state_, index, static_cast<lua_Number>(fallback)));
}

bool LuaArgs::booleanOr(std::int32_t index, bool fallback) const {
    if (lua_isnoneornil(state_, index)) {
        return fallback;
    }
    return lua_toboolean(state_, index) != 0;
}

void LuaArgs::pushNil() { lua_pushnil(state_); }

void LuaArgs::pushBoolean(bool value) { lua_pushboolean(state_, value ? 1 : 0); }

void LuaArgs::pushInteger(std::int64_t value) { lua_pushinteger(state_, static_cast<lua_Integer>(value)); }

void LuaArgs::pushNumber(double value) { lua_pushnumber(state_, static_cast<lua_Number>(value)); }

void LuaArgs::pushString(std::string_view value) { lua_pushlstring(state_, value.data(), value.size()); }

void LuaArgs::pushTable() { lua_newtable(state_); }

void LuaArgs::setField(std::string_view key) { lua_setfield(state_, -2, std::string{key}.c_str()); }

void LuaArgs::setIndex(std::int64_t index) { lua_rawseti(state_, -2, static_cast<lua_Integer>(index)); }

void LuaArgs::raise(std::string_view message) const {
    luaL_error(state_, "%s", std::string{message}.c_str());
    // `luaL_error` never returns; this is only here because the compiler cannot know that.
    std::abort();
}

} // namespace basalt::server::lua
