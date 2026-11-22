#include <basalt/server/plugins/PluginHost.hpp>
#include <basalt/server/plugins/WasmPlugin.hpp>

#include <algorithm>
#include <chrono>
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
        if (plugin->wasm != nullptr) {
            plugin->wasm->interrupt();
        }
    }
}

void PluginHost::report(LoadedPlugin& plugin, const wire::plugins::PluginFailure& failure, std::string_view why) {
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
} // namespace basalt::server::plugins
