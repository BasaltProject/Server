#include <basalt/server/plugins/PluginObjects.hpp>
#include <basalt/server/plugins/PluginPlayerOps.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace basalt::server::plugins {

void bindPlayerOperations(PluginObject& object, const std::shared_ptr<PluginPlayerRef>& player) {
    object.method("get", [player](wire::lua::LuaArgs& args) {
        pushValue(args, player->property(args.checkString(firstArgument(args))).value_or(PluginValue{}));
        return 1;
    });
    object.method("set", [player](wire::lua::LuaArgs& args) {
        const std::int32_t base = firstArgument(args);
        args.pushBoolean(player->setProperty(args.checkString(base), entityValueAt(args, base + 1)));
        return 1;
    });
    object.method("call", [player](wire::lua::LuaArgs& args) {
        const std::int32_t base = firstArgument(args);
        std::vector<PluginEntityValue> arguments;
        for (std::int32_t index = base + 1; index <= args.count(); ++index) {
            arguments.push_back(entityValueAt(args, index));
        }
        pushValue(args, player->invoke(args.checkString(base), arguments).value_or(PluginEntityValue{}));
        return 1;
    });
    auto lookup = std::make_shared<PluginObject::Fallback>();
    lookup->names = "the player properties and operations of the player ledger";
    lookup->reader = [player](std::string_view name) -> std::optional<PluginObject::Reader> {
        if (playerPropertyNamed(name) == nullptr) {
            return std::nullopt;
        }
        return PluginObject::Reader{
            [player, key = std::string{name}] { return player->property(key).value_or(PluginValue{}); }};
    };
    lookup->writer = [player](std::string_view name) -> std::optional<PluginObject::Writer> {
        const EntityPropertySpec* spec = playerPropertyNamed(name);
        if (spec == nullptr || !spec->writable) {
            return std::nullopt;
        }
        return PluginObject::Writer{[player, key = std::string{name}](wire::lua::LuaArgs& args, std::int32_t index) {
            if (!player->setProperty(key, entityValueAt(args, index))) {
                args.raise("player cannot set " + key);
            }
        }};
    };
    lookup->method = [player](std::string_view name) -> std::optional<PluginObject::Method> {
        if (!isPlayerOperation(name)) {
            return std::nullopt;
        }
        return PluginObject::Method{[player, key = std::string{name}](wire::lua::LuaArgs& args) {
            std::vector<PluginEntityValue> arguments;
            for (std::int32_t index = firstArgument(args); index <= args.count(); ++index) {
                arguments.push_back(entityValueAt(args, index));
            }
            pushValue(args, player->invoke(key, arguments).value_or(PluginEntityValue{}));
            return 1;
        }};
    };
    object.fallback(std::move(lookup));
}

} // namespace basalt::server::plugins
