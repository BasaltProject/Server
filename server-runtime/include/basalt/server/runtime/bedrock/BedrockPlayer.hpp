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
#include <unordered_set>
#include <utility>
#include <vector>

namespace basalt::server::runtime::world {
class LoadedWorld;
}

namespace basalt::server::runtime::world::dialogs {
struct Dialog;
}

namespace basalt::server::runtime::bedrock {

struct Seen {
    std::int32_t entityId = 0;
    std::string type;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    float yaw = 0.0F;
    float pitch = 0.0F;
    float width = 0.6F;
    float height = 1.8F;
    std::optional<storage::StoredItemStack> stack;
    std::uint64_t flags = 0;
    std::string name;
    bool alwaysShowName = false;
    wire::bedrock::BedrockLook look;
    std::string motive;
    std::int32_t facing = 0;

    struct Frame {
        bool glowing = false;
        std::int32_t face = 0;
        std::int32_t x = 0;
        std::int32_t y = 0;
        std::int32_t z = 0;
        std::string item;
        std::int32_t count = 0;
        std::int32_t rotation = 0;

        friend bool operator==(const Frame&, const Frame&) = default;
    };

    std::optional<Frame> frame;
};

class BedrockPlayer final : public BedrockListener::Player {
public:
    struct Context {
        player::PlayerRegistry* registry = nullptr;
        std::function<void(const player::PlayerSession&)> joined;
        std::function<void(const player::PlayerSession&)> quit;

        std::function<void(const player::PlayerSession&, std::string, std::function<void(bool, std::string)>)>
            chatFilter;

        std::function<void(const player::PlayerSession&, std::string)> runCommand;

        std::function<void(std::vector<std::pair<std::int32_t, std::int32_t>> columns, int protocol,
                           std::function<void(std::vector<std::uint8_t>)> send, std::function<void()> done)>
            sendChunks;

        std::function<std::optional<storage::StoredPlayer>(const wire::common::Uuid&)> stored;

        std::function<void(const storage::StoredPlayer&)> save;

        struct Arrival {
            wire::bedrock::BedrockDimension dimension = wire::bedrock::BedrockDimension::Overworld;
            std::string worldName;
        };

        std::function<std::optional<Arrival>(player::PlayerSession&, const world::LoadedWorld&, double, double, double)>
            changeWorld;

        std::function<void(player::PlayerSession&, std::string_view)> killed;

        std::function<void(player::PlayerSession&, float amount, std::string_view deathMessageId,
                           const wire::common::Identifier& damageType,
                           const std::optional<entity::DamageSource>& source)>
            hurt;
        std::function<void(player::PlayerSession&, float amount)> heal;

        std::function<void(player::PlayerSession&, std::int32_t x, std::int32_t y, std::int32_t z)> breakBlock;
        std::function<config::UiConfigs()> uiConfigs;
        const wire::common::placeholder::PlaceholderRegistry* placeholders = nullptr;

        std::function<void(const player::PlayerSession&, std::function<void(std::vector<Seen>)>)> lookAround;
        std::function<void(player::PlayerSession&, std::int32_t entityId, bool falling)> attackEntity;

        std::function<void(player::PlayerSession&, std::int32_t entityId)> useEntity;

        std::function<std::optional<OpenTrades>(player::PlayerSession&, std::int32_t entityId)> trades;

        std::function<void(player::PlayerSession&, std::int32_t x, std::int32_t y, std::int32_t z,
                           const std::string& text)>
            writeSign;

        std::function<void(player::PlayerSession&, std::int32_t slot, world::books::PageEdit what, std::int32_t page,
                           std::int32_t other, const std::string& text)>
            writeBookPage;
        std::function<void(player::PlayerSession&, std::int32_t slot, const std::string& title)> signBook;

        std::function<void(player::PlayerSession&, std::int64_t mapId)> mapInfo;

        std::function<void(player::PlayerSession&)> swing;

        std::function<void(player::PlayerSession&, std::int32_t x, std::int32_t y, std::int32_t z,
                           std::int32_t hotbarSlot)>
            pickBlock;

        std::function<void(player::PlayerSession&, std::int32_t entityId)> pickEntity;

        std::function<void(player::PlayerSession&, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t page)>
            turnLecternPage;

