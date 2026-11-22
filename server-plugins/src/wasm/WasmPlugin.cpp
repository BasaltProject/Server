#include "WasmMenus.hpp"
#include "WasmState.hpp"

#include <basalt/wire/wasm/Module.hpp>

#include <algorithm>
#include <filesystem>

namespace basalt::server::plugins {
namespace {
void wasiLog(void* user, std::int32_t fd, std::string_view text) {
    auto& state = *static_cast<wasm::State*>(user);
    while (!text.empty() && (text.back() == '\n' || text.back() == '\r')) {
        text.remove_suffix(1);
    }
    if (text.empty()) {
        return;
    }
    if (fd == 2) {
        state.plugin->log.warn([&text] { return std::string{text}; });
    } else {
        state.plugin->log.info([&text] { return std::string{text}; });
    }
}

template <typename Body>
[[nodiscard]] bool guarded(wasm::State& state, std::string_view why, Body&& body) {
    state.depth++;
    state.host->beginWasmTurn(*state.plugin);
    const auto startedAt = std::chrono::steady_clock::now();
    const wire::wasm::Result<void> result = body();
    state.host->endWasmTurn(
        *state.plugin,
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - startedAt).count());
    state.depth--;
    if (!result) {
        state.host->reportWasm(*state.plugin, result.error(), why);
        return false;
    }
    return true;
}

void runListeners(wasm::State& state, std::vector<std::pair<std::int32_t, std::int32_t>> listeners,
                  std::string_view why) {
    std::ranges::stable_sort(listeners, {}, &std::pair<std::int32_t, std::int32_t>::first);
    for (const auto& [priority, token] : listeners) {
        wasm::Context context;
        context.kind = wasm::Context::Kind::Event;
        static_cast<void>(state.run(token, context, why));
    }
}
} // namespace

WasmPlugin::WasmPlugin(std::unique_ptr<wasm::State> state) : state_(std::move(state)) {}

WasmPlugin::~WasmPlugin() = default;

std::expected<std::unique_ptr<WasmPlugin>, std::string> WasmPlugin::create(PluginHost& host, LoadedPlugin& plugin,
                                                                           const wire::wasm::Engine& engine,
                                                                           std::span<const std::uint8_t> module,
                                                                           const std::string& cacheDir) {
    wire::wasm::Result<wire::wasm::Module> compiled =
        cacheDir.empty() ? wire::wasm::Module::compile(engine, module)
                         : wire::wasm::Module::compileCached(engine, module, std::filesystem::path{cacheDir});
    if (!compiled) {
        return std::unexpected("the module was rejected: " + compiled.error().message);
    }
    auto state = std::make_unique<wasm::State>(host, plugin);
    wasm::addAllImports(*state, state->imports);
    wire::wasm::Result<wire::wasm::Instance> instance = wire::wasm::Instance::create(
        *compiled,
        wire::wasm::InstanceOptions{state->imports, wire::wasm::WasiHooks{state.get(), wasiLog, nullptr, nullptr}});
    if (!instance) {
        return std::unexpected("the module could not be started: " + instance.error().message);
    }
    state->instance.emplace(std::move(*instance));
    wasm::State* raw = state.get();
    static_cast<void>(plugin.registrations.add("menus", [raw] { raw->menus->closeAll(*raw); }));
    return std::unique_ptr<WasmPlugin>{new WasmPlugin{std::move(state)}};
}

bool WasmPlugin::start() {
    wasm::State& state = *state_;
    if (!guarded(state, "init", [&state] { return state.instance->init(); })) {
        return false;
    }
    runListeners(state, state.enable, "plugin.enable");
    return !state.plugin->disabled;
}

void WasmPlugin::stop() {
    const std::lock_guard<std::recursive_mutex> turn{state_->host->turnMutex()};
    runListeners(*state_, state_->disable, "plugin.disable");
}

void WasmPlugin::close() { state_->close(); }

void WasmPlugin::interrupt() noexcept {
    if (state_->instance.has_value()) {
        state_->instance->interrupt();
    }
}
} // namespace basalt::server::plugins
