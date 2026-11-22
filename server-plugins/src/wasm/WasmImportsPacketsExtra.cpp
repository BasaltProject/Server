#include "WasmPackets.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace basalt::server::plugins::wasm {
namespace {
using namespace packets;
using wire::wasm::Value;

[[nodiscard]] Value text(std::string_view value) { return Value{std::string{value}}; }

[[nodiscard]] Value packetInfo(const Active& active) {
    const PluginPacket& packet = *active.packet;
    Value::Map map;
    map.push_back({"kind", text("packet")});
    map.push_back({"name", text(packet.name)});
    map.push_back({"phase", text(phaseName(packet.address.phase))});
    map.push_back({"id", Value{static_cast<std::int64_t>(packet.address.id)}});
    map.push_back({"serverbound", Value{packet.address.serverbound}});
    map.push_back({"connection", Value{static_cast<std::int64_t>(packet.connection)}});
    map.push_back({"protocol", Value{static_cast<std::int64_t>(packet.clientProtocol)}});
    map.push_back({"translated", Value{packet.translated}});
    map.push_back({"size", Value{static_cast<std::int64_t>(packet.bytes.size())}});
    map.push_back({"player_name", text(packet.playerName)});
    map.push_back({"player_uuid", text(packet.playerUuid)});
    map.push_back({"cancelled", Value{packet.cancelled}});
    map.push_back({"observer", Value{active.observer}});
    return Value{std::move(map)};
}

[[nodiscard]] Value channelInfo(State& state, const Active& active) {
    std::string name;
    if (const std::shared_ptr<PluginPlayerRef> player = state.host->services().players().byUuid(*active.playerUuid)) {
        name = player->name();
    }
    Value::Map map;
    map.push_back({"kind", text("channel")});
    map.push_back({"channel", text(*active.channel)});
    map.push_back({"size", Value{static_cast<std::int64_t>(active.bytes.size())}});
    map.push_back({"player_name", text(name)});
    map.push_back({"player_uuid", text(*active.playerUuid)});
    return Value{std::move(map)};
}

void ctxInfo(void* user, abi::Call& c) {
    State& state = stateOf(user);
    Active* active = nullptr;
    const std::int32_t status = activeOf(state, c.i32(0), active);
    if (status != abi::kOk) {
        c.ret(status);
        return;
    }
    c.ret(putValue(c.memory(), c.i32(1), c.i32(2),
                   active->packet != nullptr ? packetInfo(*active) : channelInfo(state, *active)));
}

void ctxBody(void* user, abi::Call& c) {
    Active* active = nullptr;
    const std::int32_t status = activeOf(stateOf(user), c.i32(0), active);
    if (status != abi::kOk) {
        c.ret(status);
        return;
    }
    const std::string_view bytes{reinterpret_cast<const char*>(active->bytes.data()), active->bytes.size()};
    c.ret(c.memory().put(c.i32(1), c.i32(2), bytes));
}

void ctxPlayer(void* user, abi::Call& c) {
    State& state = stateOf(user);
    Active* active = nullptr;
    const std::int32_t status = activeOf(state, c.i32(0), active);
    if (status != abi::kOk) {
        c.ret(status);
        return;
    }
    const std::string& uuid = active->packet != nullptr ? active->packet->playerUuid : *active->playerUuid;
    const std::shared_ptr<PluginPlayerRef> player =
        uuid.empty() ? nullptr : state.host->services().players().byUuid(uuid);
    c.ret(player == nullptr ? 0 : state.playerHandle(player));
}

/// kOk when the ctx is a packet an answer may change.
[[nodiscard]] std::int32_t decidable(State& state, std::int32_t ctx, Active*& active) {
    const std::int32_t status = activeOf(state, ctx, active);
    if (status != abi::kOk) {
        return status;
    }
    if (active->packet == nullptr) {
        return abi::kErrInvalid;
    }
    return active->observer ? abi::kErrDenied : abi::kOk;
}

void ctxCancel(void* user, abi::Call& c) {
    Active* active = nullptr;
    const std::int32_t status = decidable(stateOf(user), c.i32(0), active);
    if (status == abi::kOk) {
        active->packet->cancelled = true;
    }
    c.ret(status);
}

void ctxSetBody(void* user, abi::Call& c) {
    Active* active = nullptr;
    std::int32_t status = decidable(stateOf(user), c.i32(0), active);
    if (status != abi::kOk) {
        c.ret(status);
        return;
    }
    const std::int32_t length = c.i32(2);
    const auto view = length < 0 || static_cast<std::size_t>(length) > kMaxBody
                          ? std::nullopt
                          : c.memory().range(static_cast<std::uint32_t>(c.i32(1)), static_cast<std::uint32_t>(length));
    if (!view) {
        c.ret(abi::kErrInvalid);
        return;
    }
    active->packet->replacement = std::vector<std::uint8_t>{view->begin(), view->end()};
    c.ret(abi::kOk);
}

void relayOnConnection(void* user, abi::Call& c, bool inbound) {
    State& state = stateOf(user);
    Active* active = nullptr;
    std::int32_t status = activeOf(state, c.i32(0), active);
    if (status == abi::kOk && active->packet == nullptr) {
        status = abi::kErrInvalid;
    }
    const abi::GuestMemory memory = c.memory();
    const std::optional<std::string_view> name = memory.string(c.i32(1), c.i32(2));
    const std::int32_t length = c.i32(4);
    const auto body = length < 0 || static_cast<std::size_t>(length) > kMaxBody
                          ? std::nullopt
                          : memory.range(static_cast<std::uint32_t>(c.i32(3)), static_cast<std::uint32_t>(length));
    if (status != abi::kOk) {
        c.ret(status);
        return;
    }
    if (!mayUsePackets(state)) {
        c.ret(abi::kErrDenied);
        return;
    }
    const std::optional<PacketAddress> address = name ? state.host->services().packets().resolve(*name) : std::nullopt;
    if (!address || address->serverbound != inbound || !body) {
        c.ret(abi::kErrInvalid);
        return;
    }
    PacketEndpoint* endpoint = active->packet->endpoint.get();
    const std::span<const std::uint8_t> bytes{body->data(), body->size()};
    const bool intercept = c.i32(5) != 0;
    const bool done = endpoint != nullptr && (inbound ? endpoint->receiveServerbound(address->id, bytes, intercept)
                                                      : endpoint->sendClientbound(address->id, bytes, intercept));
    c.ret(done ? 1 : 0);
}

void ctxSend(void* user, abi::Call& c) { relayOnConnection(user, c, false); }

void ctxReceive(void* user, abi::Call& c) { relayOnConnection(user, c, true); }
} // namespace

void addPacketsExtraImports(State& state, std::vector<abi::HostImport>& out) {
    void* user = &state;
    out.insert(out.end(), {
                              {"packet_ctx_info", ctxInfo, user},
                              {"packet_ctx_body", ctxBody, user},
                              {"packet_ctx_player", ctxPlayer, user},
                              {"packet_ctx_cancel", ctxCancel, user},
                              {"packet_ctx_set_body", ctxSetBody, user},
                              {"packet_ctx_send", ctxSend, user},
                              {"packet_ctx_receive", ctxReceive, user},
                          });
}
} // namespace basalt::server::plugins::wasm
