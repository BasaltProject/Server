#pragma once

#include <basalt/wire/common/Uuid.hpp>
#include <basalt/wire/protocol/play/ChatPackets.hpp>
#include <basalt/wire/text/Component.hpp>

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/// What a chat line is between arriving and being shown: its signed body, its link in the sender's
/// chain and the chat type it is shown with.
namespace basalt::server::runtime::chat {

using wire::protocol::play::MessageSignature;

/// The index of each entry of `minecraft:chat_type` as this server sends it. parity: the registry
/// comes from the vanilla pack, and pack loading sorts its entries by name.
namespace chatType {
inline constexpr std::int32_t kChat = 0;
inline constexpr std::int32_t kEmoteCommand = 1;
inline constexpr std::int32_t kMsgCommandIncoming = 2;
inline constexpr std::int32_t kMsgCommandOutgoing = 3;
inline constexpr std::int32_t kSayCommand = 4;
inline constexpr std::int32_t kTeamMsgCommandIncoming = 5;
inline constexpr std::int32_t kTeamMsgCommandOutgoing = 6;

inline constexpr std::array<std::string_view, 7> kNames{"chat",
                                                        "emote_command",
                                                        "msg_command_incoming",
                                                        "msg_command_outgoing",
                                                        "say_command",
                                                        "team_msg_command_incoming",
                                                        "team_msg_command_outgoing"};
} // namespace chatType

/// parity: `ChatType.Bound`.
struct BoundChatType {
    std::int32_t type = chatType::kChat;
    wire::text::Component name;
    std::optional<wire::text::Component> target;

    /// parity: `ChatType.Bound.decorate`, the line a client draws from the chat type's decoration.
    [[nodiscard]] wire::text::Component decorate(const wire::text::Component& content) const;

    [[nodiscard]] BoundChatType withTarget(wire::text::Component targetName) const;

    [[nodiscard]] wire::protocol::play::ChatTypeBound toWire() const;
};

/// parity: `SignedMessageBody`. The timestamp is epoch milliseconds.
struct SignedBody {
    std::string content;
    std::int64_t timestamp = 0;
    std::int64_t salt = 0;
    std::vector<MessageSignature> lastSeen;
};

/// parity: `PlayerChatMessage`, with its `SignedMessageLink` flattened in. No filter mask: this
/// server filters nothing, so every message passes through.
struct PlayerChatMessage {
    /// parity: `MESSAGE_EXPIRES_AFTER_SERVER`, five minutes.
    static constexpr std::int64_t kExpiresAfterServerMillis = 5LL * 60LL * 1000LL;

    wire::common::Uuid sender;
    wire::common::Uuid sessionId;
    std::int32_t index = 0;
    std::optional<MessageSignature> signature;
    SignedBody body;
    std::optional<wire::text::Component> unsignedContent;

    /// parity: `PlayerChatMessage.unsigned`: no salt, nothing seen, stamped now.
    [[nodiscard]] static PlayerChatMessage unsignedFrom(const wire::common::Uuid& sender, std::string content,
                                                        std::int64_t now);

    /// parity: `PlayerChatMessage.system`, the nil sender, which relays as disguised chat.
    [[nodiscard]] static PlayerChatMessage system(std::string content, std::int64_t now);

    [[nodiscard]] bool isSystem() const noexcept { return sender.isNil(); }

    [[nodiscard]] bool hasSignature() const noexcept { return signature.has_value(); }

    [[nodiscard]] wire::text::Component decoratedContent() const;

    /// parity: `withUnsignedContent`, which keeps nothing when it only says the signed text again.
    [[nodiscard]] PlayerChatMessage withUnsignedContent(const wire::text::Component& content) const;

    /// parity: `PlayerChatMessage.updateSignature`: the version, the link and the body, as signed.
    [[nodiscard]] std::vector<std::uint8_t> signedPayload() const;

    [[nodiscard]] bool hasExpiredServer(std::int64_t now) const noexcept {
        return now > body.timestamp + kExpiresAfterServerMillis;
    }
};

/// parity: `CommandSigningContext.SignedArguments`, the verified message arguments of one command.
using SignedArguments = std::map<std::string, PlayerChatMessage, std::less<>>;

/// The wall clock in epoch milliseconds, which is what signatures are stamped with.
[[nodiscard]] std::int64_t nowMillis();

/// parity: `StringUtil.isAllowedChatCharacter` over every character: no section sign, no control
/// character and no delete.
[[nodiscard]] bool isChatMessageIllegal(std::string_view message);

} // namespace basalt::server::runtime::chat
