#include "SessionPlayShared.hpp"

#include <basalt/server/runtime/ClientSession.hpp>
#include <basalt/server/runtime/chat/ChatBroadcast.hpp>
#include <basalt/server/runtime/command/CommandDispatcher.hpp>
#include <basalt/server/runtime/command/CommandSource.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/wire/common/placeholder/Placeholders.hpp>
#include <basalt/wire/multiversion/ProtocolVersions.hpp>
#include <basalt/wire/protocol/play/ChatPackets.hpp>
#include <basalt/wire/text/MiniMessage.hpp>
#include <basalt/wire/text/TextColor.hpp>
#include <basalt/wire/text/TranslationResolver.hpp>

#include <algorithm>
#include <string>

namespace basalt::server::runtime {
namespace {

namespace play = wire::protocol::play;
using player::PlayerSession;
using wire::text::Component;

/// parity: `ServerGamePacketListenerImpl.INVALID_COMMAND_SIGNATURE`.
constexpr std::string_view kInvalidCommandSignature = "chat.disabled.invalid_command_signature";

[[nodiscard]] std::string joinNames(const std::vector<std::string>& names) {
    std::string joined;
    for (const std::string& name : names) {
        joined += joined.empty() ? name : ", " + name;
    }
    return joined;
}

} // namespace

bool ClientSession::translatedClient() const {
    return handshake_.has_value() && handshake_->protocolVersion != wire::multiversion::protocolVersions::kNative;
}

bool ClientSession::tryHandleChat(const std::string& message, bool isCommand) {
    PlayerSession* session = playerSession_.get();
    if (session == nullptr) {
        return false;
    }
    if (chat::isChatMessageIllegal(message)) {
        disconnect(Component::translatable("multiplayer.disconnect.illegal_characters"));
        return false;
    }
    if (!isCommand && session->chatVisibility.load() == PlayerSession::ChatVisibility::Hidden) {
        sendPlayPacket(play::SystemChatPacket::kId, [](wire::io::ByteBuffer& buffer) {
            play::SystemChatPacket{.content = Component::translatable("chat.disabled.options").color(wire::text::kRed),
                                   .isActionBar = false}
                .write(buffer);
        });
        return false;
    }
    return true;
}

bool ClientSession::handleChatPacket(std::int32_t packetId, wire::io::ByteBuffer& body) {
    const bool chatPacket = packetId == play::ChatMessagePacket::kId || packetId == play::ChatCommandPacket::kId ||
                            packetId == play::SignedChatCommandPacket::kId || packetId == play::ChatAckPacket::kId ||
                            packetId == play::ChatSessionUpdatePacket::kId;
    PlayerSession* session = playerSession_.get();
    if (!chatPacket || session == nullptr || secureChat_ == nullptr) {
        return chatPacket;
    }
    const auto validationFailed = [this, session](const std::string& reason) {
        logger_.error([session, &reason] {
            return "Failed to validate message acknowledgements from " + session->name() + ": " + reason;
        });
        disconnect(Component::translatable("multiplayer.disconnect.chat_validation_failed"));
    };
    // A translated client speaks an older chain than this one, so its acknowledgements are not read.
    const auto lastSeen =
        [this,
         &validationFailed](const play::LastSeenUpdate& update) -> std::optional<std::vector<chat::MessageSignature>> {
        if (translatedClient()) {
            return std::vector<chat::MessageSignature>{};
        }
        std::expected<std::vector<chat::MessageSignature>, std::string> seen = secureChat_->applyLastSeen(update);
        if (!seen.has_value()) {
            validationFailed(seen.error());
            return std::nullopt;
        }
        return *seen;
    };
    const auto decodeFailed = [this, session](const std::string& key) {
        logger_.warn([this, session, &key] {
            return "Failed to update secure chat state for " + session->name() + ": '" +
                   wire::text::resolvePlainText(Component::translatable(key), consoleLang()) + "'";
        });
        session->sendSystemMessage(Component::translatable(key).color(wire::text::kRed));
    };

    if (packetId == play::ChatMessagePacket::kId) {
        const std::optional<play::ChatMessagePacket> packet = play::ChatMessagePacket::read(body);
        if (!packet.has_value()) {
            return true;
        }
        const std::optional<std::vector<chat::MessageSignature>> seen = lastSeen(packet->lastSeen);
        if (!seen.has_value() || !tryHandleChat(packet->message, false)) {
            return true;
        }
        const std::int64_t now = chat::nowMillis();
        std::expected<chat::PlayerChatMessage, std::string> message = secureChat_->unpack(
            translatedClient() ? std::nullopt : packet->signature,
            chat::SignedBody{
                .content = packet->message, .timestamp = packet->timestamp, .salt = packet->salt, .lastSeen = *seen},
            now);
        if (!message.has_value()) {
            decodeFailed(message.error());
            return true;
        }
        if (message->hasSignature() && message->hasExpiredServer(now)) {
            logger_.warn([&message] {
                return "Received expired chat: '" + message->body.content +
                       "'. Is the client/server system time unsynchronized?";
            });
        }
        broadcastChat(*message);
        return true;
    }
    if (packetId == play::ChatCommandPacket::kId) {
        const std::optional<play::ChatCommandPacket> packet = play::ChatCommandPacket::read(body);
        if (!packet.has_value() || !tryHandleChat(packet->command, true)) {
            return true;
        }
        // parity: `performUnsignedChatCommand`, which refuses what should have come signed.
        command::CommandDispatcher* dispatcher = options_.commands;
        if (dispatcher != nullptr && secureChat_->enforcesSecureProfile()) {
            const command::PlayerCommandSource probe{*session, dispatcher->environment().groups()};
            if (!dispatcher->signableArguments(probe, packet->command).empty()) {
                logger_.error([session, &packet] {
                    return "Received unsigned command packet from " + session->name() +
                           ", but the command requires signable arguments: " + packet->command;
                });
                session->sendSystemMessage(
                    Component::translatable(std::string{kInvalidCommandSignature}).color(wire::text::kRed));
                detectCommandSpam(packet->command);
                return true;
            }
        }
        runCommand(packet->command);
        detectCommandSpam(packet->command);
        return true;
    }
    if (packetId == play::SignedChatCommandPacket::kId) {
        const std::optional<play::SignedChatCommandPacket> packet = play::SignedChatCommandPacket::read(body);
        if (!packet.has_value()) {
            return true;
        }
        const std::optional<std::vector<chat::MessageSignature>> seen = lastSeen(packet->lastSeen);
        if (!seen.has_value() || !tryHandleChat(packet->command, true)) {
            return true;
        }
        runSignedCommand(*packet, *seen);
        detectCommandSpam(packet->command);
        return true;
    }
    if (packetId == play::ChatAckPacket::kId) {
        const std::optional<play::ChatAckPacket> packet = play::ChatAckPacket::read(body);
        if (packet.has_value() && !translatedClient()) {
            if (const std::expected<void, std::string> moved = secureChat_->applyAck(packet->offset);
                !moved.has_value()) {
                logger_.error([session, &moved] {
                    return "Failed to validate message acknowledgement offset from " + session->name() + ": " +
                           moved.error();
                });
                disconnect(Component::translatable("multiplayer.disconnect.chat_validation_failed"));
            }
        }
        return true;
    }
    const std::optional<play::ChatSessionUpdatePacket> packet = play::ChatSessionUpdatePacket::read(body);
    if (!packet.has_value() || translatedClient()) {
        return true;
    }
    const chat::SecureChat::SessionResult result = secureChat_->updateSession(packet->session);
    if (result == chat::SecureChat::SessionResult::Ignored) {
        logger_.warn([session] {
            return "Ignoring chat session from " + session->name() + " due to missing Services public key";
        });
    } else if (const std::optional<std::string> reason = chat::disconnectKeyOf(result); reason.has_value()) {
        logger_.error([] { return std::string{"Failed to validate profile key"}; });
        disconnect(Component::translatable(*reason));
    } else if (result == chat::SecureChat::SessionResult::Accepted) {
        // parity: `resetPlayerChatState`, which tells everyone the new session.
        session->setChatSession(packet->session);
        const play::PlayerInfoUpdatePacket update =
            play::PlayerInfoUpdatePacket::chatSession(session->uuid(), packet->session);
        if (options_.playerRegistry != nullptr) {
            for (const PlayerSession* other : options_.playerRegistry->snapshot()) {
                if (other->connection().sendPlayPacket) {
                    other->connection().sendPlayPacket(
                        play::PlayerInfoUpdatePacket::kId,
                        [&update](wire::io::ByteBuffer& buffer) { update.write(buffer); });
                }
            }
        }
    }
    return true;
}

void ClientSession::runSignedCommand(const play::SignedChatCommandPacket& packet,
                                     const std::vector<chat::MessageSignature>& lastSeen) {
    PlayerSession* session = playerSession_.get();
    command::CommandDispatcher* dispatcher = options_.commands;
    if (session == nullptr || dispatcher == nullptr) {
        return;
    }
    const command::PlayerCommandSource probe{*session, dispatcher->environment().groups()};
    const std::vector<std::pair<std::string, std::string>> expected =
        dispatcher->signableArguments(probe, packet.command);
    const std::int64_t now = chat::nowMillis();
    const auto failed = [this, session](const std::string& key) {
        logger_.warn([this, session, &key] {
            return "Failed to update secure chat state for " + session->name() + ": '" +
                   wire::text::resolvePlainText(Component::translatable(key), consoleLang()) + "'";
        });
        session->sendSystemMessage(Component::translatable(key).color(wire::text::kRed));
    };
    const auto mismatch = [this, &packet, &expected] {
        std::vector<std::string> clientNames;
        for (const play::ArgumentSignature& argument : packet.argumentSignatures) {
            clientNames.push_back(argument.name);
        }
        std::vector<std::string> expectedNames;
        for (const auto& [name, value] : expected) {
            expectedNames.push_back(name);
        }
        logger_.error([&] {
            return "Signed command mismatch between server and client ('" + packet.command + "'): got [" +
                   joinNames(clientNames) + "] from client, but expected [" + joinNames(expectedNames) + "]";
        });
    };
    chat::SignedArguments arguments;
    if (packet.argumentSignatures.empty() || translatedClient()) {
        // parity: `collectUnsignedArguments`, each argument an unsigned body of its own.
        for (const auto& [name, value] : expected) {
            std::expected<chat::PlayerChatMessage, std::string> message =
                secureChat_->unpack(std::nullopt, chat::SignedBody{.content = value, .timestamp = now}, now);
            if (!message.has_value()) {
                failed(message.error());
                return;
            }
            arguments.emplace(name, *std::move(message));
        }
    } else {
        for (const play::ArgumentSignature& argument : packet.argumentSignatures) {
            const auto found =
                std::ranges::find_if(expected, [&argument](const auto& entry) { return entry.first == argument.name; });
            if (found == expected.end()) {
                secureChat_->breakChain();
                mismatch();
                failed(std::string{kInvalidCommandSignature});
                return;
            }
            std::expected<chat::PlayerChatMessage, std::string> message = secureChat_->unpack(
                argument.signature,
                chat::SignedBody{
                    .content = found->second, .timestamp = packet.timestamp, .salt = packet.salt, .lastSeen = lastSeen},
                now);
            if (!message.has_value()) {
                failed(message.error());
                return;
            }
            arguments.insert_or_assign(found->first, *std::move(message));
        }
        for (const auto& [name, value] : expected) {
            if (!arguments.contains(name)) {
                mismatch();
                failed(std::string{kInvalidCommandSignature});
                return;
            }
        }
    }
    runCommand(packet.command, &arguments);
}

void ClientSession::broadcastChat(const chat::PlayerChatMessage& message) {
    PlayerSession* session = playerSession_.get();
    if (session == nullptr) {
        return;
    }
    std::string spoken = message.body.content;
    if (options_.plugins.chatAllowed && !options_.plugins.chatAllowed(*session, spoken)) {
        return;
    }
    const chat::PlayerChatMessage decorated = message.withUnsignedContent(Component::text(spoken));
    if (options_.playerRegistry != nullptr) {
        const scoreboard::Teams* teams =
            options_.commands == nullptr ? nullptr : options_.commands->environment().options().teams;
        chat::broadcastChatMessage(
            options_.playerRegistry->snapshot(), decorated,
            chat::BoundChatType{.type = chat::chatType::kChat, .name = chat::displayName(*session, teams)}, logger_,
            consoleLang());
    }
    detectSpam(chatSpam_);
}

void ClientSession::deliverChat(const chat::PlayerChatMessage& message, const chat::BoundChatType& chatType) {
    if (translatedClient() || secureChat_ == nullptr) {
        const Component line = chatType.decorate(message.decoratedContent());
        sendPlayPacket(play::SystemChatPacket::kId, [&line](wire::io::ByteBuffer& buffer) {
            play::SystemChatPacket{.content = line, .isActionBar = false}.write(buffer);
        });
        return;
    }
    // parity: `OutgoingChatMessage.create`.
    if (message.isSystem()) {
        const play::DisguisedChatPacket packet{.message = message.decoratedContent(), .chatType = chatType.toWire()};
        sendPlayPacket(play::DisguisedChatPacket::kId,
                       [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
        return;
    }
    const bool withinLimit = secureChat_->send(message, chatType, [this](const play::PlayerChatPacket& packet) {
        sendPlayPacket(play::PlayerChatPacket::kId, [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
    });
    if (!withinLimit) {
        disconnect(Component::translatable("multiplayer.disconnect.too_many_pending_chats"));
    }
}

void ClientSession::detectCommandSpam(const std::string& command) {
    if (options_.limits != nullptr && !options_.limits->spamExcludedCommands.empty()) {
        const std::string name = wire::common::placeholder::lowercaseAscii(command.substr(0, command.find(' ')));
        if (std::ranges::find(options_.limits->spamExcludedCommands, name) !=
            options_.limits->spamExcludedCommands.end()) {
            return;
        }
    }
    detectSpam(commandSpam_);
}

void ClientSession::detectSpam(chat::ChatThrottle& throttle) {
    throttle.increment();
    PlayerSession* session = playerSession_.get();
    if (throttle.underThreshold() || session == nullptr) {
        return;
    }
    const bool op = options_.commands != nullptr &&
                    options_.commands->environment().groups().resolve(session->group, command::kAdminPermission);
    if (!op) {
        disconnect(messageFor("spam", Component::translatable("disconnect.spam")));
    }
}

// parity: `ServerGamePacketListenerImpl.tick`, the idle kick.
void ClientSession::kickIfIdle() {
    const PlayerSession* session = playerSession_.get();
    if (session == nullptr || !options_.idleTimeoutMinutes || closeWhenDrained_.load()) {
        return;
    }
    const std::int32_t minutes = options_.idleTimeoutMinutes();
    const std::int64_t last = session->extras.lastActionMillis;
    if (minutes > 0 && last > 0 && sessionPlay::steadyMillis() - last > static_cast<std::int64_t>(minutes) * 60'000) {
        disconnect(messageFor("idle", Component::translatable("multiplayer.disconnect.idling")));
    }
}

Component ClientSession::messageFor(std::string_view key, Component vanilla, std::string_view reason) const {
    const std::string* custom = options_.messages == nullptr ? nullptr : options_.messages->find(key);
    if (custom == nullptr) {
        return vanilla;
    }
    std::string text = *custom;
    for (std::size_t at = text.find("%reason%"); at != std::string::npos; at = text.find("%reason%", at)) {
        text.replace(at, std::string_view{"%reason%"}.size(), reason);
        at += reason.size();
    }
    return wire::text::miniMessage::parse(text);
}

} // namespace basalt::server::runtime
