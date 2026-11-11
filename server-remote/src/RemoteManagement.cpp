#include "RemoteServiceImpl.hpp"

#include <basalt/server/remote/ManagementRpc.hpp>
#include <basalt/server/remote/WebSocketCodec.hpp>

#include <algorithm>

namespace basalt::server::remote {
namespace {

using detail::Clock;
using detail::Kind;
using remoteJson::constantTimeEquals;
using wire::common::JsonValue;

constexpr std::string_view kSubprotocol = "minecraft-v1";

[[nodiscard]] std::string unauthorizedBody(std::string_view reason) {
    wire::common::JsonObject object;
    object.set("error", JsonValue::of(std::string{"Unauthorized"}));
    object.set("message", JsonValue::of(std::string{reason}));
    return remoteJson::jsonText(object);
}

[[nodiscard]] std::string textFrame(std::string_view payload) {
    std::string frame;
    websocket::encode(frame, websocket::opcode::kText, payload);
    return frame;
}

} // namespace

std::string_view RemoteService::Impl::managementCheck(const http::Request& request, std::string& chosenProtocol) const {
    const std::string& secret = settings.management.secret;
    if (const auto header = request.header("authorization"); header.has_value()) {
        constexpr std::string_view kBearer = "Bearer ";
        if (header->size() > kBearer.size() && header->substr(0, kBearer.size()) == kBearer) {
            std::string_view token = header->substr(kBearer.size());
            while (!token.empty() && token.front() == ' ') {
                token.remove_prefix(1);
            }
            while (!token.empty() && token.back() == ' ') {
                token.remove_suffix(1);
            }
            return !token.empty() && constantTimeEquals(token, secret) ? std::string_view{} : "Invalid API key";
        }
    }
    const auto offered = request.header("sec-websocket-protocol");
    constexpr std::string_view kPrefix = "minecraft-v1,";
    if (!offered.has_value() || !offered->starts_with(kPrefix)) {
        return "Missing API key";
    }
    const auto origin = request.header("origin");
    if (!origin.has_value() || origin->empty() ||
        std::ranges::find(settings.management.allowedOrigins, std::string{*origin}) ==
            settings.management.allowedOrigins.end()) {
        return "Origin Not Allowed";
    }
    std::string_view token = offered->substr(kPrefix.size());
    while (!token.empty() && token.front() == ' ') {
        token.remove_prefix(1);
    }
    while (!token.empty() && token.back() == ' ') {
        token.remove_suffix(1);
    }
    if (token.empty() || !constantTimeEquals(token, secret)) {
        return "Invalid API key";
    }
    chosenProtocol = std::string{kSubprotocol};
    return {};
}

void RemoteService::Impl::handleManagementRequest(Connection& connection, http::Request& request) {
    const Clock::time_point now = Clock::now();
    if (const std::int32_t locked = managementTracker.lockedFor(connection.peer, now); locked > 0) {
        connection.closeAfterFlush = true;
        reply(connection, 429, remoteJson::errorJson("too many failed attempts"),
              "Retry-After: " + std::to_string(locked) + "\r\n");
        return;
    }
    std::string protocol;
    if (const std::string_view reason = managementCheck(request, protocol); !reason.empty()) {
        managementTracker.recordFailure(connection.peer, now);
        logger.warn(
            [&] { return "Rejected management connection from " + connection.peer + ": " + std::string{reason}; });
        connection.closeAfterFlush = true;
        reply(connection, 401, unauthorizedBody(reason));
        return;
    }
    managementTracker.recordSuccess(connection.peer);
    if (request.method != "GET" || request.path != "/") {
        reply(connection, 404, remoteJson::errorJson("connect a WebSocket to /"));
        return;
    }
    upgrade(connection, request);
}

void RemoteService::Impl::markNotified(Connection& connection) {
    if (!connection.notified) {
        connection.notified = true;
        notified.fetch_add(1);
    }
}

void RemoteService::Impl::handleRpc(Connection& connection, const JsonValue& message) {
    markNotified(connection);
    AttemptTracker& tracker = connection.management ? managementTracker : apiTracker;
    const std::int32_t perMinute =
        connection.management ? settings.management.requestsPerMinute : settings.api.requestsPerMinute;
    if (!tracker.allowRequest(connection.peer, Clock::now(), perMinute)) {
        websocket::encode(connection.out, websocket::opcode::kClose, std::string{"\x03\xF0", 2});
        connection.closeAfterFlush = true;
        return;
    }
    const std::string failure = textFrame(managementRpc::internalErrorFrame("the server could not run the request"));
    dispatch(connection.id, false, failure, [message](RemoteHost& target) {
        ManagementBackend* backend = target.management();
        if (backend == nullptr) {
            return textFrame(managementRpc::internalErrorFrame("the management protocol is not available"));
        }
        const std::string answer = managementRpc::handle(*backend, message);
        return answer.empty() ? std::string{} : textFrame(answer);
    });
}

void RemoteService::Impl::deliverNotifications(std::deque<std::string>& frames) {
    if (frames.empty() || notified.load() == 0) {
        return;
    }
    std::vector<std::uint64_t> targets;
    for (const auto& [id, connection] : connections) {
        if (connection.kind == Kind::WebSocket && connection.notified) {
            targets.push_back(id);
        }
    }
    for (const std::string& frame : frames) {
        const std::string wire = textFrame(frame);
        for (const std::uint64_t id : targets) {
            if (const auto found = connections.find(id); found != connections.end()) {
                found->second.out += wire;
            }
        }
    }
    for (const std::uint64_t id : targets) {
        if (const auto found = connections.find(id); found != connections.end()) {
            service(found->second, false, true);
        }
    }
}

void RemoteService::Impl::beat() {
    const std::int32_t every = heartbeatSeconds.load(std::memory_order_relaxed);
    const Clock::time_point now = Clock::now();
    if (notified.load() == 0) {
        return;
    }
    const bool polls = now - lastPoll >= std::chrono::seconds{1};
    const bool beats = every > 0 && now - lastHeartbeat >= std::chrono::seconds{every};
    if (!polls && !beats) {
        return;
    }
    lastPoll = polls ? now : lastPoll;
    lastHeartbeat = beats ? now : lastHeartbeat;
    host.runOnServerThread([weak = weak_from_this(), polls, beats] {
        if (const std::shared_ptr<Impl> alive = weak.lock(); alive != nullptr) {
            if (ManagementBackend* backend = alive->host.management(); backend != nullptr) {
                if (polls) {
                    backend->poll();
                }
                if (beats) {
                    backend->heartbeat();
                }
            }
        }
    });
}

void RemoteService::publishNotification(std::string frame) {
    Impl& impl = *impl_;
    if (impl.notified.load(std::memory_order_relaxed) == 0) {
        return;
    }
    {
        const std::lock_guard lock{impl.mailbox};
        if (impl.notifications.size() >= detail::kMaxQueuedNotifications) {
            impl.notifications.pop_front();
        }
        impl.notifications.push_back(std::move(frame));
    }
    static_cast<void>(impl.loop.wake());
}

void RemoteService::setHeartbeatSeconds(const std::int32_t seconds) {
    impl_->heartbeatSeconds.store(std::max(0, seconds), std::memory_order_relaxed);
    static_cast<void>(impl_->loop.wake());
}

} // namespace basalt::server::remote
