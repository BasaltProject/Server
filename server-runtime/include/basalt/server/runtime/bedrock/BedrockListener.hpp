#pragma once

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

/// The Bedrock version this build speaks, which is what a client reads in the server list next to
/// the protocol number.
inline constexpr std::string_view kBedrockGameVersion = "1.26.45";

/// What an operator writes under `[bedrock]` in `config/compatibility.toml`.
struct BedrockOptions {
    bool enabled = true;
    std::uint16_t port = 19132;

    /// What goes in front of a Bedrock player's name so two people called Steve are two people.
    /// Empty is a deliberate choice for a server where nobody can collide.
    std::string prefix = ".";

    /// Which protocols may in, and what happens to one newer than this build knows.
    wire::bedrock::BedrockGate gate;
};

/// Where a message waits until the Bedrock thread can send it. The server writes to a player from
/// the tick thread, another connection or a command, and none of those may touch a RakNet session,
/// which belongs to the listener's thread. Both sides hold it, so neither has to outlive the other.
class BedrockMailbox {
public:
    /// One packet, already written. What it is does not matter here: whoever leaves it knows the
    /// protocol, and this only has to get it to the thread that owns the socket.
    struct Item {
        std::uint16_t packetId = 0;
        std::vector<std::uint8_t> body;
        /// The last thing this client is told. What is left after it is dropped: the session is
        /// about to be gone, and a line nobody can read is not worth the trip.
        bool last = false;
    };

    /// Leaves a packet for the client. Ignored once the session is gone, which is what makes this
    /// safe to hold after the player has left.
    void post(std::uint16_t packetId, std::vector<std::uint8_t> body, bool last = false);

    /// Takes everything waiting. Called only on the listener's thread.
    [[nodiscard]] std::vector<Item> drain();

    void close();

private:
    std::mutex lock_;
    std::vector<Item> items_;
    bool open_ = true;
};

/// The Bedrock door: one UDP socket, one thread, and a session per client.
///
/// It is a second front door and not a second server: what comes through it becomes the same kind
/// of player the Java door makes, so nothing above this knows which edition anybody arrived on.
class BedrockListener {
public:
    /// What a Bedrock client reads in its server list, which is the same answer a Java client gets
    /// to its ping.
    struct Status {
        std::string motd;
        std::int32_t online = 0;
        std::int32_t max = 0;
    };

    /// A client that finished the login, as the server needs to hear about it.
    struct Joining {
        /// Already prefixed, which is the name the rest of the server knows this player by.
        std::string name;
        std::string gamertag;
        std::string xuid;
        wire::common::Uuid uuid;
        std::string address;
        bool authenticated = false;
        /// Which Bedrock protocol this client speaks, because several packets are shaped one way
        /// for the newest and another for everything below it.
        int protocol = 0;
        /// Where to leave what this player is told.
        std::shared_ptr<BedrockMailbox> mailbox;
    };

    /// What the server hands back once it has made a player out of a session. Destroying it is that
    /// player leaving, so the listener holds it for exactly as long as the client is there.
    class Player {
    public:
        Player(const Player&) = delete;
        Player& operator=(const Player&) = delete;
        virtual ~Player() = default;

        /// What the client typed, on its way to wherever chat goes.
        virtual void chat(std::string_view message) = 0;

        /// Where the client says it is now, in the server's own terms: feet, not eyes.
        virtual void move(double x, double y, double z, float yaw, float pitch) = 0;

        /// What the client did to a block: its own number, where, and which face.
        virtual void blockAction(std::int32_t action, std::int32_t x, std::int32_t y, std::int32_t z,
                                 std::int32_t face) = 0;

        /// A click on a block with whatever is in hand: which block, which face, where on it, and
        /// which slot of the hotbar the client believes it is holding.
        virtual void useItemOn(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t face, float clickX,
                               float clickY, float clickZ, std::int32_t hotbarSlot) = 0;

        /// Stacks the client asked to be moved between its own windows.
        virtual void stackRequest(const wire::bedrock::BedrockStackRequest& request) = 0;

        /// Whether the client is crouching, which decides whether a click uses a block or places
        /// one against it.
        virtual void setSneaking(bool sneaking) = 0;

        /// The client closed a window, by the number this server gave it.
        virtual void closeWindow(std::int32_t containerId) = 0;

        /// What the client chose in a form, or nothing when it dismissed one.
        virtual void formAnswer(std::uint32_t formId, const std::optional<std::string>& data) = 0;

        /// A command the client typed, without its slash.
        virtual void command(std::string_view line) = 0;

        /// The player pressed the button on the death screen.
        virtual void respawn() = 0;

        /// A turn of the listener's own loop, which is where anything this player has to be told
        /// about the rest of the server gets worked out. Called from the listener's thread.
        virtual void tick() {}

    protected:
        Player() = default;
    };

    /// What the listener needs of the server, without needing the server.
    struct Hooks {
        std::function<Status()> status;
        /// Makes a player out of a client that got in. Unset, or answering nothing, leaves the door
        /// open and the client alone: it sees a world and nobody sees it.
        std::function<std::unique_ptr<Player>(const Joining&)> join;
    };

    BedrockListener(BedrockOptions options, Hooks hooks, std::string gameVersion,
                    wire::common::Logger logger = wire::common::Logger{"Bedrock"});
    BedrockListener(const BedrockListener&) = delete;
    BedrockListener& operator=(const BedrockListener&) = delete;
    ~BedrockListener();

    /// Binds and starts the thread. False when the port was taken, which is said in the log and is
    /// never fatal: a server without its Bedrock door is still a server.
    [[nodiscard]] bool start();

    void stop();

    [[nodiscard]] std::uint16_t port() const noexcept;

    /// The half of the startup line that is this door's: where it listens and what it speaks. The
    /// server says it all in one line rather than each door announcing itself.
    [[nodiscard]] std::string welcome() const;

    [[nodiscard]] std::size_t sessionCount() const;

    /// The name this server knows a Bedrock player by: the prefix and what Xbox Live calls them.
    [[nodiscard]] std::string prefixedName(std::string_view gamertag) const;

    /// A Bedrock player's uuid, derived from the xuid rather than the name so it survives a rename,
    /// and in a space of its own so it can never land on a Mojang account or an offline player.
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
    std::jthread thread_;
};

} // namespace basalt::server::runtime::bedrock
