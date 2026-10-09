#include "Transport.hpp"

#include <basalt/server/remote/AttemptTracker.hpp>
#include <basalt/server/remote/Http.hpp>
#include <basalt/server/remote/RconCodec.hpp>
#include <basalt/server/remote/RemoteService.hpp>
#include <basalt/server/remote/WebSocketCodec.hpp>
#include <basalt/wire/common/json/Json.hpp>
#include <basalt/wire/io/EventLoop.hpp>
#include <basalt/wire/io/TcpSocket.hpp>

#include <atomic>
#include <deque>
#include <map>
#include <mutex>
#include <thread>
#include <unordered_map>

namespace basalt::server::remote {
namespace {

using Clock = std::chrono::steady_clock;
using wire::common::JsonObject;
using wire::common::JsonValue;

constexpr std::uint64_t kRconToken = 1;
constexpr std::uint64_t kApiToken = 2;
constexpr std::uint64_t kFirstConnection = 16;
constexpr std::size_t kMaxRconConnections = 16;
constexpr std::int32_t kRconIdleSeconds = 30;
constexpr std::size_t kMaxQueuedLogLines = 4096;
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
    Clock::time_point lastReceived;
    Clock::time_point lastPing;
};

struct Completion {
    std::uint64_t connection = 0;
    std::string bytes;
    bool endBusy = false;
    bool close = false;
};

[[nodiscard]] bool constantTimeEquals(std::string_view a, std::string_view b) {
    unsigned char diff = a.size() == b.size() ? 0 : 1;
    const std::size_t length = std::max(a.size(), b.size());
    for (std::size_t i = 0; i < length; ++i) {
        const auto left = static_cast<unsigned char>(i < a.size() ? a[i] : 0);
        const auto right = static_cast<unsigned char>(i < b.size() ? b[i] : 0);
        diff = static_cast<unsigned char>(diff | (left ^ right));
    }
    return diff == 0;
}

[[nodiscard]] std::string jsonText(const JsonObject& object) {
    return wire::common::writeJson(JsonValue::ofObject(object));
}

[[nodiscard]] std::string errorJson(std::string_view message) {
    JsonObject object;
    object.set("error", JsonValue::of(std::string{message}));
    return jsonText(object);
}

[[nodiscard]] std::string resultJson(const CommandResult& result, const JsonValue* id) {
    JsonObject object;
    if (id != nullptr) {
        object.set("type", JsonValue::of(std::string{"result"}));
        object.set("id", *id);
    }
    object.set("cancelled", JsonValue::of(result.cancelled));
    object.set("output", JsonValue::of(result.output));
    return jsonText(object);
}

} // namespace

struct RemoteService::Impl {
    RemoteSettings settings;
    RemoteHost& host;
    wire::common::Logger logger;
    wire::io::EventLoop loop;
    std::optional<wire::io::TcpListener> rconListener;
    std::optional<wire::io::TcpListener> apiListener;
    std::unique_ptr<TlsContext> tls;
    AttemptTracker rconTracker;
    AttemptTracker apiTracker;
    std::map<std::uint64_t, Connection> connections;
    std::uint64_t nextId = kFirstConnection;
    std::jthread thread;
    std::atomic<bool> running{false};
    std::atomic<std::int32_t> subscribers{0};
    std::mutex mailbox;
    std::deque<Completion> completions;
    std::deque<std::string> logLines;
    Clock::time_point lastSweep = Clock::now();

    Impl(RemoteSettings s, RemoteHost& h, wire::common::Logger l)
        : settings(std::move(s)), host(h), logger(std::move(l)),
          rconTracker(settings.rcon.maxFailures, settings.rcon.lockoutSeconds),
          apiTracker(settings.api.maxFailures, settings.api.lockoutSeconds) {}

    void post(Completion completion) {
        {
            const std::lock_guard lock{mailbox};
            completions.push_back(std::move(completion));
        }
        static_cast<void>(loop.wake());
    }

