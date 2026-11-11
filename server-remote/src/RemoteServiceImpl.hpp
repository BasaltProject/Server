#pragma once

#include "RemoteJson.hpp"
#include "Transport.hpp"

#include <basalt/server/remote/AttemptTracker.hpp>
#include <basalt/server/remote/Http.hpp>
#include <basalt/server/remote/RemoteService.hpp>
#include <basalt/wire/common/json/Json.hpp>
#include <basalt/wire/io/EventLoop.hpp>
#include <basalt/wire/io/TcpSocket.hpp>

#include <atomic>
#include <deque>
#include <map>
#include <mutex>
#include <thread>

namespace basalt::server::remote::detail {

using Clock = std::chrono::steady_clock;

constexpr std::uint64_t kRconToken = 1;
constexpr std::uint64_t kApiToken = 2;
constexpr std::uint64_t kManagementToken = 3;
constexpr std::uint64_t kFirstConnection = 16;
constexpr std::size_t kMaxRconConnections = 16;
constexpr std::int32_t kRconIdleSeconds = 30;
constexpr std::size_t kMaxQueuedLogLines = 4096;
constexpr std::size_t kMaxQueuedNotifications = 1024;
constexpr std::size_t kMaxWsMessage = 65536;
constexpr std::size_t kMaxOutBytes = 4 * 1024 * 1024;
constexpr std::size_t kMaxCommandChars = 32767;

enum class Kind : std::uint8_t { Rcon, Http, WebSocket };

struct Connection {
    std::uint64_t id = 0;
    Kind kind = Kind::Http;
    std::unique_ptr<Transport> transport;
    std::string peer;
    std::string in;
    std::string out;
    std::string fragments;
    bool authed = false;
    bool busy = false;
    bool closeAfterFlush = false;
    bool writeInterest = false;
    bool management = false;
    bool notified = false;
    Clock::time_point lastReceived;
    Clock::time_point lastPing;
};

struct Completion {
    std::uint64_t connection = 0;
    std::string bytes;
    bool endBusy = false;
    bool close = false;
};

} // namespace basalt::server::remote::detail

namespace basalt::server::remote {

struct RemoteService::Impl : std::enable_shared_from_this<Impl> {
    using Clock = detail::Clock;
    using Kind = detail::Kind;
    using Connection = detail::Connection;
    using Completion = detail::Completion;

    RemoteSettings settings;
    RemoteHost& host;
    wire::common::Logger logger;
    wire::io::EventLoop loop;
    std::optional<wire::io::TcpListener> rconListener;
    std::optional<wire::io::TcpListener> apiListener;
    std::optional<wire::io::TcpListener> managementListener;
    std::unique_ptr<TlsContext> tls;
    std::unique_ptr<TlsContext> managementTls;
    AttemptTracker rconTracker;
    AttemptTracker apiTracker;
    AttemptTracker managementTracker;
    std::map<std::uint64_t, Connection> connections;
    std::uint64_t nextId = detail::kFirstConnection;
    std::jthread thread;
    std::atomic<bool> running{false};
    std::atomic<std::int32_t> subscribers{0};
    std::atomic<std::int32_t> notified{0};
    std::atomic<std::int32_t> heartbeatSeconds{0};
    std::mutex mailbox;
    std::deque<Completion> completions;
    std::deque<std::string> logLines;
    std::deque<std::string> notifications;
    Clock::time_point lastSweep = Clock::now();
    Clock::time_point lastHeartbeat = Clock::now();
    Clock::time_point lastPoll = Clock::now();

    Impl(RemoteSettings s, RemoteHost& h, wire::common::Logger l)
        : settings(std::move(s)), host(h), logger(std::move(l)),
          rconTracker(settings.rcon.maxFailures, settings.rcon.lockoutSeconds),
          apiTracker(settings.api.maxFailures, settings.api.lockoutSeconds),
          managementTracker(settings.management.maxFailures, settings.management.lockoutSeconds) {
        heartbeatSeconds = settings.management.heartbeatSeconds;
    }

    void post(Completion completion) {
        {
            const std::lock_guard lock{mailbox};
            completions.push_back(std::move(completion));
        }
        static_cast<void>(loop.wake());
    }

    [[nodiscard]] std::size_t countOf(Kind a, Kind b, const std::string* peer = nullptr, bool managed = false) const {
        std::size_t count = 0;
        for (const auto& [id, connection] : connections) {
            count += (connection.kind == a || connection.kind == b) && connection.management == managed &&
                     (peer == nullptr || connection.peer == *peer);
        }
        return count;
    }

    void dispatch(std::uint64_t id, bool endBusy, std::string failure, std::function<std::string(RemoteHost&)> work);
    void run(std::stop_token stop);
    void accept(wire::io::TcpListener& listener, std::uint64_t token);
    void close(std::uint64_t id);
    void refreshInterest(Connection& connection);
    void flush(Connection& connection);
    void service(Connection& connection, bool readable, bool writable);
    void drainMailbox();
    void sweep();
    void process(Connection& connection);
    void processRcon(Connection& connection);
    void processHttp(Connection& connection);
    void processWebSocket(Connection& connection);
    void handleRequest(Connection& connection, http::Request& request);
    void upgrade(Connection& connection, const http::Request& request);
    void handleWsMessage(Connection& connection, const std::string& text);
    void handleRpc(Connection& connection, const wire::common::JsonValue& message);
    void handleManagementRequest(Connection& connection, http::Request& request);
    void deliverNotifications(std::deque<std::string>& frames);
    void beat();
    void reply(Connection& connection, int status, std::string_view body, std::string_view extra = {});
    void runCommand(Connection& connection, Origin origin, std::string command,
                    std::function<std::string(CommandResult)> render);
    [[nodiscard]] bool authorised(const http::Request& request, std::string& chosenProtocol) const;
    [[nodiscard]] std::string_view managementCheck(const http::Request& request, std::string& chosenProtocol) const;
    void markNotified(Connection& connection);
};

} // namespace basalt::server::remote
