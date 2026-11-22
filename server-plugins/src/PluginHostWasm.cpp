#include <basalt/server/plugins/PluginHost.hpp>
#include <basalt/server/plugins/WasmPlugin.hpp>
#include <basalt/wire/plugins/PluginFailure.hpp>

#include <algorithm>
#include <utility>

namespace basalt::server::plugins {
bool PluginHost::startWasm(std::unique_ptr<LoadedPlugin> plugin, bool verified) {
    LoadedPlugin& loaded = *plugin;
    const wire::plugins::PluginManifest& manifest = loaded.manifest;
    loaded.bytecodeVerified = verified;
    const std::optional<std::vector<std::uint8_t>> module = loaded.pkg->read(manifest.main);
    if (!module.has_value()) {
        loaded.log.error([&manifest] {
            return manifest.id + " says its code is at " + manifest.main + " and there is nothing there";
        });
        return false;
    }
    if (!wasmEngine_.has_value()) {
        const wire::wasm::EngineLimits limits{.maxMemoryBytes = static_cast<std::uint64_t>(memoryBytes_)};
        wire::wasm::Result<wire::wasm::Engine> engine = wire::wasm::Engine::create(limits);
        if (!engine) {
            loaded.log.error([&engine] { return "the WebAssembly engine did not start: " + engine.error().message; });
            return false;
        }
        wasmEngine_ = *engine;
    }
    std::expected<std::unique_ptr<WasmPlugin>, std::string> created =
        WasmPlugin::create(*this, loaded, *wasmEngine_, *module, wasmCache_);
    if (!created.has_value()) {
        loaded.log.error([&manifest, &created] { return manifest.id + " did not load: " + created.error(); });
        return false;
    }
    loaded.wasm = std::move(*created);
    plugins_.push_back(std::move(plugin));
    bool started = false;
    {
        const std::lock_guard<std::recursive_mutex> turn{turnMutex_};
        started = loaded.wasm->start();
    }
    if (!started || loaded.failures > 0 || loaded.disabled) {
        loaded.log.error([&manifest] { return manifest.id + " failed while loading and is off"; });
        loaded.disabled = true;
        LoadedPlugin* target = &loaded;
        stop(loaded);
        std::erase_if(plugins_, [target](const std::unique_ptr<LoadedPlugin>& held) { return held.get() == target; });
        return false;
    }
    loaded.log.info([&manifest, &loaded] {
        return manifest.name + " " + manifest.version + " loaded from " + loaded.pkg->origin() + " (WebAssembly)";
    });
    return true;
}

void PluginHost::beginWasmTurn(LoadedPlugin& plugin) { watchdog_.mark(plugin.manifest.id); }

void PluginHost::endWasmTurn(LoadedPlugin& plugin, std::int64_t nanos) {
    watchdog_.clear();
    recordTime(plugin.manifest.id, nanos);
}

void PluginHost::reportWasm(LoadedPlugin& plugin, const wire::wasm::Failure& failure, std::string_view why) {
    wire::plugins::PluginFailure converted{.plugin = plugin.manifest.id, .message = failure.message};
    if (failure.code == wire::wasm::ErrorCode::Interrupted) {
        converted.message = std::string{wire::plugins::failureMessages::kInterrupted} + " by the watchdog";
    } else if (failure.message.find("memory") != std::string::npos) {
        converted.message = std::string{wire::plugins::failureMessages::kMemory} + ": " + failure.message;
    }
    report(plugin, converted, why);
}
} // namespace basalt::server::plugins