    [[nodiscard]] std::size_t countOf(Kind a, Kind b) const {
        std::size_t count = 0;
        for (const auto& [id, connection] : connections) {
            count += connection.kind == a || connection.kind == b ? 1 : 0;
        }
        return count;
    }

    void run(std::stop_token stop);
    void accept(wire::io::TcpListener& listener, bool api);
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
    void reply(Connection& connection, int status, std::string_view body, std::string_view extra = {});
    void runCommand(Connection& connection, Origin origin, std::string command,
                    std::function<std::string(CommandResult)> render);
    [[nodiscard]] bool authorised(const http::Request& request, std::string& chosenProtocol) const;
};

void RemoteService::Impl::accept(wire::io::TcpListener& listener, bool api) {
    while (true) {
        std::expected<wire::io::TcpConnection, wire::io::SocketError> accepted = listener.accept();
        if (!accepted.has_value()) {
            return;
        }
        static_cast<void>(accepted->setNonBlocking());
        const std::string peer = accepted->remoteAddress();
        AttemptTracker& tracker = api ? apiTracker : rconTracker;
        const std::size_t limit = api ? static_cast<std::size_t>(settings.api.maxConnections) : kMaxRconConnections;
        const std::size_t open = api ? countOf(Kind::Http, Kind::WebSocket) : countOf(Kind::Rcon, Kind::Rcon);
        if (open >= limit || (!api && tracker.lockedFor(peer, Clock::now()) > 0)) {
            continue;
        }
        Connection connection;
        connection.id = nextId++;
        connection.kind = api ? Kind::Http : Kind::Rcon;
        connection.peer = peer;
        connection.lastReceived = Clock::now();
        connection.lastPing = connection.lastReceived;
        connection.transport = std::make_unique<Transport>(std::move(*accepted), api ? tls.get() : nullptr);
        const std::int64_t handle = connection.transport->socket().nativeHandle();
        if (handle < 0 || !loop.watch(handle, {true, false}, connection.id).has_value()) {
            continue;
        }
        const std::uint64_t id = connection.id;
        connections.emplace(id, std::move(connection));
        service(connections.at(id), true, false);
    }
}

void RemoteService::Impl::close(std::uint64_t id) {
    const auto found = connections.find(id);
    if (found == connections.end()) {
        return;
    }
    if (found->second.kind == Kind::WebSocket) {
        subscribers.fetch_sub(1);
    }
    loop.unwatch(found->second.transport->socket().nativeHandle());
    connections.erase(found);
}

void RemoteService::Impl::refreshInterest(Connection& connection) {
    const bool wantWrite = !connection.out.empty() || connection.transport->handshaking();
    if (wantWrite != connection.writeInterest) {
        connection.writeInterest = wantWrite;
        static_cast<void>(loop.watch(connection.transport->socket().nativeHandle(), {true, wantWrite}, connection.id));
    }
}

void RemoteService::Impl::flush(Connection& connection) {
    if (connection.out.empty()) {
        return;
    }
    if (connection.transport->writeFrom(connection.out) == IoState::Closed) {
        connection.closeAfterFlush = true;
        connection.out.clear();
    }
}

void RemoteService::Impl::reply(Connection& connection, int status, std::string_view body, std::string_view extra) {
    const bool keep = status < 400 && !connection.closeAfterFlush;
    connection.out += http::response(status, "application/json", body, keep, extra);
    if (!keep) {
        connection.closeAfterFlush = true;
    }
}

