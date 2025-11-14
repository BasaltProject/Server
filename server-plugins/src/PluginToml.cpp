#include <basalt/server/plugins/PluginToml.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace basalt::server::plugins::pluginToml {

using wire::common::toml::TomlTable;
using wire::common::toml::TomlValue;

void push(lua::LuaArgs& lua, const TomlTable& table) {
    lua.pushTable();
    for (const std::string& key : table.keys()) {
        const TomlValue* value = table.find(key);
        if (value == nullptr) {
            continue;
        }
        pushValue(lua, *value);
        lua.setField(key);
        // TOML is written kebab-case here and in the server's own files, and Lua cannot spell a dash
        // in `config.starting-balance`, so the same key answers to an underscore too.
        if (key.find('-') != std::string::npos) {
            std::string underscored = key;
            for (char& character : underscored) {
                if (character == '-') {
                    character = '_';
                }
            }
            pushValue(lua, *value);
            lua.setField(underscored);
        }
    }
}

void pushValue(lua::LuaArgs& lua, const TomlValue& value) {
    if (const std::string* text = value.get<std::string>(); text != nullptr) {
        lua.pushString(*text);
        return;
    }
    if (const std::int64_t* whole = value.get<std::int64_t>(); whole != nullptr) {
        lua.pushInteger(*whole);
        return;
    }
    if (const double* real = value.get<double>(); real != nullptr) {
        lua.pushNumber(*real);
        return;
    }
    if (const bool* flag = value.get<bool>(); flag != nullptr) {
        lua.pushBoolean(*flag);
        return;
    }
    if (const std::vector<TomlValue>* array = value.get<std::vector<TomlValue>>(); array != nullptr) {
        lua.pushTable();
        for (std::size_t index = 0; index < array->size(); ++index) {
            pushValue(lua, (*array)[index]);
            lua.setIndex(static_cast<std::int64_t>(index) + 1);
        }
        return;
    }
    if (const TomlTable* table = value.get<TomlTable>(); table != nullptr) {
        push(lua, *table);
        return;
    }
    lua.pushNil();
}

} // namespace basalt::server::plugins::pluginToml
