#include "PluginMiscShared.hpp"

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
    wire::lua::pushHostFunction(lua, "recipes.get", [server](LuaArgs& args) {
        misc::pushSnbt(args, server->services().inventories().recipeData(args.checkString(1)));
        return 1;
    });
    lua.setField("get");
    wire::lua::pushHostFunction(lua, "recipes.craft", [server](LuaArgs& args) {
        const std::optional<std::vector<std::uint8_t>> items = misc::snbtBytes(args, 3);
        misc::pushSnbt(args, !items.has_value() ? std::nullopt
                                                : server->services().inventories().craft(
                                                      static_cast<std::int32_t>(args.checkInteger(1)),
                                                      static_cast<std::int32_t>(args.checkInteger(2)), *items));
        return 1;
    });
    lua.setField("craft");
    wire::lua::pushHostFunction(lua, "recipes.anvil", [server](LuaArgs& args) {
        const std::optional<std::vector<std::uint8_t>> left = misc::snbtBytes(args, 1);
        const std::optional<std::vector<std::uint8_t>> right = misc::snbtBytes(args, 2);
        const std::string name = args.isString(3) ? args.checkString(3) : std::string{};
        misc::pushSnbt(args, !left.has_value() || !right.has_value()
                                 ? std::nullopt
                                 : server->services().inventories().anvil(*left, *right, name));
        return 1;
    });
    lua.setField("anvil");
    wire::lua::pushHostFunction(lua, "recipes.smithing", [server](LuaArgs& args) {
        const std::optional<std::vector<std::uint8_t>> first = misc::snbtBytes(args, 1);
        const std::optional<std::vector<std::uint8_t>> second = misc::snbtBytes(args, 2);
        const std::optional<std::vector<std::uint8_t>> third = misc::snbtBytes(args, 3);
        misc::pushSnbt(args, !first.has_value() || !second.has_value() || !third.has_value()
                                 ? std::nullopt
                                 : server->services().inventories().smithing(*first, *second, *third));
        return 1;
    });
    lua.setField("smithing");
    lua.setGlobal("recipes");

    lua.pushTable();
    wire::lua::pushHostFunction(lua, "registries.item_data", [server](LuaArgs& args) {
        misc::pushSnbt(args, server->services().inventories().itemData(args.checkString(1)));
        return 1;
    });
    lua.setField("item_data");
    wire::lua::pushHostFunction(lua, "registries.enchantment_data", [server](LuaArgs& args) {
        misc::pushSnbt(args, server->services().inventories().enchantmentData(args.checkString(1)));
        return 1;
    });
    lua.setField("enchantment_data");
    wire::lua::pushHostFunction(lua, "registries.enchantment_ids", [server](LuaArgs& args) {
        misc::pushNames(args, server->services().inventories().enchantmentIds());
        return 1;
    });
    lua.setField("enchantment_ids");
    wire::lua::pushHostFunction(lua, "registries.tags", [server](LuaArgs& args) {
        misc::pushNames(args, server->services().inventories().registryTags(args.checkString(1)));
        return 1;
    });
    lua.setField("tags");
    wire::lua::pushHostFunction(lua, "registries.mob_effects", [server](LuaArgs& args) {
        misc::pushSnbt(args, server->services().inventories().mobEffects());
        return 1;
    });
    lua.setField("mob_effects");
    wire::lua::pushHostFunction(lua, "registries.potions", [server](LuaArgs& args) {
        misc::pushSnbt(args, server->services().inventories().potions());
        return 1;
    });
    lua.setField("potions");
    lua.setGlobal("registries");
}
} // namespace basalt::server::plugins::pluginApi