        std::function<void(player::PlayerSession&, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t slot,
                           bool disabled)>
            toggleCrafterSlot;

        std::function<void(player::PlayerSession&, const std::string& emoteId, std::int32_t duration,
                           std::uint8_t flags)>
            emote;

        std::function<void(player::PlayerSession&, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t mode,
                           const std::string& command, bool trackOutput, bool conditional, bool needsRedstone)>
            programCommandBlock;

        std::function<void(player::PlayerSession&)> wakeUp;

        std::function<void(std::string worldName, std::int32_t x, std::int32_t y, std::int32_t z)> freeBed;

        std::function<void(std::string worldName, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId,
                           std::vector<world::BlockEffect> effects)>
            blockEffects;

        std::function<void(player::PlayerSession&, std::int32_t slot)> useInAir;
        std::function<void(player::PlayerSession&, std::int32_t slot, std::int32_t drawnTicks)> releaseItem;

        std::function<void(player::PlayerSession&, std::int32_t slot)> eat;

        std::function<void(player::PlayerSession&)> pickupItems;
        std::function<void(player::PlayerSession&, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t ticks,
                           std::function<void(float perTick)> answer)>
            digSpeed;

        std::function<void(player::PlayerSession&, std::int32_t x, std::int32_t y, std::int32_t z)> digStopped;

        std::function<void(
            player::PlayerSession&, const player::BlockHit&, std::int32_t hotbarSlot, std::shared_ptr<BedrockWindows>,
            std::function<void(const OpenWindow&, std::vector<std::optional<storage::StoredItemStack>>)> opened)>
            useOn;

        std::function<void(player::PlayerSession&, std::shared_ptr<BedrockWindows>)> closeWindow;

        std::function<void(player::PlayerSession&, wire::bedrock::BedrockStackRequest, std::shared_ptr<BedrockWindows>,
                           std::function<void(bool, std::vector<wire::bedrock::BedrockResponseContainer>)>)>
            moveStacks;

        std::function<std::vector<std::pair<std::string, std::string>>(const player::PlayerSession&)> commandList;
        std::string worldName;
        double spawnX = 0.0;
        double spawnY = 64.0;
        double spawnZ = 0.0;
        std::int32_t worldTime = 0;
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

    void setFlying(bool flying) override;

    void attack(std::uint64_t runtimeEntityId) override;
    void useOnEntity(std::uint64_t runtimeEntityId) override;
    void setHeldSlot(std::int32_t slot) override;

    void finishEating(std::int32_t hotbarSlot) override;

    void openTrades(std::uint64_t runtimeEntityId, OpenTrades offers);

    void sendTrades();

    void sendEnchantOptions();

    void editSign(std::int32_t x, std::int32_t y, std::int32_t z, const std::string& text) override;

    void showBorderColliders(double minX, double maxX, double minZ, double maxZ, std::int32_t warningBlocks);

    void editBookPage(std::int32_t slot, world::books::PageEdit what, std::int32_t page, std::int32_t other,
                      const std::string& text) override;

    void signBook(std::int32_t slot, const std::string& title) override;

    void requestMap(std::int64_t mapId) override;

    void swing() override;

    void pickBlock(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t hotbarSlot) override;

    void pickEntity(std::uint64_t runtimeEntityId) override;

    void turnLecternPage(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t page) override;

    void toggleCrafterSlot(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t slot, bool disabled) override;

    void emote(const std::string& emoteId, std::int32_t duration, std::uint8_t flags) override;

    void showEmote(std::int32_t entityId, const std::string& emoteId, std::int32_t duration, std::uint8_t flags);

    void programCommandBlock(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t mode,
                             const std::string& command, bool trackOutput, bool conditional,
                             bool needsRedstone) override;

    void useItemInAir(std::int32_t hotbarSlot) override;

    void releaseItem(std::int32_t hotbarSlot) override;
    void openOwnInventory() override;
    void closeWindow(std::int32_t containerId) override;

    void tick() override;

    void respawn() override;

    void leaveBed() override;

    void settled(bool afterDeath) override;

private:
    struct Other {
        std::int32_t entityId = 0;
        double x = 0.0;
        double y = 0.0;
        double z = 0.0;
        float yaw = 0.0F;
        float pitch = 0.0F;
        std::string held;
        std::string worn;
        std::uint64_t flags = 0;
    };