bool RemoteService::Impl::authorised(const http::Request& request, std::string& chosenProtocol) const {
    if (const auto header = request.header("authorization"); header.has_value()) {
        constexpr std::string_view kBearer = "Bearer ";
        if (header->size() > kBearer.size() && http::lower(header->substr(0, kBearer.size())) == "bearer ") {
            return constantTimeEquals(header->substr(kBearer.size()), settings.api.token);
        }
        return false;
    }
    if (const auto offered = request.header("sec-websocket-protocol"); offered.has_value()) {
        std::string_view list = *offered;
        while (!list.empty()) {
            const std::size_t comma = list.find(',');
            std::string_view item = list.substr(0, comma);
            while (!item.empty() && item.front() == ' ') {
                item.remove_prefix(1);
            }
            if (item.starts_with("bearer.")) {
                chosenProtocol = std::string{item};
                return constantTimeEquals(item.substr(7), settings.api.token);
            }
            if (comma == std::string_view::npos) {
                break;
            }
            list.remove_prefix(comma + 1);
        }
    }
    return false;
}

void RemoteService::Impl::runCommand(Connection& connection, Origin origin, std::string command,
                                     std::function<std::string(CommandResult)> render) {
    connection.busy = origin == Origin::Rcon || connection.kind == Kind::Http;
    const std::uint64_t id = connection.id;
    const std::string peer = connection.peer;
    const bool endBusy = connection.busy;
    logger.info([&] {
        return std::string{"Remote command from "} + peer + " (" + (origin == Origin::Rcon ? "rcon" : "api") +
               "): " + command;
    });
    host.runOnServerThread([this, id, origin, endBusy, peer, command = std::move(command), render = std::move(render)] {
        CommandResult result = host.executeCommand(origin, peer, command);
        post(Completion{id, render(std::move(result)), endBusy, false});
    });
}

void RemoteService::Impl::processRcon(Connection& connection) {
    rcon::Packet packet;
    while (!connection.busy && !connection.closeAfterFlush) {
        const rcon::ReadResult taken = rcon::take(connection.in, packet);
        if (taken == rcon::ReadResult::NeedMore) {
            return;
        }
        if (taken == rcon::ReadResult::Malformed) {
            connection.closeAfterFlush = true;
            return;
        }
        if (!connection.authed) {
            const Clock::time_point now = Clock::now();
            const bool good =
                packet.type == rcon::packetType::kAuth && constantTimeEquals(packet.body, settings.rcon.password);
            if (good) {
                rconTracker.recordSuccess(connection.peer);
                connection.authed = true;
                rcon::encode(connection.out, {packet.id, rcon::packetType::kCommand, {}});
                continue;
            }
            rconTracker.recordFailure(connection.peer, now);
            logger.warn([&] { return "Failed RCON login from " + connection.peer + "."; });
            rcon::encode(connection.out, {rcon::kAuthFailedId, rcon::packetType::kCommand, {}});
            connection.closeAfterFlush = true;
            return;
        }
        if (packet.type != rcon::packetType::kCommand) {
            char text[32];
            std::snprintf(text, sizeof(text), "Unknown request %x", static_cast<unsigned>(packet.type));
            rcon::encodeResponse(connection.out, packet.id, text);
            continue;
        }
        const std::int32_t requestId = packet.id;
        runCommand(connection, Origin::Rcon, std::move(packet.body), [requestId](CommandResult result) {
            std::string bytes;
            rcon::encodeResponse(bytes, requestId, result.output);
            return bytes;
        });
    }
}

void RemoteService::Impl::upgrade(Connection& connection, const http::Request& request) {
    const auto key = request.header("sec-websocket-key");
    const auto version = request.header("sec-websocket-version");
    const auto connectionHeader = request.header("connection");
    const auto upgradeHeader = request.header("upgrade");
    if (!key.has_value() || !version.has_value() || *version != "13" || !upgradeHeader.has_value() ||
        http::lower(*upgradeHeader) != "websocket" || !connectionHeader.has_value() ||
        !http::tokenListContains(*connectionHeader, "upgrade")) {
        reply(connection, 426, errorJson("a WebSocket upgrade (version 13) is required"), "Upgrade: websocket\r\n");
        return;
    }
    std::string protocol;
    static_cast<void>(authorised(request, protocol));
    std::string head = "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n";
    head += "Sec-WebSocket-Accept: " + websocket::acceptKey(*key) + "\r\n";
    if (!protocol.empty()) {
        head += "Sec-WebSocket-Protocol: " + protocol + "\r\n";
    }
    connection.out += head + "\r\n";
    connection.kind = Kind::WebSocket;
    connection.lastPing = Clock::now();
    subscribers.fetch_add(1);
}

