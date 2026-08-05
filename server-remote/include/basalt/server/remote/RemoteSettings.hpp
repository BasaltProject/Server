#pragma once

#include <cstdint>
#include <string>

namespace basalt::server::remote {

struct RconSettings {
    bool enabled = false;
    std::string bind = "127.0.0.1";
    std::int32_t port = 25575;
    std::string password;
    std::int32_t maxFailures = 5;
    std::int32_t lockoutSeconds = 30;
};

struct ApiSettings {
    bool enabled = true;
    std::string bind = "127.0.0.1";
    std::int32_t port = 25580;
    std::string token;
    bool tls = false;
    std::string certPath;
    std::string keyPath;
    std::int32_t maxFailures = 5;
    std::int32_t lockoutSeconds = 30;
    std::int32_t requestsPerMinute = 120;
    std::int32_t maxConnections = 32;
    std::int32_t maxBodyBytes = 65536;
    std::int32_t idleSeconds = 60;
};

struct RemoteSettings {
    RconSettings rcon;
    ApiSettings api;
};

} // namespace basalt::server::remote
