#pragma once

#include <basalt/server/runtime/bedrock/BedrockListener.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/wire/common/logging/Logger.hpp>
#include <basalt/wire/text/LangTable.hpp>

#include <memory>
#include <string>

namespace basalt::server::runtime::bedrock {

/// A player who arrived through the Bedrock door: the same `PlayerSession` in the same registry, so
/// anything asking who is online finds them. Only how text reaches them differs, which lives in the
/// mailbox. The world does not reach them yet: no chunk streaming, no inventory, no movement.
class BedrockPlayer final : public BedrockListener::Player {
public:
    struct Context {
        player::PlayerRegistry* registry = nullptr;
        std::string worldName;
        double spawnX = 0.0;
        double spawnY = 64.0;
        double spawnZ = 0.0;
        wire::text::LangTable lang;
        wire::common::Logger logger{"Bedrock"};
    };

    BedrockPlayer(const BedrockListener::Joining& joining, Context context);
    ~BedrockPlayer() override;

    void chat(std::string_view message) override;

    void move(double x, double y, double z, float yaw, float pitch) override;

private:
    /// A line for everyone on the server, whichever edition they are on.
    void announce(const wire::text::Component& message) const;

    Context context_;
    std::shared_ptr<BedrockMailbox> mailbox_;
    std::unique_ptr<player::PlayerSession> session_;
    bool moved_ = false;
};

} // namespace basalt::server::runtime::bedrock
