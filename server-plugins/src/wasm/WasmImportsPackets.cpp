#include "WasmPackets.hpp"

#include <basalt/server/plugins/PluginPlayerOps.hpp>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

namespace basalt::server::plugins::wasm {
namespace packets {
namespace {
constexpr std::string_view kActionPrefix = "receive_";

[[nodiscard]] bool granted(const State& state, wire::plugins::PluginPermission permission) {
    return state.host->allowed(state.plugin->manifest.id).contains(permission);
}
} // namespace

bool mayUsePackets(const State& state) {
    return state.capable("packets") && granted(state, wire::plugins::PluginPermission::Packets);
}

bool mayUseChannel(const State& state, std::string_view channel) {
    const std::vector<std::string>& listed = state.plugin->manifest.capabilities.network;
    return std::ranges::find(listed, channel) != listed.end() &&
           granted(state, wire::plugins::PluginPermission::Network);
}

std::int32_t activeOf(State& state, std::int32_t ctx, Active*& out) {
    Context* context = nullptr;
    const std::int32_t status = state.check(ctx, Context::Kind::None, context);
    if (status != abi::kOk) {
        return status;
    }
    Active* active = ownState(state).active;
    if (active == nullptr) {
        return abi::kErrInvalid;
    }
    out = active;
    return abi::kOk;
}

std::string_view phaseName(PacketPhase phase) {
    switch (phase) {
    case PacketPhase::Handshake:
        return "handshake";
    case PacketPhase::Status:
        return "status";
    case PacketPhase::Login:
        return "login";
    case PacketPhase::Configuration:
        return "configuration";
    case PacketPhase::Play:
        return "play";
    }
    return "play";
}

bool runListener(State& state, ListenerSlot& slot, std::int32_t token, Active& active, std::string_view why) {
    PacketsState& own = ownState(state);
    Active* const previous = own.active;
    own.active = &active;
    const auto started = std::chrono::steady_clock::now();
    const bool ran = state.run(token, Context{}, why);
    const auto elapsed = std::chrono::steady_clock::now() - started;
    own.active = previous;
    if (!ran) {
        return false;
    }
    if (elapsed <= kBudget) {
        slot.strikes = 0;
        return true;
    }
    ++slot.strikes;
    const bool drop = slot.strikes >= kMaxStrikes;
    state.plugin->log.warn([&] {
        return "a listener for " + std::string{why} + " took " +
               std::to_string(std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count()) +
               " us on the network thread (budget " + std::to_string(kBudget.count()) + " ms), its answer was dropped" +
               (drop ? " and it was removed" : "");
    });
    if (drop) {
        slot.dead = true;
        slot.handle.remove();
        static_cast<void>(own.listeners.remove(slot.id));
    }
    return false;
}
} // namespace packets

namespace {
using namespace packets;

[[nodiscard]] std::optional<std::span<const std::uint8_t>> bodyOf(const abi::GuestMemory& memory, std::int32_t ptr,
                                                                  std::int32_t len) {
    if (len < 0 || static_cast<std::size_t>(len) > kMaxBody) {
        return std::nullopt;
    }
    const auto view = memory.range(static_cast<std::uint32_t>(ptr), static_cast<std::uint32_t>(len));
    if (!view) {
        return std::nullopt;
    }
    return std::span<const std::uint8_t>{view->data(), view->size()};
}

void dispatchPacket(PluginHost& host, const std::shared_ptr<State*>& link, const std::shared_ptr<ListenerSlot>& slot,
                    std::int32_t token, bool observer, PluginPacket& packet) {
    const std::lock_guard<std::recursive_mutex> turn{host.turnMutex()};
    State* state = *link;
    if (state == nullptr || slot->dead) {
        return;
    }
    if (packet.name.empty()) {
        packet.name = host.services().packets().nameOf(packet.address);
    }
    Active active{.packet = &packet, .bytes = packet.bytes, .observer = observer};
    const bool cancelled = packet.cancelled;
    std::optional<std::vector<std::uint8_t>> replacement = packet.replacement;
    if (!runListener(*state, *slot, token, active, packet.name)) {
        packet.cancelled = cancelled;
        packet.replacement = std::move(replacement);
    }
}

void dispatchChannel(PluginHost& host, const std::shared_ptr<State*>& link, const std::shared_ptr<ListenerSlot>& slot,
                     std::int32_t token, const std::string& channel, const std::string& uuid,
                     std::span<const std::uint8_t> bytes) {
    const std::lock_guard<std::recursive_mutex> turn{host.turnMutex()};
    State* state = *link;
    if (state == nullptr || slot->dead) {
        return;
    }
    Active active{.channel = &channel, .playerUuid = &uuid, .bytes = bytes};
    static_cast<void>(runListener(*state, *slot, token, active, channel));
}

void channelListen(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const std::optional<std::string_view> given = c.memory().string(c.i32(0), c.i32(1));
    if (!given || given->empty()) {
        c.ret(abi::kErrInvalid);
        return;
    }
    if (!mayUseChannel(state, *given)) {
        c.ret(abi::kErrDenied);
        return;
    }
    const std::string name{*given};
    const std::int32_t token = c.i32(2);
    auto slot = std::make_shared<ListenerSlot>();
    const std::shared_ptr<State*> link = state.link;
    PluginHost* host = state.host;
    slot->handle =
        host->listenNative(name, [host, link, slot, token](const std::string& channel, const std::string& uuid,
                                                           std::span<const std::uint8_t> bytes) {
            dispatchChannel(*host, link, slot, token, channel, uuid, bytes);
        });
    static_cast<void>(state.plugin->registrations.add("channel(" + name + ")", [slot] {
        slot->dead = true;
        slot->handle.remove();
    }));
    c.ret(abi::kOk);
}

void channelSend(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const abi::GuestMemory memory = c.memory();
    const std::optional<std::string_view> name = memory.string(c.i32(1), c.i32(2));
    const auto body = bodyOf(memory, c.i32(3), c.i32(4));
    if (!name || name->empty() || !body) {
        c.ret(abi::kErrInvalid);
        return;
    }
    if (!mayUseChannel(state, *name)) {
        c.ret(abi::kErrDenied);
        return;
    }
    std::shared_ptr<PluginPlayerRef> player;
    const std::int32_t status = state.resolvePlayer(c.i32(0), player);
    c.ret(status != abi::kOk ? status : (state.host->services().channels().send(*player, *name, *body) ? 1 : 0));
}

[[nodiscard]] std::optional<PacketScope> scopeFor(PluginHost& host, std::string_view name) {
    return host.services().packets().scopeOf(name);
}

void packetListen(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const abi::GuestMemory memory = c.memory();
    const std::optional<std::string_view> pattern = memory.string(c.i32(0), c.i32(1));
    const std::int32_t token = c.i32(4);
    if (!mayUsePackets(state)) {
        c.ret(abi::kErrDenied);
        return;
    }
    if (!pattern) {
        c.ret(abi::kErrInvalid);
        return;
    }
    PluginEventPriority priority = PluginEventPriority::Normal;
    bool observer = false;
    if (c.i32(3) > 0) {
        const auto options = readValue(memory, c.i32(2), c.i32(3));
        if (!options) {
            c.ret(abi::kErrInvalid);
            return;
        }
        if (!options->isNull()) {
            if (options->asMap() == nullptr) {
                c.ret(abi::kErrInvalid);
                return;
            }
            if (const wire::wasm::Value* spelled = options->find("priority")) {
                const std::string* text = spelled->asString();
                const std::optional<PluginEventPriority> read =
                    text != nullptr ? priorityOf(*text) : std::optional<PluginEventPriority>{};
                if (!read) {
                    c.ret(abi::kErrInvalid);
                    return;
                }
                priority = *read;
            }
            if (const wire::wasm::Value* flag = options->find("observe")) {
                if (flag->asBool() == nullptr) {
                    c.ret(abi::kErrInvalid);
                    return;
                }
                observer = *flag->asBool();
            }
        }
    }
    const std::optional<PacketScope> scope = scopeFor(*state.host, *pattern);
    if (!scope) {
        c.ret(abi::kErrInvalid);
        return;
    }
    auto slot = std::make_shared<ListenerSlot>();
    slot->id = ownState(state).listeners.add(slot);
    if (slot->id == 0) {
        c.ret(abi::kErrInvalid);
        return;
    }
    PluginHost* host = state.host;
    const std::shared_ptr<State*> link = state.link;
    slot->handle = host->packets().addNative(*scope, static_cast<std::int32_t>(priority), observer,
                                             [host, link, slot, token, observer](PluginPacket& packet) {
                                                 dispatchPacket(*host, link, slot, token, observer, packet);
                                             });
    static_cast<void>(state.plugin->registrations.add("packet(" + std::string{*pattern} + ")", [slot] {
        slot->dead = true;
        slot->handle.remove();
    }));
    c.ret(slot->id);
}

void packetUnlisten(void* user, abi::Call& c) {
    State& state = stateOf(user);
    std::shared_ptr<ListenerSlot>* found = nullptr;
    const std::int32_t status = ownState(state).listeners.find(c.i32(0), found);
    if (status != abi::kOk) {
        c.ret(status);
        return;
    }
    const std::shared_ptr<ListenerSlot> slot = *found;
    slot->dead = true;
    slot->handle.remove();
    static_cast<void>(ownState(state).listeners.remove(c.i32(0)));
    c.ret(abi::kOk);
}

enum class Direction : std::uint8_t { Clientbound, Serverbound };

void relayToPlayer(void* user, abi::Call& c, Direction direction) {
    State& state = stateOf(user);
    const abi::GuestMemory memory = c.memory();
    const std::optional<std::string_view> name = memory.string(c.i32(1), c.i32(2));
    const auto body = bodyOf(memory, c.i32(3), c.i32(4));
    if (!mayUsePackets(state)) {
        c.ret(abi::kErrDenied);
        return;
    }
    if (!name || !body) {
        c.ret(abi::kErrInvalid);
        return;
    }
    const std::optional<PacketAddress> address = state.host->services().packets().resolve(*name);
    if (!address || address->serverbound != (direction == Direction::Serverbound)) {
        c.ret(abi::kErrInvalid);
        return;
    }
    std::shared_ptr<PluginPlayerRef> player;
    const std::int32_t status = state.resolvePlayer(c.i32(0), player);
    if (status != abi::kOk) {
        c.ret(status);
        return;
    }
    PluginPacketSender& sender = state.host->services().packets();
    const bool intercept = c.i32(5) != 0;
    const bool done = direction == Direction::Serverbound ? sender.receive(*player, *address, *body, intercept)
                                                          : sender.send(*player, *address, *body, intercept);
    c.ret(done ? 1 : 0);
}

void packetSend(void* user, abi::Call& c) { relayToPlayer(user, c, Direction::Clientbound); }

void packetReceive(void* user, abi::Call& c) { relayToPlayer(user, c, Direction::Serverbound); }

void packetLookup(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const abi::GuestMemory memory = c.memory();
    const std::optional<std::string_view> name = memory.string(c.i32(0), c.i32(1));
    if (!mayUsePackets(state)) {
        c.ret(abi::kErrDenied);
        return;
    }
    const std::optional<PacketScope> scope = name ? scopeFor(*state.host, *name) : std::nullopt;
    if (!scope) {
        c.ret(abi::kErrInvalid);
        return;
    }
    using wire::wasm::Value;
    const bool exact = scope->phase >= 0 && scope->direction >= 0 && scope->id >= 0;
    Value::Map map;
    map.push_back({"name", exact ? Value{state.host->services().packets().nameOf(PacketAddress{
                                       static_cast<PacketPhase>(scope->phase), scope->direction == 1, scope->id})}
                                 : Value{std::string{*name}}});
    map.push_back(
        {"phase", scope->phase >= 0 ? Value{std::string{phaseName(static_cast<PacketPhase>(scope->phase))}} : Value{}});
    map.push_back({"serverbound", scope->direction >= 0 ? Value{scope->direction == 1} : Value{}});
    map.push_back({"id", scope->id >= 0 ? Value{static_cast<std::int64_t>(scope->id)} : Value{}});
    map.push_back({"wildcard", Value{!exact}});
    c.ret(putValue(memory, c.i32(2), c.i32(3), Value{std::move(map)}));
}

void packetNames(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const abi::GuestMemory memory = c.memory();
    const std::optional<std::string_view> prefix = memory.string(c.i32(0), c.i32(1));
    if (!mayUsePackets(state)) {
        c.ret(abi::kErrDenied);
        return;
    }
    if (!prefix) {
        c.ret(abi::kErrInvalid);
        return;
    }
    wire::wasm::Value::List list;
    for (std::string& each : state.host->services().packets().names()) {
        if (each.starts_with(*prefix)) {
            list.emplace_back(std::move(each));
        }
    }
    c.ret(putValue(memory, c.i32(2), c.i32(3), wire::wasm::Value{std::move(list)}));
}

void packetPlayerActions(void* user, abi::Call& c) {
    State& state = stateOf(user);
    if (!mayUsePackets(state)) {
        c.ret(abi::kErrDenied);
        return;
    }
    wire::wasm::Value::List list;
    for (const EntityOperationSpec& spec : kPlayerOperations) {
        if (spec.name.starts_with(kActionPrefix)) {
            list.emplace_back(std::string{spec.name.substr(kActionPrefix.size())});
        }
    }
    c.ret(putValue(c.memory(), c.i32(0), c.i32(1), wire::wasm::Value{std::move(list)}));
}

[[nodiscard]] std::optional<PluginEntityValue> entityValueOf(const wire::wasm::Value& value) {
    if (value.isNull()) {
        return PluginEntityValue{};
    }
    if (const bool* flag = value.asBool()) {
        return PluginEntityValue{*flag};
    }
    if (const std::int64_t* whole = value.asInt()) {
        return PluginEntityValue{*whole};
    }
    if (const double* real = value.asFloat()) {
        return PluginEntityValue{*real};
    }
    if (const std::string* text = value.asString()) {
        return PluginEntityValue{*text};
    }
    if (const wire::wasm::Value::List* list = value.asList()) {
        std::vector<std::string> strings;
        for (const wire::wasm::Value& item : *list) {
            const std::string* text = item.asString();
            if (text == nullptr) {
                return std::nullopt;
            }
            strings.push_back(*text);
        }
        return PluginEntityValue{std::move(strings)};
    }
    return std::nullopt;
}

void packetPlayerReceive(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const abi::GuestMemory memory = c.memory();
    const std::optional<std::string_view> action = memory.string(c.i32(1), c.i32(2));
    if (!mayUsePackets(state)) {
        c.ret(abi::kErrDenied);
        return;
    }
    const auto args = readValue(memory, c.i32(3), c.i32(4));
    const wire::wasm::Value::List* list = args ? args->asList() : nullptr;
    std::string operation{kActionPrefix};
    if (action) {
        operation.append(*action);
    }
    if (!action || list == nullptr || !isPlayerOperation(operation)) {
        c.ret(abi::kErrInvalid);
        return;
    }
    std::vector<PluginEntityValue> converted;
    for (const wire::wasm::Value& item : *list) {
        std::optional<PluginEntityValue> value = entityValueOf(item);
        if (!value) {
            c.ret(abi::kErrInvalid);
            return;
        }
        converted.push_back(std::move(*value));
    }
    std::shared_ptr<PluginPlayerRef> player;
    const std::int32_t status = state.resolvePlayer(c.i32(0), player);
    if (status != abi::kOk) {
        c.ret(status);
        return;
    }
    const std::optional<PluginEntityValue> result = player->invoke(operation, converted);
    const bool* flag = result ? std::get_if<bool>(&*result) : nullptr;
    c.ret(result && (flag == nullptr || *flag) ? 1 : 0);
}
} // namespace

void addPacketsImports(State& state, std::vector<abi::HostImport>& out) {
    void* user = &state;
    out.insert(out.end(), {
                              {"channel_listen", channelListen, user},
                              {"channel_send", channelSend, user},
                              {"packet_listen", packetListen, user},
                              {"packet_unlisten", packetUnlisten, user},
                              {"packet_send", packetSend, user},
                              {"packet_receive", packetReceive, user},
                              {"packet_lookup", packetLookup, user},
                              {"packet_names", packetNames, user},
                              {"packet_player_actions", packetPlayerActions, user},
                              {"packet_player_receive", packetPlayerReceive, user},
                          });
}
} // namespace basalt::server::plugins::wasm
