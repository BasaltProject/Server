#include <basalt/server/lua/LuaCall.hpp>
#include <basalt/server/plugins/PluginHost.hpp>
#include <basalt/server/plugins/PluginObjects.hpp>
#include <basalt/server/plugins/PluginPacketView.hpp>

#include <algorithm>
#include <cstdint>
#include <mutex>
#include <utility>

namespace basalt::server::plugins {
std::int64_t PluginWatchdog::nowMillis() const {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started_).count();
}

void PluginWatchdog::mark(std::string_view pluginId) {
    const std::lock_guard<std::mutex> guard{mutex_};
    running_ = pluginId;
    since_ = nowMillis();
}

void PluginWatchdog::clear() {
    const std::lock_guard<std::mutex> guard{mutex_};
    running_.clear();
}

std::optional<std::string> PluginWatchdog::interruptIfStuck(std::int64_t limitMillis,
                                                            const std::vector<std::unique_ptr<LoadedPlugin>>& plugins) {
    std::string who;
    {
        const std::lock_guard<std::mutex> guard{mutex_};
        if (running_.empty() || nowMillis() - since_ < limitMillis) {
            return std::nullopt;
        }
        who = std::exchange(running_, std::string{});
    }
    for (const std::unique_ptr<LoadedPlugin>& plugin : plugins) {
        if (plugin->manifest.id == who) {
            plugin->log.error([limitMillis] {
                return "was still inside the server after " + std::to_string(limitMillis) + "ms and was cut off";
            });
        }
    }
    interruptAll(plugins);
    return who;
}

void PluginWatchdog::interruptAll(const std::vector<std::unique_ptr<LoadedPlugin>>& plugins) {
    for (const std::unique_ptr<LoadedPlugin>& plugin : plugins) {
        for (const std::unique_ptr<PluginInstance>& instance : plugin->instances) {
            if (instance->sandbox != nullptr) {
                instance->sandbox->interrupt();
            }
        }
    }
}

void PluginAnnouncements::announce(PluginHost& host, LoadedPlugin& plugin, const std::string& name,
                                   lua::LuaRef handler) {
    static_cast<void>(host);
    const std::int64_t id = nextId_++;
    listeners_[name].push_back(Listener{.plugin = &plugin, .handler = std::move(handler), .id = id});
    static_cast<void>(plugin.registrations.add("on(" + name + ")", [this, name, id] {
        const auto found = listeners_.find(name);
        if (found == listeners_.end()) {
            return;
        }
        std::erase_if(found->second, [id](const Listener& listener) { return listener.id == id; });
        if (found->second.empty()) {
            listeners_.erase(found);
        }
    }));
}

std::int32_t PluginAnnouncements::emit(PluginHost& host, const std::string& name, const lua::LuaRef& payload) {
    const auto found = listeners_.find(name);
    if (found == listeners_.end()) {
        return 0;
    }
    std::int32_t heard = 0;
    // A copy, because a listener may register or drop one while this runs.
    std::vector<std::pair<LoadedPlugin*, const lua::LuaRef*>> targets;
    targets.reserve(found->second.size());
    for (const Listener& listener : found->second) {
        targets.emplace_back(listener.plugin, &listener.handler);
    }
    for (const auto& [plugin, handler] : targets) {
        if (plugin == nullptr || plugin->disabled || plugin->instances.empty()) {
            continue;
        }
        const bool ran = host.ask(*plugin, *handler, "emit " + name,
                                  [&payload](lua::LuaArgs& lua) {
                                      payload.push(lua);
                                      return 1;
                                  },
                                  {});
        if (ran) {
            heard++;
        }
    }
    return heard;
}

bool PluginHost::inside(LoadedPlugin& plugin, std::string_view why,
                        const std::function<std::optional<lua::LuaFailure>(PluginInstance&)>& body) {
    if (plugin.disabled || plugin.instances.empty()) {
        return false;
    }
    PluginInstance& instance = *plugin.instances.front();
    instance.sandbox->refill();
    instance.deciderDepth++;
    watchdog_.mark(plugin.manifest.id);
    const std::chrono::steady_clock::time_point startedAt = std::chrono::steady_clock::now();
    const std::optional<lua::LuaFailure> failure = body(instance);
    instance.deciderDepth--;
    watchdog_.clear();
    if (recordTime_) {
        recordTime_(
            plugin.manifest.id,
            std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - startedAt).count());
    }
    if (failure.has_value()) {
        report(plugin, *failure, why);
        return false;
    }
    plugin.strikes = 0;
    return true;
}

bool PluginHost::ask(LoadedPlugin& plugin, const lua::LuaRef& fn, std::string_view why,
                     const std::function<std::int32_t(lua::LuaArgs&)>& pushArguments,
                     const std::function<void(lua::LuaArgs&)>& read) {
    return inside(plugin, why, [&fn, &pushArguments, &read](PluginInstance& instance) {
        const std::expected<std::int32_t, lua::LuaFailure> answered =
            lua::callSync(*instance.sandbox, fn, pushArguments, read ? 1 : 0);
        if (!answered.has_value()) {
            return std::optional{answered.error()};
        }
        lua::LuaArgs results{instance.sandbox->state()};
        if (read) {
            read(results);
        }
        results.pop(*answered);
        return std::optional<lua::LuaFailure>{};
    });
}

