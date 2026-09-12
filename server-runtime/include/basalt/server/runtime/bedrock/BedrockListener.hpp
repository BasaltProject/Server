#pragma once

#include <basalt/wire/bedrock/BedrockConnection.hpp>
#include <basalt/wire/bedrock/RakServer.hpp>
#include <basalt/wire/common/Uuid.hpp>
#include <basalt/wire/common/logging/Logger.hpp>

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
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

    /// What the listener needs of the server, without needing the server.
    struct Hooks {
        std::function<Status()> status;
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
