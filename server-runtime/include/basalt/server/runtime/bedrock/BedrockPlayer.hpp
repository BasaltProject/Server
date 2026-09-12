#pragma once

#include <basalt/server/plugins/PluginWorldAccess.hpp>
#include <basalt/server/runtime/bedrock/BedrockListener.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/WorldClock.hpp>
#include <basalt/server/storage/StoredPlayer.hpp>
#include <basalt/wire/bedrock/BedrockPackets.hpp>
#include <basalt/wire/common/logging/Logger.hpp>
#include <basalt/wire/text/LangTable.hpp>

#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace basalt::server::runtime::world {
class LoadedWorld;
}

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

        /// Builds the columns around a point and hands each one back already written, in the shape
        /// a client reads. Runs where the world runs, not on the door's own thread, which is why it
        /// answers through a callback instead of returning.
        std::function<void(std::vector<std::pair<std::int32_t, std::int32_t>> columns, int protocol,
                           std::function<void(std::vector<std::uint8_t>)> send, std::function<void()> done)>
            sendChunks;

        /// What `players.db` remembers about this player, or nothing when it knows nobody by that
        /// name. Read once, when they arrive.
        std::function<std::optional<storage::StoredPlayer>(const wire::common::Uuid&)> stored;

        /// Writes what this player is now, for the save that runs on a timer and at shutdown.
        std::function<void(const storage::StoredPlayer&)> save;

        /// Moves this player to another world, in the world's own terms: it answers which of
        /// Bedrock's three dimensions to draw and where the player lands, or nothing when the move
        /// did not happen. The world itself is the caller's business; this only draws the arrival.
        struct Arrival {
            wire::bedrock::BedrockDimension dimension = wire::bedrock::BedrockDimension::Overworld;
            std::string worldName;
        };

        std::function<std::optional<Arrival>(player::PlayerSession&, const world::LoadedWorld&, double, double, double)>
            changeWorld;

        /// What the rest of the server is told when this player dies: the same call the Java door
        /// makes, so a plugin never learns which door they came in by.
        std::function<void(player::PlayerSession&, std::string_view)> killed;

        /// Hurts or heals this player, where the world and the plugins are. The arithmetic is the
        /// same on both editions and lives in `player/PlayerDamage`; what happens here is only
        /// getting the call to the thread that owns the answer, exactly as `killed` does.
        std::function<void(player::PlayerSession&, float amount, std::string_view deathMessageId,
                           const wire::common::Identifier& damageType,
                           const std::optional<entity::DamageSource>& source)>
            hurt;
        std::function<void(player::PlayerSession&, float amount)> heal;

        /// Every command this player may use, with the usage line `/help` would print for it.
        /// Bedrock refuses to send what is not in its own list, so a client that is told nothing
        /// can run nothing.
        std::function<std::vector<std::pair<std::string, std::string>>(const player::PlayerSession&)> commandList;
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

    void respawn() override;

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

    /// Asks the world for the columns around this player that it has not sent yet, and says where
    /// to expect them. `done` runs once they are all on their way. False when there is no world to
    /// ask, which is the only reason it would not happen.
    bool sendColumnsAround(std::function<void()> done);

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

    /// One column again, to a client that already has it. What a biome change and a world edit
    /// too big to patch block by block both ask for.
    void sendColumn(std::int32_t chunkX, std::int32_t chunkZ);

    /// The three effect callbacks in one: they differ only in the word they put in the packet.
    void sendEffect(const wire::common::Identifier& effect, wire::bedrock::BedrockEffectEvent event,
                    std::int32_t durationTicks, std::int32_t amplifier, bool ambient, bool particles);

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
    /// Which of Bedrock's three the player is in. Packets that name a dimension go to a world the
    /// receiver may not be in, so they carry it rather than assuming the overworld.
    wire::bedrock::BedrockDimension dimension_ = wire::bedrock::BedrockDimension::Overworld;
    std::unordered_map<wire::common::Uuid, Other> others_;
    /// Which columns this client already has, so walking sends only what is new. A column that
    /// falls out of range is forgotten, which is what makes coming back send it again.
    std::set<std::pair<std::int32_t, std::int32_t>> columns_;
    /// Where the player was when the columns were last worked out. The unset value is deliberate:
    /// it is what makes the first turn after arriving do the work.
    std::optional<std::pair<std::int32_t, std::int32_t>> lastColumn_;
    /// What this player's row said when they arrived, so the one written back keeps the totals
    /// rather than starting them again.
    std::int64_t firstSeenEpochSeconds_ = 0;
    std::int64_t playTimeTicks_ = 0;
    /// When the others were last looked at. The listener turns faster than the world does, and
    /// sending a position more often than it changes is only traffic.
    std::chrono::steady_clock::time_point lastSync_{};
};

} // namespace basalt::server::runtime::bedrock
