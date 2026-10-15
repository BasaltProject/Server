#pragma once

#include <basalt/server/runtime/world/Books.hpp>
#include <basalt/wire/bedrock/BedrockConnection.hpp>
#include <basalt/wire/bedrock/BedrockGate.hpp>
#include <basalt/wire/bedrock/BedrockPackets.hpp>
#include <basalt/wire/bedrock/RakServer.hpp>
#include <basalt/wire/common/Uuid.hpp>
#include <basalt/wire/common/logging/Logger.hpp>

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>

namespace basalt::server::runtime::bedrock {

inline constexpr std::string_view kBedrockGameVersion = "1.26.50";

struct BedrockOptions {
    bool enabled = true;
    std::uint16_t port = 19132;

    std::string prefix = ".";

    wire::bedrock::BedrockGate gate;
};

class BedrockMailbox {
public:
    struct Item {
        std::uint16_t packetId = 0;
        std::vector<std::uint8_t> body;
        bool last = false;
    };

    void post(std::uint16_t packetId, std::vector<std::uint8_t> body, bool last = false);

    [[nodiscard]] std::vector<Item> drain(std::size_t maxBytes);

    void close();

private:
    std::mutex lock_;
    std::vector<Item> items_;
    bool open_ = true;
};

class BedrockListener {
public:
    struct Status {
        std::string motd;
        std::int32_t online = 0;
        std::int32_t max = 0;
    };

    class Skins {
    public:
        void remember(const wire::common::Uuid& who, wire::bedrock::SkinInfo skin);

        void forget(const wire::common::Uuid& who);

        [[nodiscard]] std::optional<wire::bedrock::SkinInfo> of(const wire::common::Uuid& who) const;

    private:
        mutable std::mutex lock_;
        std::unordered_map<wire::common::Uuid, wire::bedrock::SkinInfo> skins_;
    };

    struct Joining {
        std::string name;
        std::string gamertag;
        std::string xuid;
        wire::common::Uuid uuid;
        std::string address;
        bool authenticated = false;
        int protocol = 0;
        std::shared_ptr<BedrockMailbox> mailbox;
        std::shared_ptr<Skins> skins;
    };

    class Player {
    public:
        Player(const Player&) = delete;
        Player& operator=(const Player&) = delete;
        virtual ~Player() = default;

        virtual void chat(std::string_view message) = 0;

        virtual void move(double x, double y, double z, float yaw, float pitch) = 0;

        virtual void blockAction(std::int32_t action, std::int32_t x, std::int32_t y, std::int32_t z,
                                 std::int32_t face) = 0;

        virtual void useItemOn(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t face, float clickX,
                               float clickY, float clickZ, std::int32_t hotbarSlot) = 0;

        virtual void stackRequest(const wire::bedrock::BedrockStackRequest& request) = 0;

        virtual void setSneaking(bool sneaking) = 0;

        virtual void setStance(bool onGround, bool sprinting) = 0;

        virtual void setFlying(bool flying) = 0;

        virtual void useOnEntity(std::uint64_t runtimeEntityId) = 0;

        virtual void attack(std::uint64_t runtimeEntityId) = 0;

        virtual void setHeldSlot(std::int32_t slot) = 0;

        virtual void finishEating(std::int32_t hotbarSlot) = 0;

        virtual void useItemInAir(std::int32_t hotbarSlot) = 0;

        virtual void releaseItem(std::int32_t hotbarSlot) = 0;

        virtual void openOwnInventory() = 0;

        virtual void closeWindow(std::int32_t containerId) = 0;

        virtual void formAnswer(std::uint32_t formId, const std::optional<std::string>& data) = 0;

        virtual void editSign(std::int32_t x, std::int32_t y, std::int32_t z, const std::string& text) = 0;

        virtual void editBookPage(std::int32_t slot, world::books::PageEdit what, std::int32_t page, std::int32_t other,
                                  const std::string& text) = 0;

        virtual void signBook(std::int32_t slot, const std::string& title) = 0;

        virtual void requestMap(std::int64_t mapId) = 0;

        virtual void swing() = 0;

        virtual void pickBlock(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t hotbarSlot) = 0;

        virtual void pickEntity(std::uint64_t runtimeEntityId) = 0;

        virtual void programCommandBlock(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t mode,
                                         const std::string& command, bool trackOutput, bool conditional,
                                         bool needsRedstone) = 0;

        virtual void toggleCrafterSlot(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t slot,
                                       bool disabled) = 0;

        virtual void emote(const std::string& emoteId, std::int32_t duration, std::uint8_t flags) = 0;

        virtual void turnLecternPage(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t page) = 0;

        virtual void command(std::string_view line) = 0;

        virtual void respawn() = 0;

        virtual void leaveBed() = 0;

        virtual void settled(bool afterDeath) = 0;

        virtual void tick() {}

    protected:
        Player() = default;
    };

    struct Spawn {
        double x = 0.0;
        double y = 64.0;
        double z = 0.0;
        float yaw = 0.0F;
        float pitch = 0.0F;
        std::int32_t gamemode = 0;
    };

    struct Hooks {
        std::function<Status()> status;
        std::function<Spawn(const wire::common::Uuid&)> spawn;
        std::function<std::unique_ptr<Player>(const Joining&)> join;

        std::function<std::vector<wire::bedrock::BedrockRecipe>()> recipes;
    };

    BedrockListener(BedrockOptions options, Hooks hooks, std::string gameVersion,
                    wire::common::Logger logger = wire::common::Logger{"Bedrock"});
    BedrockListener(const BedrockListener&) = delete;
    BedrockListener& operator=(const BedrockListener&) = delete;
    ~BedrockListener();

    [[nodiscard]] bool start();

    void closeDoor();

    void stop();

    [[nodiscard]] std::uint16_t port() const noexcept;

    [[nodiscard]] std::string welcome() const;

    [[nodiscard]] std::size_t sessionCount() const;

    [[nodiscard]] std::string prefixedName(std::string_view gamertag) const;

    [[nodiscard]] static wire::common::Uuid uuidOf(std::string_view xuid);

private:
    struct Session;

    void run();
    void onConnect(wire::bedrock::RakSession& session);
    void onMessage(wire::bedrock::RakSession& session, std::span<const std::uint8_t> payload);
    void onDisconnect(wire::bedrock::RakSession& session);
    void onReady(Session& session);
    void deliverMail(Session& session);
    [[nodiscard]] std::string advertisement() const;

    BedrockOptions options_;
    Hooks hooks_;
    std::string gameVersion_;
    wire::common::Logger logger_;

    std::shared_ptr<const wire::bedrock::EcKeyPair> keys_;
    std::unique_ptr<wire::bedrock::RakServer> server_;
    std::unordered_map<wire::io::UdpEndpoint, std::unique_ptr<Session>> sessions_;
    std::atomic<bool> running_{false};
    std::atomic<bool> accepting_{true};
    std::shared_ptr<Skins> skins_ = std::make_shared<Skins>();
    std::jthread thread_;
};

} // namespace basalt::server::runtime::bedrock
