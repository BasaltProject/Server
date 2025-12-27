#include <basalt/server/lua/LuaCall.hpp>
#include <basalt/server/plugins/PluginDispatch.hpp>
#include <basalt/server/plugins/PluginEventView.hpp>
#include <basalt/server/plugins/PluginHost.hpp>
#include <basalt/server/plugins/PluginObjects.hpp>

#include <algorithm>
#include <utility>

namespace basalt::server::plugins {
namespace {

using lua::LuaArgs;

/// One field of an event, when it is there and it is of the type asked for.
template <typename T>
[[nodiscard]] const T* fieldOf(const PluginEvent& event, std::string_view name) {
    return std::get_if<T>(&event.get(name));
}

/// `e.player`, `e.entity` and `e.pos`: the objects behind the plain fields an event carries, looked
/// up when asked for and never before. That is why an event carries a uuid and three numbers rather
/// than objects most handlers never read; the event object is reused between fires, so this reads
/// the one the view was built around.
///
/// A lookup that finds nobody answers nil: a player who logged out is a question with an answer.
[[nodiscard]] PluginEventView::Objects objectsOf(PluginHost& host, LoadedPlugin& plugin, PluginEvent& event) {
    PluginHost* server = &host;
    LoadedPlugin* target = &plugin;
    const PluginEvent* fired = &event;
    return [server, target, fired](LuaArgs& lua, std::string_view name) {
        const auto carries = [fired](std::string_view field) { return fired->type().indexOf(field) >= 0; };
        if (name == "player" && carries("player_uuid")) {
            const std::string* uuid = fieldOf<std::string>(*fired, "player_uuid");
            std::shared_ptr<PluginPlayerRef> player =
                uuid == nullptr ? nullptr : server->services().players().byUuid(*uuid);
            // By name second: an event may name somebody it has no uuid for.
            if (player == nullptr) {
                const std::string* named = fieldOf<std::string>(*fired, "player_name");
                player = named == nullptr ? nullptr : server->services().players().byName(*named);
            }
            if (player == nullptr) {
                lua.pushNil();
                return true;
            }
            pushPlayer(lua, std::move(player), &server->services().storage(), target->manifest.id, &target->text);
            return true;
        }
        if (name == "entity" && carries("entity_id")) {
            const std::int64_t* id = fieldOf<std::int64_t>(*fired, "entity_id");
            const std::string* world = fieldOf<std::string>(*fired, "world");
            const std::shared_ptr<PluginWorldRef> where =
                world == nullptr ? nullptr : server->services().worlds().byName(*world);
            if (id != nullptr && where != nullptr) {
                for (const std::shared_ptr<PluginEntityRef>& entity : where->entities()) {
                    if (entity->id() == static_cast<std::int32_t>(*id)) {
                        pushEntity(lua, entity);
                        return true;
                    }
                }
            }
            lua.pushNil();
            return true;
        }
        if (name == "pos" && carries("x")) {
            const double* x = fieldOf<double>(*fired, "x");
            const double* y = fieldOf<double>(*fired, "y");
            const double* z = fieldOf<double>(*fired, "z");
            if (x == nullptr || y == nullptr || z == nullptr) {
                lua.pushNil();
                return true;
            }
            const std::string* world = fieldOf<std::string>(*fired, "world");
            pushPosition(lua, world == nullptr ? std::string{} : *world, *x, *y, *z);
            return true;
        }
        return false;
    };
}

/// Pushes what a command handed over as an argument, in the shapes a spec can produce.
void pushArgument(LuaArgs& lua, const PluginCommandCall::Argument& argument) {
    if (std::holds_alternative<std::monostate>(argument)) {
        lua.pushNil();
    } else if (const auto* flag = std::get_if<bool>(&argument); flag != nullptr) {
        lua.pushBoolean(*flag);
    } else if (const auto* whole = std::get_if<std::int64_t>(&argument); whole != nullptr) {
        lua.pushInteger(*whole);
    } else if (const auto* real = std::get_if<double>(&argument); real != nullptr) {
        lua.pushNumber(*real);
    } else if (const auto* text = std::get_if<std::string>(&argument); text != nullptr) {
        lua.pushString(*text);
    }
}

} // namespace

void PluginCommandDispatch::registerCommand(LoadedPlugin& plugin, PluginInstance& instance,
                                            const PluginCommandSpec& spec, lua::LuaRef handler) {
    std::string key;
    for (const std::string& literal : spec.literals) {
        key += key.empty() ? literal : " " + literal;
    }
    if (CommandEntry* existing = plugin.commandNamed(key); existing != nullptr) {
        existing->handlers.insert_or_assign(instance.lane, std::move(handler));
        return;
    }
    CommandEntry entry{.spec = spec};
    entry.handlers.emplace(instance.lane, std::move(handler));
    plugin.commands.emplace_back(key, std::move(entry));
    LoadedPlugin* target = &plugin;
    PluginHost* host = host_;
    const std::string commandKey = key;
    PluginHandle handle = host_->services().commands().registerCommand(
        plugin.manifest.id, spec, [host, target, commandKey](PluginCommandCall& call) {
            CommandEntry* entry = target->commandNamed(commandKey);
            if (entry != nullptr) {
                run(*host, *target, *entry, call);
            }
        });
    static_cast<void>(
        plugin.registrations.add("command /" + spec.usage(), [target, commandKey, moved = std::move(handle)]() mutable {
            moved.remove();
            std::erase_if(target->commands, [&commandKey](const auto& held) { return held.first == commandKey; });
        }));
}

PluginInstance* PluginCommandDispatch::laneFor(LoadedPlugin& plugin, CommandEntry& entry) {
    // With one lane there is nothing to decide. With more, they take turns, which is on purpose: a
    // plugin that assumed one state sees it here, in a test, rather than in production the first
    // time an operator turns regions on.
    if (plugin.instances.empty()) {
        return nullptr;
    }
    const std::int32_t lane = entry.nextLane % static_cast<std::int32_t>(plugin.instances.size());
    entry.nextLane = lane + 1;
    for (const std::unique_ptr<PluginInstance>& instance : plugin.instances) {
        if (instance->lane == lane) {
            return instance.get();
        }
    }
    return plugin.instances.front().get();
}

void PluginCommandDispatch::run(PluginHost& host, LoadedPlugin& plugin, CommandEntry& entry, PluginCommandCall& call) {
    PluginInstance* instance = laneFor(plugin, entry);
    if (instance == nullptr) {
        return;
    }
    const auto handler = entry.handlers.find(instance->lane);
    if (handler == entry.handlers.end()) {
        return;
    }
    const PluginCommandSpec spec = entry.spec;
    PluginCommandCall* target = &call;
    const PluginText* text = &plugin.text;
    host.enter(plugin, *instance, handler->second, "/" + spec.name(), [target, text, &spec](LuaArgs& lua) {
        // The source first, then the arguments the spec declared, in the order it declared them.
        PluginObject source{"source"};
        source.reads("name", [target] { return PluginValue{target->source()}; })
            .reads("is_console", [target] { return PluginValue{target->isConsole()}; })
            .method("send", [target, text](LuaArgs& args) {
                target->reply(resolved(text, args));
                return 0;
            });
        std::move(source).push(lua);
        for (const PluginCommandArgument& argument : spec.arguments) {
            pushArgument(lua, target->argument(argument.name));
        }
        return 1 + static_cast<std::int32_t>(spec.arguments.size());
    });
}

void PluginEventDispatch::registerEventHandler(LoadedPlugin& plugin, PluginInstance& instance,
                                               const PluginEventType& type, PluginEventPriority priority, bool observer,
                                               bool wantsCancelled,
                                               std::map<std::string, PluginEventValue, std::less<>> filters,
                                               lua::LuaRef handler) {
    // One view per handler, made on the first fire and reused: dispatching an event should not build
    // a table.
    const auto view = std::make_shared<std::unique_ptr<PluginEventView>>();
    const auto held = std::make_shared<lua::LuaRef>(std::move(handler));
    PluginHost* host = host_;
    LoadedPlugin* target = &plugin;
    PluginInstance* lane = &instance;
    PluginEventDispatch* self = this;
    const std::string typeName = type.name();
    PluginHandle handle = host_->events().add(
        type, plugin.manifest.id, instance.lane, priority, observer, wantsCancelled, std::move(filters),
        [self, host, target, lane, held, view, observer, typeName](PluginEvent& event) {
            if (target->disabled) {
                return;
            }
            if (*view == nullptr) {
                *view = PluginEventView::create(
                    *lane->sandbox, event, [&event](std::optional<std::string> message) { event.cancel(message); },
                    objectsOf(*host, *target, event),
                    [target](std::string_view message) { return target->text.resolve(message, nullptr, 0); });
            }
            if (*view == nullptr) {
                return;
            }
            self->dispatch(*host, *target, *lane, *held, **view, observer, typeName);
        });
    static_cast<void>(plugin.registrations.add((observer ? "watch(" : "on(") + typeName + ")",
                                               [moved = std::move(handle)]() mutable { moved.remove(); }));
}

void PluginEventDispatch::dispatch(PluginHost& host, LoadedPlugin& plugin, PluginInstance& instance,
                                   const lua::LuaRef& handler, PluginEventView& view, bool observer,
                                   const std::string& typeName) {
    view.bind();
    if (observer) {
        // An observer runs after the fact, so it may take as long as it likes: it is handed to the
        // owner rather than run here, which is the whole point of it being one.
        PluginHost* target = &host;
        LoadedPlugin* owner = &plugin;
        PluginInstance* lane = &instance;
        const lua::LuaRef* fn = &handler;
        PluginEventView* bound = &view;
        static_cast<void>(host.services().tasks().after(0, [target, owner, lane, fn, bound, typeName] {
            target->enter(*owner, *lane, *fn, "watch " + typeName, [bound](lua::LuaArgs& lua) {
                bound->push(lua);
                return 1;
            });
        }));
        return;
    }
    // A decider runs here and now, because the server is waiting for the answer: if this is not the
    // thread plugins run on, it does not run at all, and the reason is said once rather than every
    // tick.
    if (!host.onPluginThread()) {
        if (!warnedAboutThread_) {
            warnedAboutThread_ = true;
            host.log().warn([&typeName] {
                return "a " + typeName +
                       " was fired from a thread plugins do not run on, so no decider saw it. Either the "
                       "server is running regions on more than one thread (the known limit until lanes "
                       "follow them), or that event is being fired from the wrong place and it is a bug here";
            });
        }
        return;
    }
    instance.deciderDepth++;
    host.enter(plugin, instance, handler, "on " + typeName, [&view](lua::LuaArgs& lua) {
        view.push(lua);
        return 1;
    });
    instance.deciderDepth--;
}

} // namespace basalt::server::plugins
