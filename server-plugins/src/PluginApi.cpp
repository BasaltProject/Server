#include <basalt/server/plugins/PluginApi.hpp>
#include <basalt/server/plugins/PluginEventTypes.hpp>
#include <basalt/server/plugins/PluginFormObject.hpp>
#include <basalt/server/plugins/PluginMenuObject.hpp>
#include <basalt/server/plugins/PluginObject.hpp>
#include <basalt/server/plugins/PluginObjects.hpp>
#include <basalt/server/plugins/PluginServiceRegistry.hpp>
#include <basalt/server/plugins/PluginStore.hpp>
#include <basalt/wire/io/FileIO.hpp>
#include <basalt/wire/lua/LuaCall.hpp>
#include <basalt/wire/plugins/PluginToml.hpp>

#include <algorithm>
#include <cstdint>
#include <utility>

namespace basalt::server::plugins::pluginApi {
namespace {
using wire::lua::LuaArgs;
using wire::lua::LuaRef;

[[nodiscard]] std::optional<std::string> stringField(LuaArgs& lua, std::int32_t index, std::string_view key) {
    lua.pushField(index, key);
    std::optional<std::string> value;
    if (lua.isString(-1)) {
        value = lua.toText(-1);
    }
    lua.pop(1);
    return value;
}

void pushHandleTable(LuaArgs& lua, PluginHandle handle) {
    const auto held = std::make_shared<PluginHandle>(std::move(handle));
    lua.pushTable();
    wire::lua::pushHostFunction(lua, "cancel", [held](LuaArgs& args) {
        static_cast<void>(args);
        held->remove();
        return 0;
    });
    lua.setField("cancel");
}

void installIdentity(LuaArgs& lua, PluginInstance& instance, LoadedPlugin& plugin, std::int32_t lanes) {
    lua.pushTable();
    lua.pushString(plugin.manifest.id);
    lua.setField("id");
    lua.pushString(plugin.manifest.name);
    lua.setField("name");
    lua.pushString(plugin.manifest.version);
    lua.setField("version");
    lua.pushInteger(instance.lane);
    lua.setField("lane");
    lua.pushInteger(lanes);
    lua.setField("lanes");
    lua.setGlobal("plugin");

    LoadedPlugin* target = &plugin;
    lua.pushTable();
    wire::lua::pushHostFunction(lua, "log.info", [target](LuaArgs& args) {
        target->log.info([&args] { return args.toText(1); });
        return 0;
    });
    lua.setField("info");
    wire::lua::pushHostFunction(lua, "log.warn", [target](LuaArgs& args) {
        target->log.warn([&args] { return args.toText(1); });
        return 0;
    });
    lua.setField("warn");
    wire::lua::pushHostFunction(lua, "log.error", [target](LuaArgs& args) {
        target->log.error([&args] { return args.toText(1); });
        return 0;
    });
    lua.setField("error");
    lua.setGlobal("log");

    wire::plugins::pushToml(lua, plugin.config);
    lua.setGlobal("config");
}

void installFiles(LuaArgs& lua, LoadedPlugin& plugin) {
    LoadedPlugin* target = &plugin;
    lua.pushTable();
    wire::lua::pushHostFunction(lua, "files.read", [target](LuaArgs& args) {
        const std::string path = args.checkString(1);
        std::optional<std::string> text = wire::io::fileIO::readText(target->folder + "/" + path);
        if (!text.has_value()) {
            text = target->pkg->readText(std::string{wire::plugins::packageLayout::kResources} + path);
        }
        if (text.has_value()) {
            args.pushString(*text);
        } else {
            args.pushNil();
        }
        return 1;
    });
    lua.setField("read");
    wire::lua::pushHostFunction(lua, "files.exists", [target](LuaArgs& args) {
        const std::string path = args.checkString(1);
        args.pushBoolean(wire::io::fileIO::exists(target->folder + "/" + path) ||
                         target->pkg->exists(std::string{wire::plugins::packageLayout::kResources} + path));
        return 1;
    });
    lua.setField("exists");
    wire::lua::pushHostFunction(lua, "files.list", [target](LuaArgs& args) {
        const std::string prefix = args.count() >= 1 ? args.checkString(1) : std::string{};
        args.pushTable();
        std::int64_t position = 1;
        for (const auto& [relative, path] : target->files) {
            if (relative.starts_with(prefix)) {
                args.pushString(relative);
                args.setIndex(position++);
            }
        }
        return 1;
    });
    lua.setField("list");
    wire::lua::pushHostFunction(lua, "files.write", [target](LuaArgs& args) {
        const std::string path = args.checkString(1);
        const std::string text = args.checkString(2);
        const std::string live = target->folder + "/" + path;
        const std::size_t slash = live.rfind('/');
        if (slash != std::string::npos) {
            wire::io::fileIO::createDirectories(live.substr(0, slash));
        }
        args.pushBoolean(wire::io::fileIO::writeText(live, text));
        return 1;
    });
    lua.setField("write");
    lua.setGlobal("files");
}

void installScheduling(LuaArgs& lua, PluginInstance& instance, LoadedPlugin& plugin, PluginHost& host) {
    LoadedPlugin* target = &plugin;
    PluginInstance* lane = &instance;
    PluginHost* server = &host;

    wire::lua::installWaitingFunction(
        *instance.sandbox, "wait", [server, target, lane](wire::lua::LuaCall::Resume resume) {
            if (lane->deciderDepth > 0) {
                return;
            }
            PluginHandle handle = server->services().tasks().after(1, [server, target, lane, resume] {
                lane->sandbox->refill();
                server->resuming(*target, resume);
            });
            static_cast<void>(
                target->registrations.add("wait", [moved = std::move(handle)]() mutable { moved.remove(); }));
        });

    wire::lua::pushHostFunction(lua, "later", [server, target, lane](LuaArgs& args) {
        const auto ticks = static_cast<std::int32_t>(args.checkInteger(1));
        if (!args.isFunction(2)) {
            args.raise("later(ticks, fn) needs a function");
        }
        const auto fn = std::make_shared<LuaRef>(LuaRef::of(args.state(), 2));
        PluginHandle handle = server->services().tasks().after(
            ticks, [server, target, lane, fn] { server->enter(*target, *lane, *fn, "later"); });
        pushHandleTable(args, target->registrations.add("later(" + std::to_string(ticks) + ")",
                                                        [moved = std::move(handle)]() mutable { moved.remove(); }));
        return 1;
    });
    lua.setGlobal("later");

    wire::lua::pushHostFunction(lua, "every", [server, target, lane](LuaArgs& args) {
        const bool three = args.count() >= 3;
        const auto delay = static_cast<std::int32_t>(args.checkInteger(1));
        const auto period = static_cast<std::int32_t>(three ? args.checkInteger(2) : args.checkInteger(1));
        const std::int32_t at = three ? 3 : 2;
        if (!args.isFunction(at)) {
            args.raise("every(period, fn) needs a function");
        }
        const auto fn = std::make_shared<LuaRef>(LuaRef::of(args.state(), at));
        PluginHandle handle = server->services().tasks().every(
            delay, period, [server, target, lane, fn] { server->enter(*target, *lane, *fn, "every"); });
        pushHandleTable(args, target->registrations.add("every(" + std::to_string(period) + ")",
                                                        [moved = std::move(handle)]() mutable { moved.remove(); }));
        return 1;
    });
    lua.setGlobal("every");
}

void installWorld(LuaArgs& lua, PluginInstance& instance, LoadedPlugin& plugin, PluginHost& host) {
    LoadedPlugin* target = &plugin;
    PluginInstance* lane = &instance;
    PluginHost* server = &host;

    const auto pushOne = [server, target](LuaArgs& args, const std::shared_ptr<PluginPlayerRef>& player) {
        if (player == nullptr) {
            args.pushNil();
        } else {
            pushPlayer(args, player, &server->services().storage(), target->manifest.id, &target->text);
        }
    };

    lua.pushTable();
    wire::lua::pushHostFunction(lua, "players.online", [server, pushOne](LuaArgs& args) {
        args.pushTable();
        std::int64_t position = 1;
        for (const std::shared_ptr<PluginPlayerRef>& player : server->services().players().online()) {
            pushOne(args, player);
            args.setIndex(position++);
        }
        return 1;
    });
    lua.setField("online");
    wire::lua::pushHostFunction(lua, "players.by_name", [server, pushOne](LuaArgs& args) {
        pushOne(args, server->services().players().byName(args.checkString(1)));
        return 1;
    });
    lua.setField("by_name");
    wire::lua::pushHostFunction(lua, "players.by_uuid", [server, pushOne](LuaArgs& args) {
        pushOne(args, server->services().players().byUuid(args.checkString(1)));
        return 1;
    });
    lua.setField("by_uuid");
    wire::lua::pushHostFunction(lua, "players.count", [server](LuaArgs& args) {
        args.pushInteger(static_cast<std::int64_t>(server->services().players().online().size()));
        return 1;
    });
    lua.setField("count");
    lua.setGlobal("players");

    const auto hooks = [server, target] {
        WorldHooks built;
        built.onNpcClick = [server, target](const LuaRef& handler, const std::string& uuid) {
            server->npcClicked(*target, handler, uuid);
        };
        built.resolveText = [target](const std::string& written) { return target->text.resolve(written, nullptr, 0); };
        return built;
    };

    lua.pushTable();
    wire::lua::pushHostFunction(lua, "worlds.create", [server, hooks](LuaArgs& args) {
        if (!args.isTable(1)) {
            args.raise("worlds.create{ name = ..., dimension = ... } takes a table");
        }
        const std::optional<std::string> name = stringField(args, 1, "name");
        if (!name.has_value()) {
            args.raise("a world needs a name");
        }
        args.pushField(1, "seed");
        const std::optional<std::int64_t> seed = args.isNumber(-1) ? std::optional{args.toInteger(-1)} : std::nullopt;
        args.pop(1);
        const std::shared_ptr<PluginWorldRef> made =
            server->services().worlds().create(*name, stringField(args, 1, "dimension").value_or("overworld"),
                                               stringField(args, 1, "type").value_or("normal"), seed);
        if (made == nullptr) {
            args.pushNil();
        } else {
            pushWorld(args, made, hooks());
        }
        return 1;
    });
    lua.setField("create");
    wire::lua::pushHostFunction(lua, "worlds.all", [server, hooks](LuaArgs& args) {
        args.pushTable();
        std::int64_t position = 1;
        for (const std::shared_ptr<PluginWorldRef>& world : server->services().worlds().all()) {
            pushWorld(args, world, hooks());
            args.setIndex(position++);
        }
        return 1;
    });
    lua.setField("all");
    wire::lua::pushHostFunction(lua, "worlds.by_name", [server, hooks](LuaArgs& args) {
        const std::shared_ptr<PluginWorldRef> world = server->services().worlds().byName(args.checkString(1));
        if (world == nullptr) {
            args.pushNil();
        } else {
            pushWorld(args, world, hooks());
        }
        return 1;
    });
    lua.setField("by_name");
    wire::lua::pushHostFunction(lua, "worlds.default", [server, hooks](LuaArgs& args) {
        const std::shared_ptr<PluginWorldRef> world = server->services().worlds().defaultWorld();
        if (world == nullptr) {
            args.pushNil();
        } else {
            pushWorld(args, world, hooks());
        }
        return 1;
    });
    lua.setField("default");
    lua.setGlobal("worlds");

    wire::lua::installWaitingFunction(
        *instance.sandbox, "at", [server, target, lane](wire::lua::LuaCall::Resume resume) {
            if (lane->deciderDepth > 0) {
                resume();
                return;
            }
            LuaArgs args{lane->sandbox->state()};
            const std::optional<PluginPoint> point = pointFrom(args, 1);
            const std::string world = point.has_value() && point->world.has_value() ? *point->world : args.toText(1);
            const double x = point.has_value() ? point->x : args.toNumber(2);
            const double z = point.has_value() ? point->z : args.toNumber(3);
            const bool arranged = server->services().at(world, x, z, [server, target, lane, resume] {
                lane->sandbox->refill();
                server->resuming(*target, resume);
            });
            if (!arranged) {
                resume();
            }
        });

    wire::lua::pushHostFunction(lua, "pos", [](LuaArgs& args) {
        pushPosition(args, args.checkString(1), args.checkNumber(2), args.checkNumber(3), args.checkNumber(4));
        return 1;
    });
    lua.setGlobal("pos");
}

void installMessaging(LuaArgs& lua, LoadedPlugin& plugin, PluginHost& host) {
    LoadedPlugin* target = &plugin;
    PluginHost* server = &host;

    wire::lua::pushHostFunction(lua, "placeholder", [server, target](LuaArgs& args) {
        const std::string name = args.checkString(1);
        if (!args.isFunction(2)) {
            args.raise("placeholder(name, fn) needs a function");
        }
        const auto fn = std::make_shared<LuaRef>(LuaRef::of(args.state(), 2));
        PluginHandle handle = server->services().placeholders().registerPlaceholder(
            target->manifest.id, name, [server, target, fn](PluginPlayerRef* player) {
                return server->answer(*target, *fn, player, "placeholder");
            });
        static_cast<void>(target->registrations.add("placeholder(%" + target->manifest.id + "_" + name + "%)",
                                                    [moved = std::move(handle)]() mutable { moved.remove(); }));
        return 0;
    });
    lua.setGlobal("placeholder");

    wire::lua::pushHostFunction(lua, "fill", [server](LuaArgs& args) {
        const std::string text = args.checkString(1);
        const std::shared_ptr<PluginPlayerRef> player = playerFrom(args, 2, server->services().players());
        args.pushString(server->services().placeholders().fill(text, player.get()));
        return 1;
    });
    lua.setGlobal("fill");

    wire::lua::pushHostFunction(lua, "broadcast", [server, target](LuaArgs& args) {
        const std::string message = target->text.resolve(args.checkString(1), &args, 2);
        for (const std::shared_ptr<PluginPlayerRef>& player : server->services().players().online()) {
            player->send(message);
        }
        return 0;
    });
    lua.setGlobal("broadcast");

    wire::lua::pushHostFunction(lua, "t", [target](LuaArgs& args) {
        args.pushString(target->text.resolve("@" + args.checkString(1), &args, 2));
        return 1;
    });
    lua.setGlobal("t");

    wire::lua::pushHostFunction(lua, "service", [server, target](LuaArgs& args) {
        const std::string name = args.checkString(1);
        if (args.isNil(2)) {
            server->serviceRegistry().consume(args, *server, name);
            return 1;
        }
        if (std::ranges::find(target->manifest.exports.services, name) == target->manifest.exports.services.end()) {
            args.raise(name + " is not in [exports] services of " + target->manifest.id + "; add services = [\"" +
                       name + "\"] to plugin.toml");
        }
        PluginHandle handle = server->serviceRegistry().provide(args, 2, target->manifest.id, name);
        static_cast<void>(target->registrations.add("service(" + name + ")",
                                                    [moved = std::move(handle)]() mutable { moved.remove(); }));
        return 0;
    });
    lua.setGlobal("service");

    pushStore(lua, host, plugin.manifest.id);
    lua.setGlobal("store");
    pushData(lua, plugin.folder);
    lua.setGlobal("data");

    wire::lua::pushHostFunction(lua, "emit", [server](LuaArgs& args) {
        const std::string name = args.checkString(1);
        if (name.find(':') == std::string::npos) {
            args.raise("'" + name +
                       "' has no colon in it, so it would collide with the server's own events; call it "
                       "something like 'yourplugin:" +
                       name + "'");
        }
        args.pushInteger(server->emit(name, LuaRef::of(args.state(), 2)));
        return 1;
    });
    lua.setGlobal("emit");

    wire::lua::pushHostFunction(lua, "menu", [server, target](LuaArgs& args) {
        if (!args.isTable(1)) {
            args.raise("menu{ title = ..., rows = 3 } takes a table");
        }
        args.pushField(1, "rows");
        const auto rows = static_cast<std::int32_t>(args.isNumber(-1) ? args.toInteger(-1) : 3);
        args.pop(1);
        if (rows < 1 || rows > PluginMenus::kMaxRows) {
            args.raise("a menu has between 1 and " + std::to_string(PluginMenus::kMaxRows) + " rows, not " +
                       std::to_string(rows));
        }
        const std::string title = stringField(args, 1, "title").value_or("");
        const std::shared_ptr<LuaMenu> built = LuaMenu::create(
            server->services().menus(), server->services().players(), target->text.resolve(title, nullptr, 0), rows,
            [server, target](const LuaRef& handler, PluginPlayerRef& player, std::int32_t slot) {
                server->clicked(*target, handler, player, slot);
            });
        static_cast<void>(target->registrations.add("menu(" + title + ")", [built] { built->closeAll(); }));
        built->push(args);
        return 1;
    });
    lua.setGlobal("menu");

    wire::lua::pushHostFunction(lua, "form", [server, target](LuaArgs& args) {
        const auto resolve = [target](const std::string& written) { return target->text.resolve(written, nullptr, 0); };
        PluginForm described = readForm(args, 1, resolve);
        const std::string title = described.title;
        const std::shared_ptr<LuaForm> built =
            LuaForm::create(server->services().menus(), server->services().players(), std::move(described),
                            [server, target](const LuaRef& handler, PluginPlayerRef& player, const PluginForm& form,
                                             std::size_t button, const std::map<std::string, std::string>& values) {
                                server->formAnswered(*target, handler, player, form, button, values);
                            });
        static_cast<void>(target->registrations.add("form(" + title + ")", [built] { built->closeAll(); }));
        built->push(args);
        return 1;
    });
    lua.setGlobal("form");
}

[[nodiscard]] PluginEventValue eventValueOf(LuaArgs& lua, std::int32_t index) {
    if (lua.isBoolean(index)) {
        return PluginEventValue{lua.toBoolean(index)};
    }
    if (lua.isNumber(index)) {
        return lua.isInteger(index) ? PluginEventValue{lua.toInteger(index)} : PluginEventValue{lua.toNumber(index)};
    }
    return PluginEventValue{lua.toText(index)};
}

void installListener(LuaArgs& lua, PluginInstance& instance, LoadedPlugin& plugin, PluginHost& host, bool observer) {
    LoadedPlugin* target = &plugin;
    PluginInstance* lane = &instance;
    PluginHost* server = &host;
    const std::string verb = observer ? "watch" : "on";
    wire::lua::pushHostFunction(lua, verb, [server, target, lane, observer, verb](LuaArgs& args) {
        const std::string name = args.checkString(1);
        const bool hasOptions = args.count() >= 3;
        const std::int32_t options = hasOptions ? 2 : 0;
        const std::int32_t at = hasOptions ? 3 : 2;
        if (!args.isFunction(at)) {
            args.raise(verb + "(event, fn) needs a function");
        }
        LuaRef handler = LuaRef::of(args.state(), at);

        if (name == pluginEventTypes::kPluginEnable || name == pluginEventTypes::kPluginDisable) {
            if (observer) {
                args.raise("'" + name + "' is about this plugin: listen to it with on(), not watch()");
            }
            if (name == pluginEventTypes::kPluginEnable) {
                lane->onEnable.push_back(std::move(handler));
            } else {
                lane->onDisable.push_back(std::move(handler));
            }
            return 0;
        }
        if (name.find(':') != std::string::npos) {
            if (observer) {
                args.raise("'" + name + "' belongs to a plugin: listen to it with on()");
            }
            server->announce(*target, name, std::move(handler));
            return 0;
        }

        const PluginEventType* type = server->events().typeOf(name);
        if (type == nullptr) {
            std::string known;
            for (const PluginEventType* declared : server->events().declared()) {
                known += known.empty() ? declared->name() : ", " + declared->name();
            }
            args.raise("there is no event called '" + name + "'; this server has " + known);
        }

        PluginEventPriority priority = PluginEventPriority::Normal;
        bool wantsCancelled = false;
        std::map<std::string, PluginEventValue, std::less<>> filters;
        if (options != 0 && args.isTable(options)) {
            args.pushField(options, "priority");
            if (args.isString(-1)) {
                const std::optional<PluginEventPriority> spelled = priorityOf(args.toText(-1));
                if (!spelled.has_value()) {
                    const std::string written = args.toText(-1);
                    args.pop(1);
                    args.raise("'" + written + "' is not a priority");
                }
                priority = *spelled;
            }
            args.pop(1);
            args.pushField(options, "cancelled");
            if (!args.isNil(-1)) {
                if (!observer && !type->cancellable()) {
                    args.pop(1);
                    args.raise("'" + name + "' cannot be cancelled, so 'cancelled' means nothing on it");
                }
                wantsCancelled = args.toBoolean(-1);
            }
            args.pop(1);
            for (const std::string& field : type->fields()) {
                args.pushField(options, field);
                if (!args.isNil(-1)) {
                    filters.emplace(field, eventValueOf(args, args.top()));
                }
                args.pop(1);
            }
        }
        server->registerEventHandler(*target, *lane, *type, priority, observer, wantsCancelled, std::move(filters),
                                     std::move(handler));
        return 0;
    });
    lua.setGlobal(verb);
}

void installListeners(LuaArgs& lua, PluginInstance& instance, LoadedPlugin& plugin, PluginHost& host) {
    installListener(lua, instance, plugin, host, false);
    installListener(lua, instance, plugin, host, true);

    LoadedPlugin* target = &plugin;
    PluginInstance* lane = &instance;
    PluginHost* server = &host;
    wire::lua::pushHostFunction(lua, "cmd", [server, target, lane](LuaArgs& args) {
        const std::string spec = args.checkString(1);
        if (!args.isFunction(2)) {
            args.raise("cmd(spec, fn) needs a function");
        }
        std::optional<std::string> permission;
        if (args.isTable(3)) {
            permission = stringField(args, 3, "permission");
        }
        const std::expected<PluginCommandSpec, wire::plugins::ManifestError> parsed =
            PluginCommandSpec::parse(spec, std::move(permission));
        if (!parsed.has_value()) {
            args.raise(parsed.error().message);
        }
        server->registerCommand(*target, *lane, *parsed, LuaRef::of(args.state(), 2));
        return 0;
    });
    lua.setGlobal("cmd");

    for (const PluginSurface surface : allSurfaces()) {
        const std::string name{surfaceName(surface)};
        wire::lua::pushHostFunction(lua, name, [server, target, surface, name](LuaArgs& args) {
            if (!args.isTable(1)) {
                args.raise(name + "{ priority = ..., lines = ... } takes a table");
            }
            args.pushField(1, "priority");
            if (!args.isNumber(-1)) {
                args.pop(1);
                args.raise(name + "{} needs a priority: it is what decides who draws it when two plugins want to, "
                                  "and there is no default so a tie cannot happen by accident");
            }
            const auto priority = static_cast<std::int32_t>(args.toInteger(-1));
            args.pop(1);
            args.pushField(1, "when");
            std::shared_ptr<LuaRef> applies;
            if (args.isFunction(-1)) {
                applies = std::make_shared<LuaRef>(LuaRef::of(args.state(), -1));
            }
            args.pop(1);
            args.pushField(1, "title");
            const bool titleIsFunction = args.isFunction(-1);
            const std::shared_ptr<LuaRef> title =
                titleIsFunction ? std::make_shared<LuaRef>(LuaRef::of(args.state(), -1)) : nullptr;
            const std::string writtenTitle = titleIsFunction ? std::string{} : args.toText(-1);
            args.pop(1);
            args.pushField(1, "lines");
            const bool linesAreFunction = args.isFunction(-1);
            const std::shared_ptr<LuaRef> lines =
                linesAreFunction ? std::make_shared<LuaRef>(LuaRef::of(args.state(), -1)) : nullptr;
            std::vector<std::string> writtenLines;
            if (!linesAreFunction && args.isTable(-1)) {
                const std::int64_t length = args.length(-1);
                for (std::int64_t position = 1; position <= length; position++) {
                    args.pushIndex(-1, position);
                    writtenLines.push_back(args.toText(-1));
                    args.pop(1);
                }
            }
            args.pop(1);

            PluginSurfaceClaim claim{.pluginId = target->manifest.id, .surface = surface, .priority = priority};
            if (applies != nullptr) {
                claim.appliesTo = [server, target, applies, name](PluginPlayerRef& player) {
                    return server->asks(*target, *applies, &player, name + " when");
                };
            }
            claim.render = [server, target, title, writtenTitle, lines, writtenLines,
                            name](PluginPlayerRef& player) -> std::optional<PluginSurfaceContent> {
                PluginSurfaceContent drawn;
                drawn.title = title != nullptr ? server->answer(*target, *title, &player, name + " title")
                                               : target->text.resolve(writtenTitle, nullptr, 0);
                if (lines != nullptr) {
                    const std::string said = server->answer(*target, *lines, &player, name + " lines");
                    if (!said.empty()) {
                        drawn.lines.push_back(said);
                    }
                } else {
                    for (const std::string& written : writtenLines) {
                        drawn.lines.push_back(target->text.resolve(written, nullptr, 0));
                    }
                }
                if (drawn.title.empty() && drawn.lines.empty()) {
                    return std::nullopt;
                }
                return drawn;
            };
            PluginHandle handle = server->surfaces().claim(std::move(claim));
            static_cast<void>(target->registrations.add(name + "(priority " + std::to_string(priority) + ")",
                                                        [moved = std::move(handle)]() mutable { moved.remove(); }));
            return 0;
        });
        lua.setGlobal(name);
    }
}

void installGranted(LuaArgs& lua, LoadedPlugin& plugin, PluginHost& host) {
    LoadedPlugin* target = &plugin;
    PluginHost* server = &host;
    const std::set<wire::plugins::PluginPermission> granted = host.allowed(plugin.manifest.id);

    if (plugin.manifest.capabilities.packets && granted.contains(wire::plugins::PluginPermission::Packets)) {
        lua.pushTable();
        for (const bool observer : {false, true}) {
            wire::lua::pushHostFunction(
                lua, observer ? "packet.watch" : "packet.on", [server, target, observer](LuaArgs& args) {
                    const std::string name = args.checkString(1);
                    const bool hasOptions = args.count() >= 3;
                    const std::int32_t at = hasOptions ? 3 : 2;
                    if (!args.isFunction(at)) {
                        args.raise("packet.on(name, fn) needs a function");
                    }
                    if (!server->services().packets().isKnown(name)) {
                        args.raise("this server has no packet called '" + name +
                                   "'. Names look like clientbound.set_title_text; "
                                   "/plugins packets lists them");
                    }
                    PluginEventPriority priority = PluginEventPriority::Normal;
                    if (hasOptions && args.isTable(2)) {
                        const std::optional<std::string> spelled = stringField(args, 2, "priority");
                        if (spelled.has_value()) {
                            const std::optional<PluginEventPriority> read = priorityOf(*spelled);
                            if (!read.has_value()) {
                                args.raise("'" + *spelled + "' is not a priority");
                            }
                            priority = *read;
                        }
                    }
                    PluginHandle handle =
                        server->packets().add(*target, name, LuaRef::of(args.state(), at), priority, observer);
                    static_cast<void>(
                        target->registrations.add(std::string{"packet."} + (observer ? "watch(" : "on(") + name + ")",
                                                  [moved = std::move(handle)]() mutable { moved.remove(); }));
                    return 0;
                });
            lua.setField(observer ? "watch" : "on");
        }
        wire::lua::pushHostFunction(lua, "packet.send", [server](LuaArgs& args) {
            const std::shared_ptr<PluginPlayerRef> player = playerFrom(args, 1, server->services().players());
            if (player == nullptr) {
                args.raise("packet.send(player, name, bytes) needs a player");
            }
            const std::string name = args.checkString(2);
            if (!args.isTable(3)) {
                args.raise("packet.send needs the body as a list of bytes");
            }
            std::vector<std::uint8_t> bytes;
            const std::int64_t length = args.length(3);
            bytes.reserve(static_cast<std::size_t>(length));
            for (std::int64_t position = 1; position <= length; position++) {
                args.pushIndex(3, position);
                bytes.push_back(static_cast<std::uint8_t>(args.toInteger(-1) & 0xFF));
                args.pop(1);
            }
            args.pushBoolean(server->services().packets().send(*player, name, bytes));
            return 1;
        });
        lua.setField("send");
        lua.setGlobal("packet");
    }

    if (!plugin.manifest.capabilities.network.empty() && granted.contains(wire::plugins::PluginPermission::Network)) {
        const std::set<std::string, std::less<>> allowed{plugin.manifest.capabilities.network.begin(),
                                                         plugin.manifest.capabilities.network.end()};
        const auto check = [target, allowed](LuaArgs& args, const std::string& name) {
            if (allowed.contains(name)) {
                return;
            }
            if (allowed.empty()) {
                args.raise(target->manifest.id + " declared no channels; add network = [\"" + name +
                           "\"] to [capabilities] in plugin.toml");
            }
            std::string listed;
            for (const std::string& each : allowed) {
                listed += listed.empty() ? each : ", " + each;
            }
            args.raise("'" + name + "' is not one of " + target->manifest.id + "'s channels, which are " + listed);
        };
        lua.pushTable();
        wire::lua::pushHostFunction(lua, "channel.on", [server, target, check](LuaArgs& args) {
            const std::string name = args.checkString(1);
            check(args, name);
            if (!args.isFunction(2)) {
                args.raise("channel.on(name, fn) needs a function");
            }
            server->listenOn(*target, name, LuaRef::of(args.state(), 2));
            return 0;
        });
        lua.setField("on");
        wire::lua::pushHostFunction(lua, "channel.send", [server, check](LuaArgs& args) {
            const std::shared_ptr<PluginPlayerRef> player = playerFrom(args, 1, server->services().players());
            if (player == nullptr) {
                args.raise("channel.send(player, name, bytes) needs a player");
            }
            const std::string name = args.checkString(2);
            check(args, name);
            if (!args.isTable(3)) {
                args.raise("channel.send needs the body as a list of bytes");
            }
            std::vector<std::uint8_t> bytes;
            const std::int64_t length = args.length(3);
            bytes.reserve(static_cast<std::size_t>(length));
            for (std::int64_t position = 1; position <= length; position++) {
                args.pushIndex(3, position);
                bytes.push_back(static_cast<std::uint8_t>(args.toInteger(-1) & 0xFF));
                args.pop(1);
            }
            args.pushBoolean(server->services().channels().send(*player, name, bytes));
            return 1;
        });
        lua.setField("send");
        lua.setGlobal("channel");
    }

    if (plugin.manifest.capabilities.commands && granted.contains(wire::plugins::PluginPermission::Commands)) {
        wire::lua::pushHostFunction(lua, "run", [server, target](LuaArgs& args) {
            const std::string command = args.checkString(1);
            target->log.info([&command] { return "ran /" + command; });
            args.pushBoolean(server->services().commands().runAsConsole(command));
            return 1;
        });
        lua.setGlobal("run");
    }
}
} // namespace

void install(PluginInstance& instance, LoadedPlugin& plugin, PluginHost& host, std::int32_t lanes) {
    LuaArgs lua{instance.sandbox->state()};
    installIdentity(lua, instance, plugin, lanes);
    installFiles(lua, plugin);
    installScheduling(lua, instance, plugin, host);
    installWorld(lua, instance, plugin, host);
    installMessaging(lua, plugin, host);
    installListeners(lua, instance, plugin, host);
    installGranted(lua, plugin, host);
}
} // namespace basalt::server::plugins::pluginApi
