#pragma once

#include <basalt/server/plugins/PluginWorldAccess.hpp>
#include <basalt/server/runtime/bedrock/BedrockListener.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/WorldClock.hpp>
#include <basalt/wire/common/logging/Logger.hpp>
#include <basalt/wire/text/LangTable.hpp>

#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace basalt::server::runtime::bedrock {

/// A player who arrived through the Bedrock door: the same `PlayerSession` in the same registry, so
/// anything asking who is online finds them. Only how text reaches them differs, which lives in the
/// mailbox. The world does not reach them yet: no chunk streaming, no inventory, no movement.
class BedrockPlayer final : public BedrockListener::Player {
public:
    struct Context {
        player::PlayerRegistry* registry = nullptr;
        /// What the rest of the server is told when this player arrives and when it leaves. The
        /// same calls the Java door makes, so a plugin never learns which door somebody came in by.
        std::function<void(const player::PlayerSession&)> joined;
        std::function<void(const player::PlayerSession&)> quit;

        /// What a plugin gets to say about a line of chat before anybody hears it: it may rewrite
        /// it, or stop it. Runs where the plugins run, so the answer comes back later and the line
        /// goes out from there.
        std::function<void(const player::PlayerSession&, std::string, std::function<void(bool, std::string)>)>
            chatFilter;

        /// Runs a command as this player, where commands are allowed to run.
        std::function<void(const player::PlayerSession&, std::string)> runCommand;
        std::string worldName;
        double spawnX = 0.0;
        double spawnY = 64.0;
        double spawnZ = 0.0;
        std::int32_t worldTime = 0;
        /// What the sky is doing where this player lands, for a client that would otherwise draw
        /// whatever it last saw.
        world::WorldWeather weather;
        wire::text::LangTable lang;
        wire::common::Logger logger{"Bedrock"};
    };

    BedrockPlayer(const BedrockListener::Joining& joining, Context context);
    ~BedrockPlayer() override;

    void chat(std::string_view message) override;

    void move(double x, double y, double z, float yaw, float pitch) override;

    void formAnswer(std::uint32_t formId, const std::optional<std::string>& data) override;

    void command(std::string_view line) override;

    void tick() override;

private:
    /// Everybody else, as this client has been told about them. What the registry says now and
    /// what is in here is the whole of the difference that has to go out.
    struct Other {
        std::int32_t entityId = 0;
        double x = 0.0;
        double y = 0.0;
        double z = 0.0;
        float yaw = 0.0F;
        float pitch = 0.0F;
    };

    /// Draws whoever arrived, moves whoever moved and takes away whoever left.
    void syncOthers();

    /// The line, said to everyone and written to the log, once nobody has objected to it.
    void say(std::string message);

    /// A window of items a plugin drew, as the list of buttons a Bedrock client can actually use.
    ///
    /// A menu is a grid with holes and a form is a list, so what a Bedrock player sees is the items
    /// in slot order and nothing where the holes were. The plugin is told the slot it meant, never
    /// the button number, so it never learns which edition is looking.
    void showMenu(const std::string& title, const std::map<std::int32_t, server::plugins::PluginItem>& items,
                  std::function<void(std::int32_t)> onClick);

    /// A line for everyone on the server, whichever edition they are on.
    void announce(const wire::text::Component& message) const;

    /// The form this player is looking at: which button is which slot, and who to tell.
    struct OpenForm {
        std::uint32_t id = 0;
        std::vector<std::int32_t> slots;
        std::function<void(std::int32_t)> onClick;
    };

    Context context_;
    std::shared_ptr<BedrockMailbox> mailbox_;
    /// Which Bedrock protocol this client speaks: several packets are shaped one way for the
    /// newest and another for everything below it.
    int protocol_ = 0;
    std::unique_ptr<player::PlayerSession> session_;
    bool moved_ = false;
    /// Written by whoever opens a menu and read by the thread the answer arrives on, which are
    /// never the same one.
    std::mutex formLock_;
    std::uint32_t nextFormId_ = 1;
    std::optional<OpenForm> form_;
    std::string openTitle_;
    std::unordered_map<wire::common::Uuid, Other> others_;
    /// When the others were last looked at. The listener turns faster than the world does, and
    /// sending a position more often than it changes is only traffic.
    std::chrono::steady_clock::time_point lastSync_{};
};

} // namespace basalt::server::runtime::bedrock
