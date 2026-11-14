#include "PluginApiShared.hpp"
#include "PluginCallGate.hpp"

#include <basalt/server/plugins/PluginHost.hpp>
#include <basalt/server/plugins/PluginObjects.hpp>
#include <basalt/server/plugins/PluginPermissions.hpp>
#include <basalt/wire/lua/LuaCall.hpp>

#include <atomic>
#include <memory>

namespace basalt::server::plugins {
namespace permissions {
namespace {
class Unavailable final : public PluginPermissions {
public:
    PluginHandle provide(std::string_view, std::int32_t, Check, Meta) override { return {}; }

    void declare(std::string_view, std::int32_t, std::span<const std::string_view>, std::span<const bool>) override {}

    void undeclare(std::string_view) override {}

    std::int32_t check(std::string_view, std::string_view) override { return kUndefined; }

    std::optional<std::string> meta(std::string_view, std::string_view) override { return std::nullopt; }

    void changed() override {}
};
} // namespace

PluginPermissions& PluginPermissions::unavailable() {
    static Unavailable kNone;
    return kNone;
}
} // namespace permissions

namespace pluginApi::internal {
namespace {
struct Provided {
    std::shared_ptr<LuaRef> check;
    std::shared_ptr<LuaRef> meta;
    PluginCallGate gate;
};

void pushAsked(LuaArgs& lua, PluginPlayers& players, std::string_view uuid, std::string_view text) {
    const std::shared_ptr<PluginPlayerRef> player = uuid.empty() ? nullptr : players.byUuid(uuid);
    lua.pushTable();
    lua.pushString(player == nullptr ? (uuid.empty() ? "console" : "") : player->name());
    lua.setField("name");
    lua.pushString(uuid);
    lua.setField("uuid");
    lua.pushBoolean(uuid.empty());
    lua.setField("console");
    lua.pushString(text);
    lua.pushTable();
    if (player != nullptr) {
        lua.pushString(player->world());
        lua.setField("world");
    }
}

[[nodiscard]] std::string uuidOf(LuaArgs& args, std::int32_t index, PluginPlayers& players) {
    if (args.isString(index)) {
        const std::shared_ptr<PluginPlayerRef> named = players.byName(args.toText(index));
        return named == nullptr ? std::string{} : named->uuid();
    }
    const std::shared_ptr<PluginPlayerRef> player = playerFrom(args, index, players);
    return player == nullptr ? std::string{} : player->uuid();
}

[[nodiscard]] bool namesSomeone(LuaArgs& args, std::int32_t index) {
    return !args.isString(index) || args.toText(index) != "console";
}
} // namespace

void installPermissions(LuaArgs& lua, LoadedPlugin& plugin, PluginHost& host) {
    LoadedPlugin* target = &plugin;
    PluginHost* server = &host;
    lua.pushTable();

    wire::lua::pushHostFunction(lua, "permissions.check", [server](LuaArgs& args) {
        PluginPlayers& players = server->services().players();
        const std::string uuid = uuidOf(args, 1, players);
        const std::string node = args.checkString(2);
        const std::int32_t answer = uuid.empty() && namesSomeone(args, 1)
                                        ? permissions::kUndefined
                                        : server->services().permissions().check(uuid, node);
        if (answer == permissions::kUndefined) {
            args.pushNil();
        } else {
            args.pushBoolean(answer == permissions::kTrue);
        }
        return 1;
    });
    lua.setField("check");

    wire::lua::pushHostFunction(lua, "permissions.meta", [server](LuaArgs& args) {
        PluginPlayers& players = server->services().players();
        const std::string uuid = uuidOf(args, 1, players);
        const std::string key = args.checkString(2);
        const std::optional<std::string> value =
            uuid.empty() && namesSomeone(args, 1) ? std::nullopt : server->services().permissions().meta(uuid, key);
        if (value.has_value()) {
            args.pushString(*value);
        } else {
            args.pushNil();
        }
        return 1;
    });
    lua.setField("meta");

    wire::lua::pushHostFunction(lua, "permissions.changed", [server](LuaArgs&) {
        server->services().permissions().changed();
        return 0;
    });
    lua.setField("changed");

    wire::lua::pushHostFunction(lua, "permissions.provide", [server, target](LuaArgs& args) {
        auto provided = std::make_shared<Provided>();
        if (args.isFunction(1)) {
            provided->check = std::make_shared<LuaRef>(LuaRef::of(args.state(), 1));
        } else if (args.isTable(1)) {
            args.pushField(1, "check");
            if (args.isFunction(-1)) {
                provided->check = std::make_shared<LuaRef>(LuaRef::of(args.state(), -1));
            }
            args.pop(1);
            args.pushField(1, "meta");
            if (args.isFunction(-1)) {
                provided->meta = std::make_shared<LuaRef>(LuaRef::of(args.state(), -1));
            }
            args.pop(1);
        }
        if (provided->check == nullptr && provided->meta == nullptr) {
            args.raise("permissions.provide(fn) needs a function or a table with check and meta");
        }
        std::int32_t priority = permissions::kPriorityPlugin;
        if (args.isTable(2)) {
            args.pushField(2, "priority");
            if (args.isInteger(-1)) {
                priority = static_cast<std::int32_t>(args.toInteger(-1));
            }
            args.pop(1);
            if (priority <= 0) {
                args.raise("a plugin provider's priority must be above 0");
            }
        }
        permissions::Check check;
        if (provided->check != nullptr) {
            check = [server, target, provided](std::string_view uuid, std::string_view node) {
                std::int32_t answer = permissions::kUndefined;
                const PluginCallGate::Pass pass{provided->gate};
                if (!pass.entered()) {
                    return answer;
                }
                provided->gate.run(*server, [&] {
                    static_cast<void>(server->ask(
                        *target, *provided->check, "permission provider",
                        [server, uuid, node](LuaArgs& lua) {
                            pushAsked(lua, server->services().players(), uuid, node);
                            return 3;
                        },
                        [&answer](LuaArgs& lua) {
                            if (lua.isBoolean(-1)) {
                                answer = lua.toBoolean(-1) ? permissions::kTrue : permissions::kFalse;
                            }
                        }));
                });
                return answer;
            };
        }
        permissions::Meta meta;
        if (provided->meta != nullptr) {
            meta = [server, target, provided](std::string_view uuid,
                                              std::string_view key) -> std::optional<std::string> {
                std::optional<std::string> value;
                const PluginCallGate::Pass pass{provided->gate};
                if (!pass.entered()) {
                    return value;
                }
                provided->gate.run(*server, [&] {
                    static_cast<void>(server->ask(
                        *target, *provided->meta, "permission meta",
                        [server, uuid, key](LuaArgs& lua) {
                            pushAsked(lua, server->services().players(), uuid, key);
                            return 3;
                        },
                        [&value](LuaArgs& lua) {
                            if (lua.isString(-1)) {
                                value = lua.toText(-1);
                            }
                        }));
                });
                return value;
            };
        }
        PluginHandle handle = server->services().permissions().provide("lua:" + target->manifest.id, priority,
                                                                       std::move(check), std::move(meta));
        const auto registered = std::make_shared<PluginHandle>(
            target->registrations.add("permissions.provide", [server, provided, moved = std::move(handle)]() mutable {
                moved.remove();
                provided->gate.close(*server);
                server->underLock([&provided] {
                    for (const std::shared_ptr<LuaRef>* held : {&provided->check, &provided->meta}) {
                        if (*held != nullptr) {
                            (*held)->release();
                        }
                    }
                });
            }));
        args.pushTable();
        const auto removeIt = [registered](LuaArgs&) {
            registered->remove();
            return 0;
        };
        wire::lua::pushHostFunction(args, "remove", removeIt);
        args.setField("remove");
        wire::lua::pushHostFunction(args, "cancel", removeIt);
        args.setField("cancel");
        return 1;
    });
    lua.setField("provide");
    lua.setGlobal("permissions");
}
} // namespace pluginApi::internal
} // namespace basalt::server::plugins