void PluginHost::enter(LoadedPlugin& plugin, PluginInstance& instance, const lua::LuaRef& fn, std::string_view why,
                       const std::function<std::int32_t(lua::LuaArgs&)>& pushArguments) {
    if (plugin.disabled || !fn.valid()) {
        return;
    }
    instance.sandbox->refill();
    lua::LuaArgs lua{instance.sandbox->state()};
    fn.push(lua);
    const std::int32_t argumentCount = pushArguments ? pushArguments(lua) : 0;
    const std::string reason{why};
    LoadedPlugin* target = &plugin;
    static_cast<void>(lua::LuaCall::start(*instance.sandbox, argumentCount,
                                          [this, target, reason](std::optional<lua::LuaFailure> failure) {
                                              if (!failure.has_value()) {
                                                  target->strikes = 0;
                                                  return;
                                              }
                                              report(*target, *failure, reason);
                                          }));
}

void PluginHost::report(LoadedPlugin& plugin, const lua::LuaFailure& failure, std::string_view why) {
    plugin.strikes++;
    plugin.failures++;
    const std::string reason = failure.outOfBudget()   ? "ran out of its turn"
                               : failure.outOfMemory() ? "asked for more memory than it may have"
                                                       : "failed";
    plugin.log.error([&why, &reason, &failure] { return std::string{why} + " " + reason + ": " + failure.message; });
    if (plugin.strikes < strikeLimit_) {
        return;
    }
    plugin.disabled = true;
    // A plugin that is off must not leave a frozen sidebar on everyone's screen.
    surfaces_.yieldOf(plugin.manifest.id, "it is off after " + std::to_string(strikeLimit_) + " failures in a row");
    plugin.log.error([this, &plugin] {
        return plugin.manifest.id + " is off after " + std::to_string(strikeLimit_) +
               " failures in a row; fix it and run /plugins reload " + plugin.manifest.id;
    });
}

void PluginHost::resuming(LoadedPlugin& plugin, const std::function<void()>& resume) {
    if (plugin.disabled || !resume) {
        return;
    }
    resume();
    plugin.strikes = 0;
}

std::string PluginHost::answer(LoadedPlugin& plugin, const lua::LuaRef& fn, PluginPlayerRef* player,
                               std::string_view why) {
    std::string said;
    static_cast<void>(ask(
        plugin, fn, why,
        [this, &plugin, player](lua::LuaArgs& args) {
            if (player == nullptr) {
                args.pushNil();
            } else {
                pushPlayer(args, player->shared_from_this(), &services_->storage(), plugin.manifest.id, &plugin.text);
            }
            return 1;
        },
        [&said](lua::LuaArgs& args) { said = args.toText(-1); }));
    return said;
}

bool PluginHost::asks(LoadedPlugin& plugin, const lua::LuaRef& fn, PluginPlayerRef* player, std::string_view why) {
    bool answered = false;
    static_cast<void>(ask(
        plugin, fn, why,
        [this, &plugin, player](lua::LuaArgs& args) {
            if (player == nullptr) {
                args.pushNil();
            } else {
                pushPlayer(args, player->shared_from_this(), &services_->storage(), plugin.manifest.id, &plugin.text);
            }
            return 1;
        },
        [&answered](lua::LuaArgs& args) { answered = args.booleanOr(-1, false); }));
    return answered;
}

void PluginHost::clicked(LoadedPlugin& plugin, const lua::LuaRef& handler, PluginPlayerRef& player, std::int32_t slot) {
    static_cast<void>(ask(plugin, handler, "menu click",
                          [this, &plugin, &player, slot](lua::LuaArgs& args) {
                              args.pushTable();
                              pushPlayer(args, player.shared_from_this(), &services_->storage(), plugin.manifest.id,
                                         &plugin.text);
                              args.setField("player");
                              args.pushInteger(slot);
                              args.setField("slot");
                              return 1;
                          },
                          {}));
}

void PluginHost::npcClicked(LoadedPlugin& plugin, const lua::LuaRef& handler, const std::string& playerUuid) {
    const std::shared_ptr<PluginPlayerRef> player = services_->players().byUuid(playerUuid);
    if (player == nullptr) {
        return;
    }
    static_cast<void>(answer(plugin, handler, player.get(), "npc click"));
}

void PluginHost::deliverPacket(LoadedPlugin& plugin, const lua::LuaRef& fn, PluginPacket& packet) {
    static_cast<void>(ask(plugin, fn, "packet " + packet.name,
                          [this, &plugin, &packet](lua::LuaArgs& args) {
                              pushPacket(args, packet, *this, plugin);
                              return 1;
                          },
                          {}));
}
} // namespace basalt::server::plugins
