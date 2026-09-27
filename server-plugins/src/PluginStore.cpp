#include <basalt/server/plugins/PluginJson.hpp>
#include <basalt/server/plugins/PluginObject.hpp>
#include <basalt/server/plugins/PluginStore.hpp>
#include <basalt/wire/io/FileIO.hpp>
#include <basalt/wire/lua/LuaCall.hpp>
#include <basalt/wire/plugins/PluginValue.hpp>

#include <charconv>
#include <cstdint>
#include <memory>
#include <utility>

namespace basalt::server::plugins {
namespace {
using wire::lua::LuaArgs;

[[nodiscard]] std::optional<std::string> storedValue(LuaArgs& args) {
    if (args.isNil(3)) {
        return std::nullopt;
    }
    std::optional<std::string> encoded = wire::plugins::encodeValue(args, 3);
    if (!encoded.has_value()) {
        args.raise("a stored value is a boolean, a number or a string");
    }
    return encoded;
}

void pushTop(LuaArgs& lua, PluginHost& host, const std::string& pluginId) {
    PluginHost* target = &host;
    wire::lua::pushHostFunction(lua, "store.top", [target, pluginId](LuaArgs& args) {
        const std::int32_t first = firstArgument(args);
        const std::string key = args.checkString(first);
        const auto limit = static_cast<std::int32_t>(args.integerOr(first + 1, 10));
        args.pushTable();
        std::int64_t position = 1;
        for (const auto& [uuid, value] : target->services().storage().top(pluginId, key, limit)) {
            args.pushTable();
            args.pushString(uuid);
            args.setField("uuid");
            const std::shared_ptr<PluginPlayerRef> player = target->services().players().byUuid(uuid);
            args.pushString(player == nullptr ? uuid : player->name());
            args.setField("name");
            wire::plugins::pushValue(args, value);
            args.setField("value");
            args.setIndex(position++);
        }
        return 1;
    });
}
} // namespace

void pushStore(LuaArgs& lua, PluginHost& host, const std::string& pluginId) {
    PluginHost* target = &host;
    lua.pushTable();
    lua.pushTable();
    wire::lua::pushHostFunction(lua, "store.index", [target, pluginId](LuaArgs& args) {
        const std::string key = args.checkString(2);
        if (key == "top") {
            pushTop(args, *target, pluginId);
            return 1;
        }
        const std::optional<std::string> stored = target->services().storage().playerValue(pluginId, kServerScope, key);
        if (!stored.has_value()) {
            args.pushNil();
        } else {
            wire::plugins::pushValue(args, *stored);
        }
        return 1;
    });
    lua.setField("__index");
    wire::lua::pushHostFunction(lua, "store.newindex", [target, pluginId](LuaArgs& args) {
        const std::string key = args.checkString(2);
        target->services().storage().setPlayerValue(pluginId, kServerScope, key, storedValue(args));
        return 0;
    });
    lua.setField("__newindex");
    lua.setMetatable(-2);
}

std::string jsonPathOf(const std::string& folder, const std::string& name) {
    if (name.find('/') != std::string::npos || name.ends_with(".json")) {
        return folder + "/" + name;
    }
    return folder + "/data/" + name + ".json";
}

void pushData(LuaArgs& lua, const std::string& folder) {
    const auto loaded = std::make_shared<std::map<std::string, wire::lua::LuaRef, std::less<>>>();
    lua.pushTable();
    wire::lua::pushHostFunction(lua, "data.json", [folder, loaded](LuaArgs& args) {
        const std::string name = args.checkString(firstArgument(args));
        if (name.starts_with('/') || name.find("..") != std::string::npos) {
            args.raise("'" + name + "' is outside the plugin folder");
        }
        if (const auto held = loaded->find(name); held != loaded->end()) {
            held->second.push(args);
            return 1;
        }
        const std::optional<std::string> text = wire::io::fileIO::readText(jsonPathOf(folder, name));
        if (text.has_value()) {
            pluginJson::parse(args, *text);
        } else {
            args.pushTable();
        }
        loaded->emplace(name, wire::lua::LuaRef::of(args.state(), -1));
        return 1;
    });
    lua.setField("json");
    wire::lua::pushHostFunction(lua, "data.save", [folder, loaded](LuaArgs& args) {
        const std::string name = args.checkString(firstArgument(args));
        const auto held = loaded->find(name);
        if (held == loaded->end()) {
            args.pushBoolean(false);
            return 1;
        }
        const std::string path = jsonPathOf(folder, name);
        const std::size_t slash = path.rfind('/');
        if (slash != std::string::npos) {
            wire::io::fileIO::createDirectories(path.substr(0, slash));
        }
        held->second.push(args);
        const std::string text = pluginJson::write(args, args.top());
        args.pop(1);
        args.pushBoolean(wire::io::fileIO::writeText(path, text));
        return 1;
    });
    lua.setField("save");
}
} // namespace basalt::server::plugins
