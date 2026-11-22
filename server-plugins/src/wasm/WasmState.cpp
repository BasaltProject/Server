#include "WasmState.hpp"

#include "WasmMenus.hpp"

#include <chrono>
#include <cstring>

namespace basalt::server::plugins::wasm {
namespace {
using wire::common::toml::TomlTable;
using wire::common::toml::TomlValue;

constexpr std::int32_t kMaxGeneration = 0x7FFF;

[[nodiscard]] std::int32_t encode(std::size_t index, std::uint16_t generation) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(generation) << 16) |
           static_cast<std::int32_t>(index + 1);
}

[[nodiscard]] std::uint16_t nextGeneration(std::uint16_t generation) {
    return generation >= kMaxGeneration ? static_cast<std::uint16_t>(1) : static_cast<std::uint16_t>(generation + 1);
}

struct Decoded {
    std::size_t index = 0;
    std::uint16_t generation = 0;
};

[[nodiscard]] std::optional<Decoded> decode(std::int32_t handle) {
    if (handle <= 0) {
        return std::nullopt;
    }
    const auto low = static_cast<std::uint32_t>(handle) & 0xFFFFU;
    if (low == 0) {
        return std::nullopt;
    }
    return Decoded{low - 1, static_cast<std::uint16_t>(static_cast<std::uint32_t>(handle) >> 16)};
}
} // namespace

void PlayerTable::release(Slot& slot) {
    slot.player.reset();
    slot.uuid.clear();
    slot.graceContext = 0;
    slot.generation = nextGeneration(slot.generation);
}

std::int32_t PlayerTable::handleFor(const std::shared_ptr<PluginPlayerRef>& player, std::int32_t liveContext) {
    if (player == nullptr) {
        return 0;
    }
    const std::string uuid = player->uuid();
    constexpr std::size_t kNone = static_cast<std::size_t>(-1);
    std::size_t reusable = kNone;
    for (std::size_t index = 0; index < slots_.size(); ++index) {
        Slot& slot = slots_[index];
        if (slot.player == nullptr) {
            reusable = reusable == kNone ? index : reusable;
            continue;
        }
        if (slot.uuid == uuid) {
            slot.player = player;
            slot.graceContext = liveContext;
            return encode(index, slot.generation);
        }
        if (reusable == kNone && !slot.player->online() && (liveContext == 0 || slot.graceContext != liveContext)) {
            reusable = index;
        }
    }
    if (reusable == kNone) {
        if (slots_.size() >= kMaxPlayerSlots) {
            return 0;
        }
        slots_.emplace_back();
        reusable = slots_.size() - 1;
    } else if (slots_[reusable].player != nullptr) {
        release(slots_[reusable]);
    }
    Slot& slot = slots_[reusable];
    slot.player = player;
    slot.uuid = uuid;
    slot.graceContext = liveContext;
    return encode(reusable, slot.generation);
}

std::int32_t PlayerTable::resolve(std::int32_t handle, std::int32_t liveContext,
                                  std::shared_ptr<PluginPlayerRef>& out) {
    const std::optional<Decoded> parts = decode(handle);
    if (!parts || parts->index >= slots_.size()) {
        return abi::kErrInvalid;
    }
    Slot& slot = slots_[parts->index];
    if (slot.player == nullptr || slot.generation != parts->generation) {
        return abi::kErrStale;
    }
    const bool inGrace = liveContext != 0 && slot.graceContext == liveContext;
    if (!slot.player->online() && !inGrace) {
        release(slot);
        return abi::kErrStale;
    }
    out = slot.player;
    return abi::kOk;
}

bool TaskTable::cancel(std::int32_t task) {
    PluginHandle* handle = nullptr;
    if (slots_.find(task, handle) != abi::kOk) {
        return false;
    }
    handle->remove();
    return slots_.remove(task);
}

std::int32_t MenuTable::create(std::string title, std::int32_t rows) {
    auto menu = std::make_unique<Menu>();
    menu->title = std::move(title);
    menu->rows = rows;
    menus_.push_back(std::move(menu));
    return static_cast<std::int32_t>(menus_.size());
}

Menu* MenuTable::find(std::int32_t handle) {
    if (handle < 1 || static_cast<std::size_t>(handle) > menus_.size()) {
        return nullptr;
    }
    return menus_[static_cast<std::size_t>(handle - 1)].get();
}

void MenuTable::closeAll(State& state) {
    for (const std::unique_ptr<Menu>& menu : menus_) {
        for (auto& [uuid, handle] : menu->opened) {
            const std::shared_ptr<PluginPlayerRef> player = state.host->services().players().byUuid(uuid);
            if (player != nullptr) {
                state.host->services().menus().close(*player);
            }
            handle.remove();
        }
        menu->opened.clear();
    }
}