void RemoteService::Impl::handleRequest(Connection& connection, http::Request& request) {
    const Clock::time_point now = Clock::now();
    if (const std::int32_t locked = apiTracker.lockedFor(connection.peer, now); locked > 0) {
        connection.closeAfterFlush = true;
        reply(connection, 429, errorJson("too many failed attempts"),
              "Retry-After: " + std::to_string(locked) + "\r\n");
        return;
    }
    if (!apiTracker.allowRequest(connection.peer, now, settings.api.requestsPerMinute)) {
        connection.closeAfterFlush = true;
        reply(connection, 429, errorJson("rate limit exceeded"), "Retry-After: 60\r\n");
        return;
    }
    std::string protocol;
    if (!authorised(request, protocol)) {
        apiTracker.recordFailure(connection.peer, now);
        logger.warn([&] { return "Rejected API request from " + connection.peer + ": bad or missing token."; });
        connection.closeAfterFlush = true;
        reply(connection, 401, errorJson("a valid bearer token is required"), "WWW-Authenticate: Bearer\r\n");
        return;
    }
    apiTracker.recordSuccess(connection.peer);
    const bool get = request.method == "GET";
    const auto listing = [&](std::string (RemoteHost::*fetch)()) {
        connection.busy = true;
        const std::uint64_t id = connection.id;
        host.runOnServerThread([this, id, fetch] {
            std::string body = (host.*fetch)();
            post(Completion{id, http::response(200, "application/json", body, true), true, false});
        });
    };
    if (request.path == "/v1/status" || request.path == "/v1/players" || request.path == "/v1/plugins") {
        if (!get) {
            reply(connection, 405, errorJson("use GET"), "Allow: GET\r\n");
        } else if (request.path == "/v1/status") {
            listing(&RemoteHost::statusJson);
        } else if (request.path == "/v1/players") {
            listing(&RemoteHost::playersJson);
        } else {
            listing(&RemoteHost::pluginsJson);
        }
    } else if (request.path == "/v1/command") {
        if (request.method != "POST") {
            reply(connection, 405, errorJson("use POST"), "Allow: POST\r\n");
            return;
        }
        const auto parsed = wire::common::parseJson(request.body);
        const JsonObject* object = parsed.has_value() ? parsed->get<JsonObject>() : nullptr;
        const JsonValue* command = object == nullptr ? nullptr : object->find("command");
        const std::string* text = command == nullptr ? nullptr : command->get<std::string>();
        if (text == nullptr || text->empty() || text->size() > kMaxCommandChars) {
            reply(connection, 400, errorJson("the body must be {\"command\": \"<non-empty string>\"}"));
            return;
        }
        runCommand(connection, Origin::Api, *text, [](CommandResult result) {
            return http::response(200, "application/json", resultJson(result, nullptr), true);
        });
    } else if (request.path == "/v1/console") {
        if (!get) {
            reply(connection, 405, errorJson("use GET"), "Allow: GET\r\n");
        } else {
            upgrade(connection, request);
        }
    } else {
        reply(connection, 404, errorJson("no such route"));
    }
}

