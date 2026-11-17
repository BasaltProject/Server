#include "PluginApiShared.hpp"
#include "PluginCallGate.hpp"

#include <basalt/server/plugins/PluginHost.hpp>
#include <basalt/server/plugins/PluginObjects.hpp>
#include <basalt/wire/auth/MojangSessionService.hpp>
#include <basalt/wire/lua/LuaCall.hpp>

#include <memory>
#include <optional>
#include <thread>

namespace basalt::server::plugins::pluginApi::internal {
namespace {
struct Found {
    std::string uuid;
    std::string name;
    std::string texture;
    std::string signature;
};

std::optional<Found> lookup(const std::string& name) {
    const std::optional<std::string> uuid = wire::auth::mojangUuidOf(name);
    if (!uuid.has_value()) {
        return std::nullopt;
    }
    const std::optional<wire::auth::SessionProfile> profile = wire::auth::mojangProfileOf(*uuid);
    Found found{.uuid = *uuid, .name = name, .texture = {}, .signature = {}};
    if (profile.has_value()) {
        found.name = profile->name;
        for (const wire::auth::SessionProfileProperty& property : profile->properties) {
            if (property.name == "textures") {
                found.texture = property.value;
                found.signature = property.signature.value_or("");
                break;
            }
        }
    }
    return found;
}
} // namespace

void installProfiles(LuaArgs& lua, PluginInstance& instance, LoadedPlugin& plugin, PluginHost& host) {
    LoadedPlugin* target = &plugin;
    PluginInstance* lane = &instance;
    PluginHost* server = &host;
    lua.pushTable();
    wire::lua::pushHostFunction(lua, "profiles.lookup", [server, target, lane](LuaArgs& args) {
        const std::string name = args.checkString(1);
        if (!args.isFunction(2)) {
            args.raise("profiles.lookup(name, fn) needs a function");
        }
        const auto fn = std::make_shared<wire::lua::LuaRef>(wire::lua::LuaRef::of(args.state(), 2));
        const auto gate = std::make_shared<PluginCallGate>();
        static_cast<void>(target->registrations.add("profiles.lookup(" + name + ")", [server, fn, gate]() {
            gate->close(*server);
            server->underLock([&fn] { fn->release(); });
        }));
        // The worker only touches the plugin through the gate, which unloading closes first.
        std::thread{[server, target, lane, fn, gate, name] {
            const std::optional<Found> found = lookup(name);
            const PluginCallGate::Pass pass{*gate};
            if (!pass.entered()) {
                return;
            }
            gate->run(*server, [&] {
                server->enter(*target, *lane, *fn, "profiles.lookup", [&found](LuaArgs& out) {
                    if (!found.has_value()) {
                        out.pushNil();
                        return 1;
                    }
                    out.pushTable();
                    out.pushString(found->uuid);
                    out.setField("uuid");
                    out.pushString(found->name);
                    out.setField("name");
                    out.pushString(found->texture);
                    out.setField("texture");
                    out.pushString(found->signature);
                    out.setField("signature");
                    return 1;
                });
                fn->release();
            });
        }}.detach();
        return 0;
    });
    lua.setField("lookup");
    lua.setGlobal("profiles");
}
} // namespace basalt::server::plugins::pluginApi::internal
