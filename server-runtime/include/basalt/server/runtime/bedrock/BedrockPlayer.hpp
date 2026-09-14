#pragma once

#include <basalt/server/plugins/PluginWorldAccess.hpp>
#include <basalt/server/runtime/bedrock/BedrockListener.hpp>
#include <basalt/server/runtime/bedrock/BedrockStackRequests.hpp>
#include <basalt/server/runtime/player/PlayerBlockPlace.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/ui/PlayerUi.hpp>
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

/// One thing in the world that is not a player: a mob, or a stack on the floor. Filled in where the
/// world runs and read on the door's thread, which is why it travels behind a lock.
struct Seen {
    std::int32_t entityId = 0;
    /// What the client draws it as, in Bedrock's own names. Empty for a dropped stack.
    std::string type;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    float yaw = 0.0F;
    float pitch = 0.0F;
    float width = 0.6F;
    float height = 1.8F;
    /// What is lying there, for a stack. Nothing for anything alive.
    std::optional<storage::StoredItemStack> stack;
    /// What it is doing, as the flags that edition draws it by: burning, small, sitting. A mob that
    /// is only ever drawn once stays as it arrived, so a lamb never grows and a fire never shows.
    std::uint64_t flags = 0;
};

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

        /// Takes a block away where the world is, as a player breaking it does. The arithmetic and
        /// everything that hears it are the same on both editions and live in
        /// `player/PlayerBlockBreak`; what happens here is only getting onto the right thread.
        std::function<void(player::PlayerSession&, std::int32_t x, std::int32_t y, std::int32_t z)> breakBlock;
        /// What the files say a sidebar, a tab list and a boss bar should hold. The same renderer
        /// the Java door uses: only where the drawing ends up is different.
        std::function<config::UiConfigs()> uiConfigs;
        const wire::common::placeholder::PlaceholderRegistry* placeholders = nullptr;

        /// What the world holds around this player, taken where the world is and handed back on
        /// that same thread. A Bedrock client is told about mobs and dropped stacks by packet, so a
        /// server that never looks draws an empty world with nothing on the floor.
        std::function<void(const player::PlayerSession&, std::function<void(std::vector<Seen>)>)> lookAround;
        /// Hits whatever that entity is, where the world is. The numbers are the same ones the
        /// other door swings by: the formula lives in `player::combat` so the two cannot drift.
        std::function<void(player::PlayerSession&, std::int32_t entityId, bool falling)> attackEntity;

        /// A click on something alive that is not a hit: sitting in a cart, feeding an animal.
        std::function<void(player::PlayerSession&, std::int32_t entityId)> useEntity;

        /// Gets this player out of the bed they are in, where the world is. Morning asks every
        /// connection for this, and a Bedrock player who is never asked stays asleep for good.
        std::function<void(player::PlayerSession&)> wakeUp;

        /// Puts a bed back down when its sleeper leaves without waking. Takes the square rather
        /// than the player: by the time this is called there is no player left to wake.
        std::function<void(std::string worldName, std::int32_t x, std::int32_t y, std::int32_t z)> freeBed;

        /// What a block that changed asks everyone in this player's world to hear: a door, a note,
        /// a bell. Answered where the world is, because the list of who is there lives with it.
        std::function<void(std::string worldName, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId,
                           std::vector<world::BlockEffect> effects)>
            blockEffects;

        /// A click on nothing with that slot in hand, where the world is: a throw, or the start of
        /// a draw. `drawnTicks` is how long a bow was held, which only the release knows.
        std::function<void(player::PlayerSession&, std::int32_t slot)> useInAir;
        std::function<void(player::PlayerSession&, std::int32_t slot, std::int32_t drawnTicks)> releaseItem;

        /// Swallows what is in that slot, where the world is: the food, the effects and the stack
        /// it comes off. The clock for it belongs to the client on this door.
        std::function<void(player::PlayerSession&, std::int32_t slot)> eat;

        /// Picks up whatever this player is standing on, where the world is. Java does the same
        /// thing once per tick from its own session; a Bedrock player has none, and without this
        /// nothing they break ever reaches their bag.
        std::function<void(player::PlayerSession&)> pickupItems;
        /// How fast this player takes the block at those coordinates apart, answered where the
        /// world is. Bedrock is told the pace rather than deciding it, which is what draws the
        /// cracks: a client that is told nothing digs at a guess of its own and shows no cracks.
        std::function<void(player::PlayerSession&, std::int32_t x, std::int32_t y, std::int32_t z,
                           std::function<void(float perTick)> answer)>
            digSpeed;

        /// Puts down whatever that slot holds, where the world is, unless the block that was
        /// clicked opens a window of its own: then `opened` is called with it and nothing is
        /// placed. The arithmetic of placing is the same on both editions and lives in
        /// `player/PlayerBlockPlace`.
        std::function<void(
            player::PlayerSession&, const player::BlockHit&, std::int32_t hotbarSlot, std::shared_ptr<BedrockWindows>,
            std::function<void(const OpenWindow&, std::vector<std::optional<storage::StoredItemStack>>)> opened)>
            useOn;

        /// Forgets the window this player had open, and puts back whatever they were dragging.
        std::function<void(player::PlayerSession&, std::shared_ptr<BedrockWindows>)> closeWindow;

        /// Moves stacks around this player's own windows, where the world is. The answer goes
        /// back through the callback because it is the client's own request number that has to
        /// come back to it, and by then the call is on another thread.
        std::function<void(player::PlayerSession&, wire::bedrock::BedrockStackRequest, std::shared_ptr<BedrockWindows>,
                           std::function<void(bool, std::vector<wire::bedrock::BedrockResponseContainer>)>)>
            moveStacks;

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

    void blockAction(std::int32_t action, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t face) override;

    void useItemOn(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t face, float clickX, float clickY,
                   float clickZ, std::int32_t hotbarSlot) override;

    void stackRequest(const wire::bedrock::BedrockStackRequest& request) override;

    void setSneaking(bool sneaking) override;
    void setStance(bool onGround, bool sprinting) override;

    void attack(std::uint64_t runtimeEntityId) override;
    void useOnEntity(std::uint64_t runtimeEntityId) override;
    void setHeldSlot(std::int32_t slot) override;

    void finishEating(std::int32_t hotbarSlot) override;

    void useItemInAir(std::int32_t hotbarSlot) override;

    void releaseItem(std::int32_t hotbarSlot) override;
    void openOwnInventory() override;
    void closeWindow(std::int32_t containerId) override;

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
        /// What they are holding, so a sword that changes hands is drawn changing hands.
        std::string held;
        /// And what they are wearing, as the four ids run together: cheaper to compare than four
        /// stacks, and the only thing this needs to know is whether it changed.
        std::string worn;
    };

    /// Draws whoever arrived, moves whoever moved and takes away whoever left.
    void syncOthers();

    /// The same, for what is not a player: mobs and the stacks on the floor.
    void syncSurroundings();

    /// The sidebar, drawn whole: over here a line is a fake player with a number beside it.
    void drawSidebar(const std::string& title, const std::vector<std::string>& lines);

    void clearSidebar();

    /// What this player's hands may do and whether the world will take a change from them. Sent
    /// together because the client reads them as one answer, and again whenever the mode changes.
    void sendAbilities();

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

    /// The same, and a line on the console with it: what an operator watches for rather than what a
    /// player says.
    void announceAndSay(const wire::text::Component& message) const;

    /// The form this player is looking at: which button is which slot, and who to tell.
    struct OpenForm {
        std::uint32_t id = 0;
        std::vector<std::int32_t> slots;
        std::function<void(std::int32_t)> onClick;
    };

    /// One column again, to a client that already has it. What a biome change and a world edit
    /// too big to patch block by block both ask for.
    void sendColumn(std::int32_t chunkX, std::int32_t chunkZ);

    /// Everything this player can be told, built once when they arrive. Its own file, because it
    /// is the whole of the translation into that edition and nothing else.
    [[nodiscard]] player::PlayerConnection buildConnection(const BedrockListener::Joining& joining);

    /// Draws a block's window: the window itself, then everything in it. Static because it runs
    /// where the world runs, and by then the player it was asked for may have left.
    static void drawWindow(const std::shared_ptr<BedrockMailbox>& mailbox, int protocol, const OpenWindow& window,
                           const std::vector<std::optional<storage::StoredItemStack>>& slots);

    /// One stack as the other edition carries it, or air when it has no such item.
    [[nodiscard]] wire::bedrock::BedrockItemStack stackFor(const std::optional<storage::StoredItemStack>& stack) const;

    /// One window's worth of slots, said whole. Bedrock has no "one slot changed" for a window it
    /// was never told about, so the three windows a player has go out together.
    void sendWindow(std::uint32_t containerId, std::span<const wire::bedrock::BedrockItemStack> items,
                    wire::bedrock::BedrockContainerSlot slot);

    /// The three effect callbacks in one: they differ only in the word they put in the packet.
    void sendEffect(const wire::common::Identifier& effect, wire::bedrock::BedrockEffectEvent event,
                    std::int32_t durationTicks, std::int32_t amplifier, bool ambient, bool particles);

    Context context_;
    std::shared_ptr<BedrockMailbox> mailbox_;
    /// Which Bedrock protocol this client speaks: several packets are shaped one way for the
    /// newest and another for everything below it.
    int protocol_ = 0;
    std::unique_ptr<player::PlayerSession> session_;
    /// What this player is dragging and which block they have open. Shared because the world
    /// thread is what moves both, and a call already on its way there outlives the player.
    std::shared_ptr<BedrockWindows> windows_ = std::make_shared<BedrockWindows>();
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

    /// The last look the world took, and the lock it crosses threads behind. Shared rather than
    /// owned: the answer comes back on another thread, and by then this player may be gone.
    struct Surroundings {
        std::mutex lock;
        std::vector<Seen> latest;
        bool fresh = false;
    };

    /// The sidebar and the boss bar, drawn by the same renderer the other door uses. Absent when
    /// this build was handed no files to draw from.
    std::unique_ptr<ui::PlayerUi> ui_;
    std::int64_t uiTick_ = 0;
    bool bossBarShown_ = false;
    /// How far this player has dropped since they last stood on something.
    double fallDistance_ = 0.0;
    /// The bars this client has been shown, by the id each one travels under over here.
    std::set<std::int64_t> shownBars_;
    /// What the sidebar is showing right now, line by line, so only what changed goes out.
    std::vector<std::string> sidebarShown_;
    std::optional<std::string> sidebarTitle_;

    /// When this player started holding an item down, which is the only clock there is for a draw:
    /// the client bends the bow itself and says nothing until it lets go.
    std::optional<std::chrono::steady_clock::time_point> drawingSince_;

    /// Every Bedrock player's skin, so this client can be shown the others as they described
    /// themselves rather than as a flat colour.
    std::shared_ptr<BedrockListener::Skins> skins_;

    std::shared_ptr<Surroundings> surroundings_ = std::make_shared<Surroundings>();
    /// What this client has already been shown, by entity id. The door's thread owns it.
    std::unordered_map<std::int32_t, Seen> shown_;
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