void RemoteService::Impl::processHttp(Connection& connection) {
    http::Request request;
    while (!connection.busy && !connection.closeAfterFlush && connection.kind == Kind::Http) {
        const http::ParseResult taken =
            http::take(connection.in, request, static_cast<std::size_t>(settings.api.maxBodyBytes));
        switch (taken) {
        case http::ParseResult::NeedMore:
            return;
        case http::ParseResult::Request:
            handleRequest(connection, request);
            continue;
        case http::ParseResult::HeadTooLarge:
            connection.closeAfterFlush = true;
            reply(connection, 431, errorJson("request head too large"));
            return;
        case http::ParseResult::BodyTooLarge:
            connection.closeAfterFlush = true;
            reply(connection, 413, errorJson("request body too large"));
            return;
        case http::ParseResult::Unsupported:
            connection.closeAfterFlush = true;
            reply(connection, 501, errorJson("chunked request bodies are not supported"));
            return;
        case http::ParseResult::Malformed:
            connection.closeAfterFlush = true;
            reply(connection, 400, errorJson("malformed request"));
            return;
        }
    }
    if (connection.kind == Kind::WebSocket) {
        processWebSocket(connection);
    }
}

void RemoteService::Impl::handleWsMessage(Connection& connection, const std::string& text) {
    JsonValue id;
    std::string command = text;
    bool hasId = false;
    if (!text.empty() && text.front() == '{') {
        const auto parsed = wire::common::parseJson(text);
        const JsonObject* object = parsed.has_value() ? parsed->get<JsonObject>() : nullptr;
        const JsonValue* written = object == nullptr ? nullptr : object->find("command");
        const std::string* value = written == nullptr ? nullptr : written->get<std::string>();
        if (value == nullptr) {
            std::string frame;
            websocket::encode(frame, websocket::opcode::kText, errorJson("send {\"command\": \"...\"}"));
            connection.out += frame;
            return;
        }
        command = *value;
        if (const JsonValue* given = object->find("id"); given != nullptr) {
            id = *given;
            hasId = true;
        }
    }
    std::string failure;
    if (command.empty() || command.size() > kMaxCommandChars) {
        failure = "the command must be a non-empty string";
    } else if (!apiTracker.allowRequest(connection.peer, Clock::now(), settings.api.requestsPerMinute)) {
        failure = "rate limit exceeded";
    }
    if (!failure.empty()) {
        std::string frame;
        websocket::encode(frame, websocket::opcode::kText, errorJson(failure));
        connection.out += frame;
        return;
    }
    runCommand(connection, Origin::Api, std::move(command), [id, hasId](CommandResult result) {
        std::string frame;
        const JsonValue none;
        websocket::encode(frame, websocket::opcode::kText, resultJson(result, &(hasId ? id : none)));
        return frame;
    });
}

void RemoteService::Impl::processWebSocket(Connection& connection) {
    websocket::Frame frame;
    while (!connection.closeAfterFlush) {
        const websocket::ReadResult taken = websocket::take(connection.in, frame, kMaxWsMessage);
        if (taken == websocket::ReadResult::NeedMore) {
            return;
        }
        if (taken != websocket::ReadResult::Frame) {
            websocket::encode(connection.out, websocket::opcode::kClose, std::string{"\x03\xF1", 2});
            connection.closeAfterFlush = true;
            return;
        }
        switch (frame.opcode) {
        case websocket::opcode::kPing:
            websocket::encode(connection.out, websocket::opcode::kPong, frame.payload);
            break;
        case websocket::opcode::kClose:
            websocket::encode(connection.out, websocket::opcode::kClose, frame.payload.substr(0, 2));
            connection.closeAfterFlush = true;
            break;
        case websocket::opcode::kText:
        case websocket::opcode::kBinary:
        case websocket::opcode::kContinuation:
            connection.fragments += frame.payload;
            if (connection.fragments.size() > kMaxWsMessage) {
                connection.closeAfterFlush = true;
                break;
            }
            if (frame.fin) {
                std::string message = std::move(connection.fragments);
                connection.fragments.clear();
                handleWsMessage(connection, message);
            }
            break;
        default:
            break;
        }
    }
}

void RemoteService::Impl::process(Connection& connection) {
    if (connection.kind == Kind::Rcon) {
        processRcon(connection);
    } else if (connection.kind == Kind::Http) {
        processHttp(connection);
    } else {
        processWebSocket(connection);
    }
}

