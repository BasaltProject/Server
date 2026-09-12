#pragma once

#include <basalt/wire/bedrock/BedrockConnection.hpp>
#include <basalt/wire/bedrock/BedrockGate.hpp>
#include <basalt/wire/bedrock/RakServer.hpp>
#include <basalt/wire/common/Uuid.hpp>
#include <basalt/wire/common/logging/Logger.hpp>

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>

namespace basalt::server::runtime::bedrock {

/// The Bedrock version this build speaks, which is what a client reads in the server list next to
/// the protocol number.
inline constexpr std::string_view kBedrockGameVersion = "1.26.40";

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

/// Where a message waits until the Bedrock thread can send it.
///
/// The server writes to a player from wherever it happens to be: the tick thread, another player's
/// connection, a command. None of those may touch a RakNet session, which belongs to the listener's
/// own thread, so what they leave here is picked up there. Both sides hold it, so neither has to
/// outlive the other.
class BedrockMailbox {
public:
    /// Leaves a line for the client. Ignored once the session is gone, which is what makes this safe
    /// to hold after the player has left.
    void post(std::string line);

    /// Takes everything waiting. Called only on the listener's thread.
    [[nodiscard]] std::vector<std::string> drain();

    void close();

private:
    std::mutex lock_;
    std::vector<std::string> lines_;
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
