#include <basalt/server/plugins/PluginObjects.hpp>

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace basalt::server::plugins {
namespace {

[[nodiscard]] PluginEntityValue valueAt(wire::lua::LuaArgs& lua, std::int32_t index) {
    if (lua.isBoolean(index)) {
        return lua.toBoolean(index);
    }
    if (lua.isInteger(index)) {
        return lua.toInteger(index);
    }
    if (lua.isNumber(index)) {
        return lua.toNumber(index);
    }
    if (lua.isString(index)) {
        return lua.toText(index);
    }
    if (lua.isTable(index)) {
        std::vector<std::string> list;
        const std::int64_t length = lua.length(index);
        for (std::int64_t position = 1; position <= length; ++position) {
            lua.pushIndex(index, position);
            list.push_back(lua.toText(-1));
            lua.pop(1);
        }
        return list;
    }
    return std::monostate{};
}

[[nodiscard]] std::vector<std::string_view> split(std::string_view text) {
    std::vector<std::string_view> parts;
    while (!text.empty()) {
        const std::size_t comma = text.find(',');
        parts.push_back(text.substr(0, comma));
        text = comma == std::string_view::npos ? std::string_view{} : text.substr(comma + 1);
    }
    return parts;
}

} // namespace

void bindEntityOperations(PluginObject& object, const std::shared_ptr<PluginEntityRef>& entity) {
    object.method("get", [entity](wire::lua::LuaArgs& args) {
        pushValue(args, entity->property(args.checkString(firstArgument(args))).value_or(PluginValue{}));
        return 1;
    });
    object.method("call", [entity](wire::lua::LuaArgs& args) {
        const std::int32_t base = firstArgument(args);
        std::vector<PluginEntityValue> arguments;
        for (std::int32_t index = base + 1; index <= args.count(); ++index) {
            arguments.push_back(valueAt(args, index));
        }
        pushValue(args, entity->invoke(args.checkString(base), arguments).value_or(PluginEntityValue{}));
        return 1;
    });
    object.method("set", [entity](wire::lua::LuaArgs& args) {
        const std::int32_t base = firstArgument(args);
        args.pushBoolean(entity->setProperty(args.checkString(base), valueAt(args, base + 1)));
        return 1;
    });
    for (const EntityPropertySpec& spec : kEntityProperties) {
        const std::string name{spec.name};
        object.reads(name, [entity, name] { return entity->property(name).value_or(PluginValue{}); });
        if (spec.writable) {
            object.writes(name, [entity, name](wire::lua::LuaArgs& args, std::int32_t index) {
                if (!entity->setProperty(name, valueAt(args, index))) {
                    static_cast<void>(args.fail("entity cannot set " + name));
                }
            });
        }
    }
    for (const EntityOperationSpec& spec : kEntityOperations) {
        const std::string name{spec.name};
        const std::vector<std::string_view> columns = split(spec.columns);
        std::vector<std::string> columnNames{columns.begin(), columns.end()};
        object.method(name, [entity, name, columnNames = std::move(columnNames)](wire::lua::LuaArgs& args) {
            std::vector<PluginEntityValue> arguments;
            for (std::int32_t index = firstArgument(args); index <= args.count(); ++index) {
                arguments.push_back(valueAt(args, index));
            }
            const std::optional<PluginEntityValue> result = entity->invoke(name, arguments);
            const auto* rows = result.has_value() ? std::get_if<std::vector<std::string>>(&*result) : nullptr;
            if (columnNames.empty() || rows == nullptr) {
                pushValue(args, result.value_or(PluginEntityValue{}));
                return 1;
            }
            args.pushTable();
            std::int64_t position = 1;
            for (std::size_t at = 0; at + columnNames.size() <= rows->size(); at += columnNames.size()) {
                args.pushTable();
                for (std::size_t column = 0; column < columnNames.size(); ++column) {
                    args.pushString((*rows)[at + column]);
                    args.setField(columnNames[column]);
                }
                args.setIndex(position++);
            }
            return 1;
        });
    }
}

} // namespace basalt::server::plugins