void RemoteService::Impl::service(Connection& connection, bool readable, bool writable) {
    static_cast<void>(writable);
    const std::uint64_t id = connection.id;
    if (connection.transport->handshaking()) {
        if (connection.transport->handshake() == IoState::Closed) {
            close(id);
            return;
        }
    }
    if (readable && !connection.transport->handshaking() && !connection.closeAfterFlush) {
        const std::size_t before = connection.in.size();
        const IoState state = connection.transport->readInto(
            connection.in,
            std::max<std::size_t>(1 << 20, static_cast<std::size_t>(settings.api.maxBodyBytes) + (1 << 16)));
        if (connection.in.size() != before) {
            connection.lastReceived = Clock::now();
        }
        process(connection);
        if (state == IoState::Closed) {
            flush(connection);
            close(id);
            return;
        }
    }
    flush(connection);
    if (connection.closeAfterFlush && connection.out.empty()) {
        close(id);
        return;
    }
    if (connection.out.size() > kMaxOutBytes) {
        close(id);
        return;
    }
    refreshInterest(connection);
}

void RemoteService::Impl::drainMailbox() {
    std::deque<Completion> done;
    std::deque<std::string> lines;
    {
        const std::lock_guard lock{mailbox};
        done.swap(completions);
        lines.swap(logLines);
    }
    for (Completion& completion : done) {
        const auto found = connections.find(completion.connection);
        if (found == connections.end()) {
            continue;
        }
        Connection& connection = found->second;
        connection.out += completion.bytes;
        if (completion.endBusy) {
            connection.busy = false;
            process(connection);
        }
        service(connection, false, true);
    }
    if (subscribers.load() == 0) {
        return;
    }
    std::vector<std::uint64_t> targets;
    for (const auto& [id, connection] : connections) {
        if (connection.kind == Kind::WebSocket) {
            targets.push_back(id);
        }
    }
    for (const std::string& line : lines) {
        JsonObject object;
        object.set("type", JsonValue::of(std::string{"log"}));
        object.set("line", JsonValue::of(line));
        std::string frame;
        websocket::encode(frame, websocket::opcode::kText, jsonText(object));
        for (const std::uint64_t id : targets) {
            if (const auto found = connections.find(id); found != connections.end()) {
                found->second.out += frame;
            }
        }
    }
    for (const std::uint64_t id : targets) {
        if (const auto found = connections.find(id); found != connections.end()) {
            service(found->second, false, true);
        }
    }
}

void RemoteService::Impl::sweep() {
    const Clock::time_point now = Clock::now();
    std::vector<std::uint64_t> stale;
    std::vector<std::uint64_t> pinged;
    for (auto& [id, connection] : connections) {
        const std::int32_t idle = connection.kind == Kind::Rcon && !connection.authed ? kRconIdleSeconds
                                  : connection.kind == Kind::Rcon                     ? settings.api.idleSeconds * 10
                                                                                      : settings.api.idleSeconds;
        const auto quiet = std::chrono::duration_cast<std::chrono::seconds>(now - connection.lastReceived).count();
        if (quiet > idle && !connection.busy) {
            stale.push_back(id);
        } else if (connection.kind == Kind::WebSocket &&
                   std::chrono::duration_cast<std::chrono::seconds>(now - connection.lastPing).count() >=
                       std::max(1, idle / 2)) {
            connection.lastPing = now;
            websocket::encode(connection.out, websocket::opcode::kPing, {});
            pinged.push_back(id);
        }
    }
    for (const std::uint64_t id : pinged) {
        if (const auto found = connections.find(id); found != connections.end()) {
            service(found->second, false, true);
        }
    }
    for (const std::uint64_t id : stale) {
        close(id);
    }
    if (now - lastSweep >= std::chrono::seconds{60}) {
        lastSweep = now;
        rconTracker.sweep(now);
        apiTracker.sweep(now);
    }
}

