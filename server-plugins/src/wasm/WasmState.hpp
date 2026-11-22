#pragma once

#include "WasmHandles.hpp"

#include <basalt/server/plugins/PluginHost.hpp>
#include <basalt/server/plugins/WasmPlugin.hpp>
#include <basalt/wire/wasm/Abi.hpp>
#include <basalt/wire/wasm/Call.hpp>
#include <basalt/wire/wasm/Instance.hpp>
#include <basalt/wire/wasm/Value.hpp>

#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::plugins::wasm {
namespace abi = wire::wasm;

inline constexpr std::size_t kMaxArguments = 32;
inline constexpr std::size_t kMaxPlayerSlots = 65535;

struct ArgumentView {
    std::string_view key;
    std::string_view value;
};

/// Named arguments read straight out of guest memory, so substituting them allocates nothing.
struct ArgumentList {
    std::array<ArgumentView, kMaxArguments> entries{};
    std::size_t count = 0;

    [[nodiscard]] std::span<const ArgumentView> view() const noexcept { return {entries.data(), count}; }
};

/// Nothing means the packed list is malformed or too long; a zero length is an empty list whatever the pointer.
[[nodiscard]] std::optional<ArgumentList> readArguments(const abi::GuestMemory& memory, std::int32_t ptr,
                                                        std::int32_t len);

/// Replaces each `{key}` of `body` that `arguments` names, into `out`; other braces stay.
void substitute(std::string_view body, std::span<const ArgumentView> arguments, std::string& out);

/// Substitutes into `out`; a '@' prefix names a lang key, and false means that key does not exist.
[[nodiscard]] bool renderText(const PluginText& text, std::string_view source, std::span<const ArgumentView> arguments,
                              std::string& out);

[[nodiscard]] const wire::common::toml::TomlValue* configValue(const wire::common::toml::TomlTable& table,
                                                               std::string_view dotted);

struct Context {
    enum class Kind : std::uint8_t { None, Event, Command, Task, Placeholder, MenuClick, Completion };

    Kind kind = Kind::None;
    std::int32_t id = 0;
    bool live = false;
    std::int32_t player = 0;
    PluginEvent* event = nullptr;
    PluginCommandCall* command = nullptr;
    std::string* result = nullptr;
    const wire::wasm::Value* value = nullptr;
};

/// Players this plugin has seen, by handle. A handle carries a generation, so a freed slot never answers an old one.
class PlayerTable {
public:
    [[nodiscard]] std::int32_t handleFor(const std::shared_ptr<PluginPlayerRef>& player, std::int32_t liveContext);

    /// kOk with `out` set, or the error a stale or malformed handle gets.
    [[nodiscard]] std::int32_t resolve(std::int32_t handle, std::int32_t liveContext,
                                       std::shared_ptr<PluginPlayerRef>& out);

    void clear() { slots_.clear(); }

private:
    struct Slot {
        std::shared_ptr<PluginPlayerRef> player;
        std::string uuid;
        std::int32_t graceContext = 0;
        std::uint16_t generation = 1;
    };

    void release(Slot& slot);

    std::vector<Slot> slots_;
};

class TaskTable {
public:
    [[nodiscard]] std::int32_t add(PluginHandle handle) {
        const std::int32_t task = slots_.add(std::move(handle));
        return task != 0 ? task : abi::kErrInvalid;
    }

    /// Whether the task was still pending and has been cancelled.
    bool cancel(std::int32_t task);

    void finish(std::int32_t task) { static_cast<void>(slots_.remove(task)); }

    void clear() { slots_.clear(); }

private:
    HandleTable<PluginHandle> slots_;
};

class MenuTable;

class State {
public:
    State(PluginHost& host, LoadedPlugin& plugin);
    ~State();

    State(const State&) = delete;
    State& operator=(const State&) = delete;

    PluginHost* host;
    LoadedPlugin* plugin;

    [[nodiscard]] bool capable(std::string_view name) const { return plugin->manifest.capabilities.has(name); }

    /// Callbacks that other threads can trigger hold this; it goes null once the plugin is closed.
    std::shared_ptr<State*> link;
    std::optional<abi::Instance> instance;
    std::vector<abi::HostImport> imports;
    PlayerTable players;
    TaskTable tasks;
    std::unique_ptr<MenuTable> menus;
    std::map<std::string, std::shared_ptr<void>, std::less<>> areas;
    std::vector<std::pair<std::int32_t, std::int32_t>> enable;
    std::vector<std::pair<std::int32_t, std::int32_t>> disable;
    std::vector<std::string> placeholders;
    Context current;
    std::int32_t nextContext = 1;
    std::int32_t depth = 0;
    bool warnedAboutThread = false;
    std::string scratch;

    /// The state an area keeps between calls, created on first use (the area name is the key).
    template <typename T>
    [[nodiscard]] T& areaState(std::string_view area) {
        std::shared_ptr<void>& slot = areas.try_emplace(std::string{area}).first->second;
        if (!slot) {
            slot = std::make_shared<T>();
        }
        return *static_cast<T*>(slot.get());
    }

    /// Runs one callback under the interpreter lock, the watchdog and the strike rules. False when it did not run.
    bool run(std::int32_t token, const Context& context, std::string_view why);

    [[nodiscard]] std::int32_t check(std::int32_t id, Context::Kind kind, Context*& out);

    [[nodiscard]] std::int32_t activeContext() const noexcept { return current.live ? current.id : 0; }

    [[nodiscard]] std::int32_t resolvePlayer(std::int32_t handle, std::shared_ptr<PluginPlayerRef>& out) {
        return players.resolve(handle, activeContext(), out);
    }

    [[nodiscard]] std::int32_t playerHandle(const std::shared_ptr<PluginPlayerRef>& player) {
        return players.handleFor(player, activeContext());
    }

    void close();
};

[[nodiscard]] inline State& stateOf(void* user) { return *static_cast<State*>(user); }

/// Collects the imports of every area: one src/wasm/WasmImports<Area>.cpp defining add<Area>Imports (generated).
void addAllImports(State& state, std::vector<abi::HostImport>& out);
} // namespace basalt::server::plugins::wasm