State::State(PluginHost& host, LoadedPlugin& plugin)
    : host(&host), plugin(&plugin), link(std::make_shared<State*>(this)), menus(std::make_unique<MenuTable>()) {}

State::~State() = default;

void State::close() {
    const std::lock_guard<std::recursive_mutex> turn{host->turnMutex()};
    *link = nullptr;
}

std::int32_t State::check(std::int32_t id, Context::Kind kind, Context*& out) {
    if (current.live && id == current.id) {
        if (current.kind != kind) {
            return abi::kErrInvalid;
        }
        out = &current;
        return abi::kOk;
    }
    return id >= 1 && id < nextContext ? abi::kErrStale : abi::kErrInvalid;
}

bool State::run(std::int32_t token, const Context& context, std::string_view why) {
    if (plugin->disabled || !instance.has_value()) {
        return false;
    }
    if (depth > 0) {
        plugin->log.warn([&why] { return "a re-entrant call for " + std::string{why} + " was skipped"; });
        return false;
    }
    const auto startedAt = std::chrono::steady_clock::now();
    current = context;
    current.id = nextContext++;
    current.live = true;
    ++depth;
    host->beginWasmTurn(*plugin);
    const abi::Result<void> result = instance->callback(token, current.id);
    --depth;
    current.live = false;
    current.event = nullptr;
    current.command = nullptr;
    current.result = nullptr;
    current.value = nullptr;
    host->endWasmTurn(
        *plugin,
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - startedAt).count());
    if (!result) {
        host->reportWasm(*plugin, result.error(), why);
        return false;
    }
    plugin->strikes = 0;
    return true;
}

std::optional<ArgumentList> readArguments(const abi::GuestMemory& memory, std::int32_t ptr, std::int32_t len) {
    ArgumentList list;
    if (len == 0) {
        return list;
    }
    const std::optional<std::string_view> bytes = memory.string(ptr, len);
    if (!bytes) {
        return std::nullopt;
    }
    std::size_t at = 0;
    const auto readU32 = [&](std::uint32_t& out) {
        if (bytes->size() - at < 4) {
            return false;
        }
        std::memcpy(&out, bytes->data() + at, 4);
        at += 4;
        return true;
    };
    const auto readText = [&](std::string_view& out) {
        std::uint32_t length = 0;
        if (!readU32(length) || bytes->size() - at < length) {
            return false;
        }
        out = bytes->substr(at, length);
        at += length;
        return true;
    };
    std::uint32_t count = 0;
    if (!readU32(count) || count > kMaxArguments) {
        return std::nullopt;
    }
    for (std::uint32_t index = 0; index < count; ++index) {
        if (!readText(list.entries[index].key) || !readText(list.entries[index].value)) {
            return std::nullopt;
        }
    }
    list.count = count;
    if (at != bytes->size()) {
        return std::nullopt;
    }
    return list;
}

void substitute(std::string_view body, std::span<const ArgumentView> arguments, std::string& out) {
    out.clear();
    std::size_t at = 0;
    while (at < body.size()) {
        const std::size_t open = body.find('{', at);
        if (open == std::string_view::npos) {
            out.append(body.substr(at));
            break;
        }
        out.append(body.substr(at, open - at));
        const std::size_t close = body.find('}', open);
        bool replaced = false;
        if (close != std::string_view::npos) {
            const std::string_view key = body.substr(open + 1, close - open - 1);
            for (const ArgumentView& argument : arguments) {
                if (argument.key == key) {
                    out.append(argument.value);
                    replaced = true;
                    break;
                }
            }
            if (replaced) {
                at = close + 1;
                continue;
            }
        }
        out.push_back('{');
        at = open + 1;
    }
}

bool renderText(const PluginText& text, std::string_view source, std::span<const ArgumentView> arguments,
                std::string& out) {
    if (!source.starts_with('@')) {
        substitute(source, arguments, out);
        return true;
    }
    const std::string* found = text.find(source.substr(1));
    if (found == nullptr) {
        return false;
    }
    substitute(*found, arguments, out);
    return true;
}

const TomlValue* configValue(const TomlTable& table, std::string_view dotted) {
    const TomlTable* level = &table;
    while (true) {
        const std::size_t dot = dotted.find('.');
        if (dot == std::string_view::npos) {
            return level->find(dotted);
        }
        level = level->table(dotted.substr(0, dot));
        if (level == nullptr) {
            return nullptr;
        }
        dotted.remove_prefix(dot + 1);
    }
}
} // namespace basalt::server::plugins::wasm
