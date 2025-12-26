#include <basalt/server/lua/LuaCall.hpp>
#include <basalt/server/plugins/PluginObject.hpp>

#include <algorithm>
#include <memory>
#include <set>
#include <utility>

namespace basalt::server::plugins {
namespace {

/// What the two metatable functions share, kept alive by the closures that hold it.
struct Fields {
    std::string kind;
    std::map<std::string, PluginObject::Reader, std::less<>> readers;
    std::map<std::string, PluginObject::Pusher, std::less<>> pushers;
    std::map<std::string, PluginObject::Writer, std::less<>> writers;
    std::map<std::string, PluginObject::Method, std::less<>> methods;

    [[nodiscard]] std::string names() const {
        std::set<std::string> all;
        for (const auto& [name, reader] : readers) {
            all.insert(name);
        }
        for (const auto& [name, pusher] : pushers) {
            all.insert(name);
        }
        for (const auto& [name, method] : methods) {
            all.insert(name);
        }
        std::string out;
        for (const std::string& name : all) {
            out += out.empty() ? name : ", " + name;
        }
        return out;
    }
};

} // namespace

void pushValue(lua::LuaArgs& lua, const PluginValue& value) {
    if (std::holds_alternative<std::monostate>(value)) {
        lua.pushNil();
    } else if (const auto* flag = std::get_if<bool>(&value); flag != nullptr) {
        lua.pushBoolean(*flag);
    } else if (const auto* whole = std::get_if<std::int64_t>(&value); whole != nullptr) {
        lua.pushInteger(*whole);
    } else if (const auto* real = std::get_if<double>(&value); real != nullptr) {
        lua.pushNumber(*real);
    } else if (const auto* text = std::get_if<std::string>(&value); text != nullptr) {
        lua.pushString(*text);
    } else if (const auto* list = std::get_if<std::vector<std::string>>(&value); list != nullptr) {
        lua.pushTable();
        std::int64_t position = 1;
        for (const std::string& element : *list) {
            lua.pushString(element);
            lua.setIndex(position++);
        }
    }
}

std::int32_t firstArgument(const lua::LuaArgs& lua) { return lua.count() >= 1 && lua.isTable(1) ? 2 : 1; }

PluginObject& PluginObject::reads(std::string name, Reader reader) {
    readers_.emplace(std::move(name), std::move(reader));
    return *this;
}

PluginObject& PluginObject::pushes(std::string name, Pusher pusher) {
    pushers_.emplace(std::move(name), std::move(pusher));
    return *this;
}

PluginObject& PluginObject::writes(std::string name, Writer writer) {
    writers_.emplace(std::move(name), std::move(writer));
    return *this;
}

PluginObject& PluginObject::method(std::string name, Method body) {
    methods_.emplace(std::move(name), std::move(body));
    return *this;
}

void PluginObject::push(lua::LuaArgs& lua) && {
    auto fields = std::make_shared<Fields>(Fields{.kind = std::move(kind_),
                                                  .readers = std::move(readers_),
                                                  .pushers = std::move(pushers_),
                                                  .writers = std::move(writers_),
                                                  .methods = std::move(methods_)});
    lua.pushTable();
    lua.pushTable();
    lua::pushHostFunction(lua, fields->kind + ".index", [fields](lua::LuaArgs& args) {
        const std::string key = args.checkString(2);
        if (const auto method = fields->methods.find(key); method != fields->methods.end()) {
            lua::pushHostFunction(args, fields->kind + "." + key, method->second);
            return 1;
        }
        if (const auto pusher = fields->pushers.find(key); pusher != fields->pushers.end()) {
            pusher->second(args);
            return 1;
        }
        const auto reader = fields->readers.find(key);
        if (reader == fields->readers.end()) {
            args.raise("a " + fields->kind + " has no '" + key + "'; it has " + fields->names());
        }
        pushValue(args, reader->second());
        return 1;
    });
    lua.setField("__index");
    lua::pushHostFunction(lua, fields->kind + ".newindex", [fields](lua::LuaArgs& args) {
        const std::string key = args.checkString(2);
        const auto writer = fields->writers.find(key);
        if (writer == fields->writers.end()) {
            const bool readable = fields->readers.contains(key) || fields->pushers.contains(key);
            args.raise(readable ? "'" + key + "' of a " + fields->kind + " can be read but not changed"
                                : "a " + fields->kind + " has no '" + key + "' to change");
        }
        writer->second(args, 3);
        return 0;
    });
    lua.setField("__newindex");
    const std::string kind = fields->kind;
    lua::pushHostFunction(lua, kind + ".tostring", [kind](lua::LuaArgs& args) {
        args.pushString(kind);
        return 1;
    });
    lua.setField("__tostring");
    lua.setMetatable(-2);
}

} // namespace basalt::server::plugins
