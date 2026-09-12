#include <basalt/server/runtime/bedrock/BedrockPlayer.hpp>
#include <basalt/wire/text/Serializers.hpp>
#include <basalt/wire/text/TranslationResolver.hpp>

#include <utility>

namespace basalt::server::runtime::bedrock {
namespace {

using wire::text::Component;

} // namespace

BedrockPlayer::BedrockPlayer(const BedrockListener::Joining& joining, Context context)
    : context_(std::move(context)), mailbox_(joining.mailbox) {
    player::PlayerConnection connection;
    // Bedrock has its own numbering, and nothing may read this as a Java protocol version. Zero is
    // what the rest of the server already treats as "no version was announced".
    connection.protocolVersion = 0;
    connection.connectedHost = joining.address;
    connection.address = joining.address;
    // The only thing this connection can do so far. Everything else is unset, and every caller
    // already checks before it sends: a Bedrock player is simply told nothing it cannot be told.
    connection.sendSystemMessage = [mailbox = joining.mailbox, lang = context_.lang](const Component& message) {
        mailbox->post(wire::text::resolvePlainText(message, lang));
    };

    const std::int32_t entityId = context_.registry != nullptr ? context_.registry->allocateEntityId() : 1;
    session_ = std::make_unique<player::PlayerSession>(joining.uuid, joining.name, entityId, std::move(connection));
    session_->x = context_.spawnX;
    session_->y = context_.spawnY;
    session_->z = context_.spawnZ;
    session_->worldName = context_.worldName;

    if (context_.registry != nullptr) {
        context_.registry->add(session_.get());
    }
    announce(Component::translatable("multiplayer.player.joined", {Component::text(session_->name())})
                 .color(wire::text::kYellow));
}

BedrockPlayer::~BedrockPlayer() {
    if (session_ == nullptr) {
        return;
    }
    if (context_.registry != nullptr) {
        context_.registry->remove(session_->uuid());
    }
    announce(Component::translatable("multiplayer.player.left", {Component::text(session_->name())})
                 .color(wire::text::kYellow));
}

void BedrockPlayer::chat(const std::string_view message) {
    if (session_ == nullptr || message.empty()) {
        return;
    }
    // The same shape the Java side logs and sends, so a line reads the same wherever it is read.
    const Component text = Component::text("<" + session_->name() + "> " + std::string{message});
    context_.logger.info([this, &text] { return wire::text::resolvePlainText(text, context_.lang); });
    announce(text);
}

void BedrockPlayer::move(const double x, const double y, const double z, const float yaw, const float pitch) {
    if (session_ == nullptr) {
        return;
    }
    // A client sends this every tick, so only the first one is worth a line: it says the movement
    // arrived and was read as a position, which is the part that can be wrong.
    if (!moved_) {
        moved_ = true;
        context_.logger.debug([this, x, y, z] {
            return session_->name() + " is moving: " + std::to_string(x) + ", " + std::to_string(y) + ", " +
                   std::to_string(z);
        });
    }
    // Written from the listener's thread, read by the tick, which is exactly what a Java connection
    // does with the same fields from its own thread.
    session_->x = x;
    session_->y = y;
    session_->z = z;
    session_->yaw = yaw;
    session_->pitch = pitch;
}

void BedrockPlayer::announce(const Component& message) const {
    if (context_.registry == nullptr) {
        return;
    }
    for (const player::PlayerSession* player : context_.registry->snapshot()) {
        if (player->connection().sendSystemMessage) {
            player->connection().sendSystemMessage(message);
        }
    }
}

} // namespace basalt::server::runtime::bedrock
