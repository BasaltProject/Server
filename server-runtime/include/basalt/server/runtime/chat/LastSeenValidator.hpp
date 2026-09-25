#pragma once

#include <basalt/server/runtime/chat/ChatMessage.hpp>

#include <cstddef>
#include <cstdint>
#include <deque>
#include <expected>
#include <optional>
#include <string>
#include <vector>

namespace basalt::server::runtime::chat {

/// parity: `LastSeenMessagesValidator`: the signed messages a client was sent and has not yet
/// said it saw, and the window its acknowledgements move over. An error is the reason the client
/// is disconnected for desyncing.
class LastSeenValidator {
public:
    /// parity: `ServerGamePacketListenerImpl.lastSeenMessages`, twenty wide.
    static constexpr std::size_t kWindow = wire::protocol::play::LastSeenUpdate::kWindow;

    LastSeenValidator();

    /// parity: `addPending`, which skips a signature sent twice in a row.
    void addPending(const MessageSignature& signature);

    [[nodiscard]] std::size_t trackedCount() const noexcept { return tracked_.size(); }

    [[nodiscard]] std::expected<void, std::string> applyOffset(std::int32_t offset);

    /// The signatures the client says it has seen, which is what its signed body carries.
    [[nodiscard]] std::expected<std::vector<MessageSignature>, std::string>
    applyUpdate(const wire::protocol::play::LastSeenUpdate& update);

    /// parity: `LastSeenMessages.computeChecksum`, the Java array hashes folded as a list's.
    [[nodiscard]] static std::int8_t checksumOf(const std::vector<MessageSignature>& signatures);

private:
    struct Entry {
        MessageSignature signature{};
        bool pending = true;
    };

    std::deque<std::optional<Entry>> tracked_;
    std::optional<MessageSignature> lastPending_;
};

/// parity: `MessageSignatureCache`, what a client is assumed to remember so a signature it already
/// has goes out as an index.
class MessageSignatureCache {
public:
    static constexpr std::size_t kCapacity = 128;

    /// The slot holding this signature, or -1.
    [[nodiscard]] std::int32_t pack(const MessageSignature& signature) const;

    /// parity: `push(body, signature)`: what the body saw, then the message itself, newest first.
    void push(const std::vector<MessageSignature>& lastSeen, const std::optional<MessageSignature>& signature);

private:
    std::vector<std::optional<MessageSignature>> entries_ = std::vector<std::optional<MessageSignature>>(kCapacity);
};

} // namespace basalt::server::runtime::chat