    void syncOthers();

    void syncSurroundings();

    void tickCamera();

    void drawSidebar(const std::string& title, const std::vector<std::string>& lines);

    void clearSidebar();

    void sendAbilities();

    bool sendColumnsAround(std::function<void()> done);

    void say(std::string message);

    void setCamera(std::int32_t entityId);

    void showDialog(const world::dialogs::Dialog& dialog);

    void showMenu(const std::string& title, const std::map<std::int32_t, server::plugins::PluginItem>& items,
                  std::function<void(std::int32_t)> onClick);

    void announce(const wire::text::Component& message) const;

    void announceAndSay(const wire::text::Component& message) const;

    struct OpenForm {
        std::uint32_t id = 0;
        std::vector<std::int32_t> slots;
        std::function<void(std::int32_t)> onClick;
        std::function<void(const std::string&)> onFields;
    };

    void sendColumn(std::int32_t chunkX, std::int32_t chunkZ);

    [[nodiscard]] player::PlayerConnection buildConnection(const BedrockListener::Joining& joining);

    static void drawWindow(const std::shared_ptr<BedrockMailbox>& mailbox, int protocol, const OpenWindow& window,
                           const std::vector<std::optional<storage::StoredItemStack>>& slots);

    [[nodiscard]] wire::bedrock::BedrockItemStack stackFor(const std::optional<storage::StoredItemStack>& stack) const;

    void sendWindow(std::uint32_t containerId, std::span<const wire::bedrock::BedrockItemStack> items,
                    wire::bedrock::BedrockContainerSlot slot);

    void sendEffect(const wire::common::Identifier& effect, wire::bedrock::BedrockEffectEvent event,
                    std::int32_t durationTicks, std::int32_t amplifier, bool ambient, bool particles);

    Context context_;
    std::shared_ptr<BedrockMailbox> mailbox_;

    bool borderColliderX_ = false;
    bool borderColliderZ_ = false;

    std::optional<std::array<std::int32_t, 3>> diggingAt_;
    std::int32_t diggingTicks_ = 0;
    int protocol_ = 0;
    std::unique_ptr<player::PlayerSession> session_;
    std::shared_ptr<BedrockWindows> windows_ = std::make_shared<BedrockWindows>();
    bool moved_ = false;
    /// Guards form ids between the thread that opens a menu and the one its answer arrives on.
    std::mutex formLock_;
    std::uint32_t nextFormId_ = 1;
    std::optional<OpenForm> form_;
    std::string openTitle_;
    wire::bedrock::BedrockDimension dimension_ = wire::bedrock::BedrockDimension::Overworld;
    std::unordered_map<wire::common::Uuid, Other> others_;
    std::unordered_set<wire::common::Uuid> waypoints_;

    std::int32_t spectating_ = 0;
    bool clearCamera_ = false;

    /// The last look the world took, and the lock it crosses threads behind.
    struct Surroundings {
        std::mutex lock;
        std::vector<Seen> latest;
        bool fresh = false;
    };

    std::unique_ptr<ui::PlayerUi> ui_;
    std::int64_t uiTick_ = 0;
    bool bossBarShown_ = false;
    double fallDistance_ = 0.0;
    std::set<std::int64_t> shownBars_;
    std::vector<std::string> sidebarShown_;
    std::optional<std::string> sidebarTitle_;

    std::optional<std::chrono::steady_clock::time_point> drawingSince_;

    std::uint64_t tradingWith_ = 0;

    std::shared_ptr<BedrockListener::Skins> skins_;

    std::shared_ptr<Surroundings> surroundings_ = std::make_shared<Surroundings>();
    /// The door's thread owns it.
    std::unordered_map<std::int32_t, Seen> shown_;
    std::set<std::pair<std::int32_t, std::int32_t>> columns_;
    std::optional<std::pair<std::int32_t, std::int32_t>> lastColumn_;
    std::int64_t firstSeenEpochSeconds_ = 0;
    std::int64_t playTimeTicks_ = 0;
    std::chrono::steady_clock::time_point lastSync_{};
};

} // namespace basalt::server::runtime::bedrock
