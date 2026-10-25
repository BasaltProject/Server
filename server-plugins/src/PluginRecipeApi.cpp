#include <basalt/server/plugins/PluginRecipeApi.hpp>
#include <basalt/wire/lua/LuaCall.hpp>

namespace basalt::server::plugins::pluginApi {
using wire::lua::LuaArgs;

void installRecipes(LuaArgs& lua, PluginHost& host) {
    PluginHost* server = &host;
    lua.pushTable();
    wire::lua::pushHostFunction(lua, "recipes.add", [server](LuaArgs& args) {
        args.pushBoolean(server->services().inventories().addRecipeJson(args.checkString(1), args.checkString(2)));
        return 1;
    });
    lua.setField("add");
    wire::lua::pushHostFunction(lua, "recipes.remove", [server](LuaArgs& args) {
        args.pushBoolean(server->services().inventories().removeRecipe(args.checkString(1)));
        return 1;
    });
    lua.setField("remove");
    wire::lua::pushHostFunction(lua, "recipes.has", [server](LuaArgs& args) {
        args.pushBoolean(server->services().inventories().recipeData(args.checkString(1)).has_value());
        return 1;
    });
    lua.setField("has");
    wire::lua::pushHostFunction(lua, "recipes.ids", [server](LuaArgs& args) {
        args.pushTable();
        std::int64_t position = 1;
        for (const std::string& id : server->services().inventories().recipeIds()) {
            args.pushString(id);
            args.setIndex(position++);
        }
        return 1;
    });
    lua.setField("ids");
    wire::lua::pushHostFunction(lua, "recipes.clear", [server](LuaArgs& args) {
        static_cast<void>(args);
        server->services().inventories().clearRecipes();
        return 0;
    });
    lua.setField("clear");
    wire::lua::pushHostFunction(lua, "recipes.reset", [server](LuaArgs& args) {
        static_cast<void>(args);
        server->services().inventories().resetRecipes();
        return 0;
    });
    lua.setField("reset");
    wire::lua::pushHostFunction(lua, "recipes.reset_brewing", [server](LuaArgs& args) {
        static_cast<void>(args);
        server->services().inventories().resetBrewingRecipes();
        return 0;
    });
    lua.setField("reset_brewing");
    lua.setGlobal("recipes");
}
} // namespace basalt::server::plugins::pluginApi