void RemoteService::Impl::run(std::stop_token stop) {
    while (!stop.stop_requested()) {
        const std::optional<int> timeout = connections.empty() ? std::nullopt : std::optional<int>{1000};
        const auto ready = loop.wait(timeout);
        if (stop.stop_requested()) {
            break;
        }
        if (ready.has_value()) {
            const std::vector<wire::io::IoReady> events{ready->begin(), ready->end()};
            for (const wire::io::IoReady& event : events) {
                if (event.token == kRconToken && rconListener.has_value()) {
                    accept(*rconListener, false);
                } else if (event.token == kApiToken && apiListener.has_value()) {
                    accept(*apiListener, true);
                } else if (const auto found = connections.find(event.token); found != connections.end()) {
                    if (event.closed && !event.readable) {
                        close(found->first);
                    } else {
                        service(found->second, event.readable || event.closed, event.writable);
                    }
                }
            }
        }
        drainMailbox();
        if (!connections.empty()) {
            sweep();
        }
    }
    connections.clear();
}

RemoteService::RemoteService(RemoteSettings settings, RemoteHost& host, wire::common::Logger logger)
    : impl_(std::make_unique<Impl>(std::move(settings), host, std::move(logger))) {}

RemoteService::~RemoteService() { stop(); }

std::expected<void, std::string> RemoteService::start() {
    Impl& impl = *impl_;
    const auto bind = [&impl](const std::string& host, std::int32_t port, std::uint64_t token,
                              std::optional<wire::io::TcpListener>& into) -> std::expected<void, std::string> {
        const auto address = wire::io::TcpAddress::literal(host, static_cast<std::uint16_t>(port));
        if (!address.has_value()) {
            return std::unexpected("invalid bind address " + host);
        }
        auto bound = wire::io::TcpListener::bind(*address, 64);
        if (!bound.has_value()) {
            return std::unexpected("could not bind " + host + " port " + std::to_string(port));
        }
        static_cast<void>(bound->setNonBlocking());
        if (!impl.loop.watch(bound->nativeHandle(), {true, false}, token).has_value()) {
            return std::unexpected("could not watch the listener on port " + std::to_string(port));
        }
        into = std::move(*bound);
        return {};
    };
    if (impl.settings.api.enabled && impl.settings.api.tls) {
        auto context = TlsContext::create(impl.settings.api.certPath, impl.settings.api.keyPath);
        if (!context.has_value()) {
            return std::unexpected(context.error());
        }
        impl.tls = std::move(*context);
    }
    if (impl.settings.rcon.enabled) {
        if (auto bound = bind(impl.settings.rcon.bind, impl.settings.rcon.port, kRconToken, impl.rconListener);
            !bound.has_value()) {
            return std::unexpected("RCON: " + bound.error());
        }
    }
    if (impl.settings.api.enabled) {
        if (auto bound = bind(impl.settings.api.bind, impl.settings.api.port, kApiToken, impl.apiListener);
            !bound.has_value()) {
            return std::unexpected("API: " + bound.error());
        }
    }
    if (!impl.rconListener.has_value() && !impl.apiListener.has_value()) {
        return {};
    }
    impl.running = true;
    impl.thread = std::jthread([&impl](std::stop_token stop) { impl.run(stop); });
    return {};
}

void RemoteService::stop() {
    Impl& impl = *impl_;
    if (!impl.running.exchange(false)) {
        return;
    }
    impl.thread.request_stop();
    static_cast<void>(impl.loop.wake());
    impl.thread.join();
    impl.rconListener.reset();
    impl.apiListener.reset();
}

void RemoteService::publishLog(std::string_view line) {
    Impl& impl = *impl_;
    if (impl.subscribers.load(std::memory_order_relaxed) == 0) {
        return;
    }
    {
        const std::lock_guard lock{impl.mailbox};
        if (impl.logLines.size() >= kMaxQueuedLogLines) {
            impl.logLines.pop_front();
        }
        impl.logLines.emplace_back(line);
    }
    static_cast<void>(impl.loop.wake());
}

} // namespace basalt::server::remote
