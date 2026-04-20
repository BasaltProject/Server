#include <basalt/server/plugins/PluginHost.hpp>
#include <basalt/server/plugins/PluginObjects.hpp>
#include <basalt/server/plugins/PluginServiceRegistry.hpp>
#include <basalt/wire/lua/LuaCall.hpp>

#include <cstdint>
#include <utility>

namespace basalt::server::plugins {
namespace {
using wire::lua::LuaArgs;
} // namespace

std::set<std::string> PluginServiceRegistry::names() const {
    std::set<std::string> out;
    for (const auto& [name, offer] : offers_) {
        out.insert(name);
    }
    return out;
}

PluginHandle PluginServiceRegistry::provide(LuaArgs& lua, std::int32_t index, const std::string& pluginId,
                                            const std::string& name) {
    if (!lua.isTable(index)) {
        lua.raise("service('" + name + "', ...) needs a table of functions");
    }
    Offer offer{.pluginId = pluginId};
    lua.pushNil();
    while (lua.nextEntry(index)) {
        if (lua.isFunction(-1)) {
            offer.functions.emplace(lua.toText(-2),
                                    std::make_shared<wire::lua::LuaRef>(wire::lua::LuaRef::of(lua.state(), -1)));
        }
        lua.pop(1);
    }
    if (offer.functions.empty()) {
        lua.raise("service('" + name + "', ...) has no functions in it");
    }
    offers_.insert_or_assign(name, std::move(offer));
    return PluginHandle{[this, name, pluginId] {
        const auto found = offers_.find(name);
        if (found != offers_.end() && found->second.pluginId == pluginId) {
            offers_.erase(found);
        }
    }};
}

void PluginServiceRegistry::consume(LuaArgs& lua, PluginHost& host, const std::string& name) {
    const auto offer = offers_.find(name);
    LoadedPlugin* provider = offer == offers_.end() ? nullptr : host.pluginById(offer->second.pluginId);
    if (provider == nullptr) {
        lua.pushNil();
        return;
    }
    lua.pushTable();
    PluginHost* target = &host;
    for (const auto& [functionName, function] : offer->second.functions) {
        const std::shared_ptr<wire::lua::LuaRef> held = function;
        const std::string providerId = offer->second.pluginId;
        wire::lua::pushHostFunction(lua, name + "." + functionName, [target, providerId, held](LuaArgs& args) {
            LoadedPlugin* owner = target->pluginById(providerId);
            if (owner == nullptr) {
                args.raise("the plugin behind this service is gone");
            }
            const std::vector<PluginCrossValue> copied = crossArguments(args);
            return callService(*target, *owner, *held, copied, args) ? 1 : 0;
        });
        lua.setField(functionName);
    }
}

void pushCrossed(LuaArgs& lua, const PluginCrossValue& value, PluginHost& host, LoadedPlugin& plugin) {
    if (std::holds_alternative<std::monostate>(value.value)) {
        lua.pushNil();
        return;
    }
    if (const auto* flag = std::get_if<bool>(&value.value); flag != nullptr) {
        lua.pushBoolean(*flag);
        return;
    }
    if (const auto* whole = std::get_if<std::int64_t>(&value.value); whole != nullptr) {
        lua.pushInteger(*whole);
        return;
    }
    if (const auto* real = std::get_if<double>(&value.value); real != nullptr) {
        lua.pushNumber(*real);
        return;
    }
    if (const auto* text = std::get_if<std::string>(&value.value); text != nullptr) {
        lua.pushString(*text);
        return;
    }
    const auto* table = std::get_if<PluginCrossTable>(&value.value);
    if (table == nullptr) {
        lua.pushNil();
        return;
    }
    const auto marked = table->find(kPlayerMark);
    if (marked != table->end() && table->size() == 1) {
        const auto* uuid = std::get_if<std::string>(&marked->second->value);
        const std::shared_ptr<PluginPlayerRef> player =
            uuid == nullptr ? nullptr : host.services().players().byUuid(*uuid);
        if (player == nullptr) {
            lua.pushNil();
        } else {
            pushPlayer(lua, player, &host.services().storage(), plugin.manifest.id, &plugin.text);
        }
        return;
    }
    lua.pushTable();
    for (const auto& [key, element] : *table) {
        pushCrossed(lua, *element, host, plugin);
        lua.setField(key);
    }
}

bool callService(PluginHost& host, LoadedPlugin& provider, const wire::lua::LuaRef& function,
                 const std::vector<PluginCrossValue>& arguments, LuaArgs& caller) {
    if (provider.disabled) {
        caller.raise("the plugin behind this service is off");
    }
    PluginCrossValue answered;
    const bool ran = host.ask(
        provider, function, "service",
        [&arguments, &host, &provider](LuaArgs& args) {
            for (const PluginCrossValue& argument : arguments) {
                pushCrossed(args, argument, host, provider);
            }
            return static_cast<std::int32_t>(arguments.size());
        },
        [&answered](LuaArgs& args) { answered = crossOut(args, args.top()); });
    if (!ran) {
        caller.raise("the service failed inside " + provider.manifest.id);
    }
    pushCrossed(caller, answered, host, provider);
    return true;
}
} // namespace basalt::server::plugins
