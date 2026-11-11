#include <basalt/server/remote/ManagementRpc.hpp>

#include <doctest.h>

using namespace basalt::server::remote;
using basalt::wire::common::parseJson;

namespace {

class IdleBackend final : public ManagementBackend {
public:
    bool ready() override { return true; }

    std::string versionName() override { return "26.3"; }

    std::int32_t protocolVersion() override { return 777; }

    std::vector<ManagedPlayer> onlinePlayers() override { return {}; }

    std::optional<ManagedPlayer> resolveUser(const ManagedPlayer&) override { return std::nullopt; }

    std::vector<ManagedPlayer> allowlist() override { return {}; }

    void allowlistAdd(const ManagedPlayer&) override {}

    void allowlistRemove(const ManagedPlayer&) override {}

    void kickUnlisted() override {}

    std::vector<ManagedUserBan> userBans() override { return {}; }

    void banUser(const ManagedUserBan&) override {}

    void unbanUser(const ManagedPlayer&) override {}

    std::vector<ManagedIpBan> ipBans() override { return {}; }

    void banIp(const ManagedIpBan&) override {}

    void unbanIp(std::string_view) override {}

    std::optional<std::string> addressOf(const ManagedPlayer&) override { return std::nullopt; }

    std::vector<ManagedOperator> operators() override { return {}; }

    void op(const ManagedPlayer&, std::optional<std::int32_t>, std::optional<bool>) override {}

    void deop(const ManagedPlayer&) override {}

    bool kick(const ManagedPlayer&, const std::optional<ManagedMessage>&) override { return false; }

    bool save(bool) override { return true; }

    void stop() override {}

    void heartbeat() override {}

    void poll() override {}

    bool systemMessage(const ManagedMessage&, bool, const std::optional<std::vector<ManagedPlayer>>&) override {
        return false;
    }

    std::vector<ManagedGameRule> gameRules() override { return {}; }

    std::optional<std::string> updateGameRule(const ManagedGameRule&) override { return std::nullopt; }

    std::optional<std::string> setting(std::string_view) override { return std::nullopt; }

    std::optional<std::string> changeSetting(std::string_view, std::string_view) override { return std::nullopt; }
};

[[nodiscard]] std::string ask(std::string_view request) {
    IdleBackend backend;
    return managementRpc::handle(backend, *parseJson(request));
}

} // namespace

TEST_CASE("the management protocol answers with vanilla error codes and a bare path means minecraft") {
    CHECK(ask(R"({"jsonrpc":"2.0","id":1,"method":"server/status"})") ==
          R"({"jsonrpc":"2.0","id":1,"result":{"started":true,"version":{"name":"26.3","protocol":777}}})");
    CHECK(ask(R"({"jsonrpc":"2.0","id":2,"method":"minecraft:nope"})").contains("-32601"));
    CHECK(ask(R"({"jsonrpc":"2.0","id":3,"method":"allowlist/add","params":[]})").contains("-32602"));
    CHECK(ask(R"({"jsonrpc":"2.0","method":"server/save","params":[true]})").empty());
}
