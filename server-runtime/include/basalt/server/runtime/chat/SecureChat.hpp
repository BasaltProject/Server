#pragma once

#include <basalt/server/runtime/chat/ChatMessage.hpp>
#include <basalt/server/runtime/chat/LastSeenValidator.hpp>
#include <basalt/wire/auth/SignatureValidator.hpp>
#include <basalt/wire/protocol/play/ChatPackets.hpp>

#include <atomic>
#include <cstdint>
#include <expected>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace basalt::server::runtime::chat {

/// parity: the chat half of `ServerGamePacketListenerImpl`.
class SecureChat {
public:
    /// parity: the pending count past which `sendPlayerChatMessage` disconnects.
    static constexpr std::size_t kMaxPendingChats = 4096;

    enum class SessionResult : std::uint8_t {
        Accepted,
        Unchanged,
        /// parity: "Ignoring chat session ... due to missing Services public key".
        Ignored,
        /// parity: `EXPIRED_PROFILE_PUBLIC_KEY`, a key older than the one already in use.
        ExpiredKey,
        /// parity: `ProfilePublicKey.INVALID_SIGNATURE`.
        InvalidSignature,
    };

    SecureChat(wire::common::Uuid profileId, bool enforceSecureProfile, const wire::auth::ServicesKeySet* services);

    SecureChat(const SecureChat&) = delete;
    SecureChat& operator=(const SecureChat&) = delete;
    SecureChat(SecureChat&&) = delete;
    SecureChat& operator=(SecureChat&&) = delete;
    ~SecureChat();

    /// parity: `handleChatSessionUpdate` and `resetPlayerChatState`.
    [[nodiscard]] SessionResult updateSession(const wire::protocol::play::RemoteChatSessionData& data);

    [[nodiscard]] const std::optional<wire::protocol::play::RemoteChatSessionData>& session() const noexcept {
        return session_;
    }

    [[nodiscard]] bool enforcesSecureProfile() const noexcept { return enforceSecureProfile_; }

    /// parity: `unpackAndApplyLastSeen`.
    [[nodiscard]] std::expected<std::vector<MessageSignature>, std::string>
    applyLastSeen(const wire::protocol::play::LastSeenUpdate& update);

    /// parity: `handleChatAck`.
    [[nodiscard]] std::expected<void, std::string> applyAck(std::int32_t offset);

    /// parity: `SignedMessageChain.Decoder.unpack`.
    [[nodiscard]] std::expected<PlayerChatMessage, std::string> unpack(const std::optional<MessageSignature>& signature,
                                                                       SignedBody body, std::int64_t now);

    /// parity: `Decoder.setChainBroken`.
    void breakChain() noexcept { nextIndex_.reset(); }

    /// parity: `sendPlayerChatMessage`.
    [[nodiscard]] bool send(const PlayerChatMessage& message, const BoundChatType& chatType,
                            const std::function<void(const wire::protocol::play::PlayerChatPacket&)>& write);

private:
    wire::common::Uuid profileId_;
    bool enforceSecureProfile_;
    const wire::auth::ServicesKeySet* services_;

    std::optional<wire::protocol::play::RemoteChatSessionData> session_;
    std::optional<wire::auth::RsaPublicKey> key_;
    /// parity: `SignedMessageChain.nextLink`, empty once the chain is broken.
    std::optional<std::int32_t> nextIndex_;
    std::int64_t lastTimestamp_ = 0;

    std::mutex outgoingMutex_;
    LastSeenValidator lastSeen_;
    MessageSignatureCache cache_;
    std::int32_t nextChatIndex_ = 0;
};

/// parity: `TickThrottler`.
class ChatThrottle {
public:
    /// parity: `chat-spam-threshold-seconds` and `command-spam-threshold-seconds`, both ten.
    static constexpr std::int32_t kStep = 20;
    static constexpr std::int32_t kThreshold = 20 * 10;

    void increment() noexcept { count_.fetch_add(kStep); }

    void tick() noexcept {
        std::int32_t current = count_.load();
        while (current > 0 && !count_.compare_exchange_weak(current, current - 1)) {
        }
    }

    [[nodiscard]] bool underThreshold() const noexcept { return count_.load() < kThreshold; }

private:
    std::atomic<std::int32_t> count_{0};
};

[[nodiscard]] std::optional<std::string> disconnectKeyOf(SecureChat::SessionResult result);

} // namespace basalt::server::runtime::chat
