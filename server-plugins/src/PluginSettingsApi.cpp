#include <basalt/server/plugins/PluginHost.hpp>
#include <basalt/server/plugins/PluginMiscApi.hpp>
#include <basalt/wire/lua/LuaCall.hpp>

namespace basalt::server::plugins::pluginApi {
namespace {
using wire::lua::LuaArgs;
} // namespace

void installSettings(LuaArgs& lua, PluginHost& host) {
    PluginHost* server = &host;
    lua.pushTable();
    wire::lua::pushHostFunction(lua, "settings.get", [server](LuaArgs& args) {
        const std::optional<std::string> value = server->services().serverControl().setting(args.checkString(1));
        if (value.has_value()) {
            args.pushString(*value);
        } else {
            args.pushNil();
        }
        return 1;
    });
    lua.setField("get");
    wire::lua::pushHostFunction(lua, "settings.set", [server](LuaArgs& args) {
        args.pushBoolean(server->services().serverControl().setSetting(args.checkString(1), args.toText(2)));
        return 1;
    });
    lua.setField("set");
    wire::lua::pushHostFunction(lua, "settings.keys", [server](LuaArgs& args) {
        args.pushTable();
        std::int64_t position = 1;
        for (const std::string& key : server->services().serverControl().settingKeys()) {
            args.pushString(key);
            args.setIndex(position++);
        }
        return 1;
    });
    lua.setField("keys");
    lua.setGlobal("settings");
}

} // namespace basalt::server::plugins::pluginApi
