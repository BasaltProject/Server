#include "ManagementDto.hpp"
#include "ManagementMethods.hpp"
#include "ManagementSchema.hpp"

#include <algorithm>
#include <set>

namespace basalt::server::remote::managementMethods {
namespace {

using namespace managementDto;
namespace schema = managementSchema;

constexpr std::string_view kBanSource = "Management server";

void add(std::vector<IncomingMethod>& out, std::string name, std::string description, std::string paramName,
         JsonValue paramSchema, std::string resultName, JsonValue resultSchema, Handler handler) {
    out.push_back(IncomingMethod{.name = std::move(name),
                                 .description = std::move(description),
                                 .paramName = std::move(paramName),
                                 .paramSchema = std::move(paramSchema),
                                 .resultName = std::move(resultName),
                                 .resultSchema = std::move(resultSchema),
                                 .discoverable = true,
                                 .handler = std::move(handler)});
}

[[nodiscard]] std::vector<ManagedPlayer> resolved(ManagementBackend& backend, const std::vector<ManagedPlayer>& hints) {
    std::vector<ManagedPlayer> out;
    for (const ManagedPlayer& hint : hints) {
        if (std::optional<ManagedPlayer> found = backend.resolveUser(hint); found.has_value()) {
            out.push_back(std::move(*found));
        }
    }
    return out;
}

[[nodiscard]] std::set<std::string> idsOf(const std::vector<ManagedPlayer>& players) {
    std::set<std::string> ids;
    for (const ManagedPlayer& player : players) {
        ids.insert(player.id.value_or(""));
    }
    return ids;
}

[[nodiscard]] Parsed<std::vector<ManagedPlayer>> players(const JsonValue* param) {
    return parseList<ManagedPlayer>(*param, parsePlayer);
}

[[nodiscard]] JsonValue playerList(const std::vector<ManagedPlayer>& list) {
    return encodeList<ManagedPlayer>(list, &encodePlayer);
}

[[nodiscard]] std::string sourceOf(const std::optional<std::string>& source) {
    return source.value_or(std::string{kBanSource});
}

[[nodiscard]] JsonValue allowlistNow(ManagementBackend& backend) { return playerList(backend.allowlist()); }

void addAllowlist(std::vector<IncomingMethod>& out) {
    const JsonValue players = schema::array(schema::ref("player"));
    add(out, "allowlist", "Get the allowlist", "", {}, "allowlist", players,
        [](ManagementBackend& backend, const JsonValue*) -> Outcome { return allowlistNow(backend); });
    add(out, "allowlist/set", "Set the allowlist", "players", players, "allowlist", players,
        [](ManagementBackend& backend, const JsonValue* param) -> Outcome {
            const auto wanted = managementMethods::players(param);
            if (!wanted.has_value()) {
                return std::unexpected{invalid(wanted.error())};
            }
            const std::vector<ManagedPlayer> target = resolved(backend, *wanted);
            const std::set<std::string> keep = idsOf(target);
            const std::set<std::string> have = idsOf(backend.allowlist());
            for (const ManagedPlayer& player : backend.allowlist()) {
                if (!keep.contains(player.id.value_or(""))) {
                    backend.allowlistRemove(player);
                }
            }
            for (const ManagedPlayer& player : target) {
                if (!have.contains(player.id.value_or(""))) {
                    backend.allowlistAdd(player);
                }
            }
            backend.kickUnlisted();
            return allowlistNow(backend);
        });
    add(out, "allowlist/add", "Add players to allowlist", "add", players, "allowlist", players,
        [](ManagementBackend& backend, const JsonValue* param) -> Outcome {
            const auto wanted = managementMethods::players(param);
            if (!wanted.has_value()) {
                return std::unexpected{invalid(wanted.error())};
            }
            for (const ManagedPlayer& player : resolved(backend, *wanted)) {
                backend.allowlistAdd(player);
            }
            return allowlistNow(backend);
        });
    add(out, "allowlist/remove", "Remove players from allowlist", "remove", players, "allowlist", players,
        [](ManagementBackend& backend, const JsonValue* param) -> Outcome {
            const auto wanted = managementMethods::players(param);
            if (!wanted.has_value()) {
                return std::unexpected{invalid(wanted.error())};
            }
            for (const ManagedPlayer& player : resolved(backend, *wanted)) {
                backend.allowlistRemove(player);
            }
            backend.kickUnlisted();
            return allowlistNow(backend);
        });
    add(out, "allowlist/clear", "Clear all players in allowlist", "", {}, "allowlist", players,
        [](ManagementBackend& backend, const JsonValue*) -> Outcome {
            for (const ManagedPlayer& player : backend.allowlist()) {
                backend.allowlistRemove(player);
            }
            return allowlistNow(backend);
        });
}

[[nodiscard]] JsonValue bansNow(ManagementBackend& backend) {
    return encodeList<ManagedUserBan>(backend.userBans(), &encodeUserBan);
}

[[nodiscard]] Parsed<std::vector<ManagedUserBan>> userBans(const JsonValue* param) {
    return parseList<ManagedUserBan>(*param, parseUserBan);
}

[[nodiscard]] std::vector<ManagedUserBan> resolvedBans(ManagementBackend& backend,
                                                       const std::vector<ManagedUserBan>& bans) {
    std::vector<ManagedUserBan> out;
    for (ManagedUserBan ban : bans) {
        if (std::optional<ManagedPlayer> found = backend.resolveUser(ban.player); found.has_value()) {
            ban.player = std::move(*found);
            ban.source = sourceOf(ban.source);
            out.push_back(std::move(ban));
        }
    }
    return out;
}

void addBans(std::vector<IncomingMethod>& out) {
    const JsonValue bans = schema::array(schema::ref("user_ban"));
    const JsonValue players = schema::array(schema::ref("player"));
    add(out, "bans", "Get the ban list", "", {}, "banlist", bans,
        [](ManagementBackend& backend, const JsonValue*) -> Outcome { return bansNow(backend); });
    add(out, "bans/set", "Set the banlist", "bans", bans, "banlist", bans,
        [](ManagementBackend& backend, const JsonValue* param) -> Outcome {
            const auto wanted = userBans(param);
            if (!wanted.has_value()) {
                return std::unexpected{invalid(wanted.error())};
            }
            const std::vector<ManagedUserBan> target = resolvedBans(backend, *wanted);
            std::set<std::string> keep;
            for (const ManagedUserBan& ban : target) {
                keep.insert(ban.player.id.value_or(""));
            }
            std::set<std::string> have;
            for (const ManagedUserBan& ban : backend.userBans()) {
                have.insert(ban.player.id.value_or(""));
                if (!keep.contains(ban.player.id.value_or(""))) {
                    backend.unbanUser(ban.player);
                }
            }
            for (const ManagedUserBan& ban : target) {
                if (!have.contains(ban.player.id.value_or(""))) {
                    backend.banUser(ban);
                }
            }
            return bansNow(backend);
        });
    add(out, "bans/add", "Add players to ban list", "add", bans, "banlist", bans,
        [](ManagementBackend& backend, const JsonValue* param) -> Outcome {
            const auto wanted = userBans(param);
            if (!wanted.has_value()) {
                return std::unexpected{invalid(wanted.error())};
            }
            for (const ManagedUserBan& ban : resolvedBans(backend, *wanted)) {
                backend.banUser(ban);
            }
            return bansNow(backend);
        });
    add(out, "bans/remove", "Remove players from ban list", "remove", players, "banlist", bans,
        [](ManagementBackend& backend, const JsonValue* param) -> Outcome {
            const auto wanted = managementMethods::players(param);
            if (!wanted.has_value()) {
                return std::unexpected{invalid(wanted.error())};
            }
            for (const ManagedPlayer& player : resolved(backend, *wanted)) {
                backend.unbanUser(player);
            }
            return bansNow(backend);
        });
    add(out, "bans/clear", "Clear all players in ban list", "", {}, "banlist", bans,
        [](ManagementBackend& backend, const JsonValue*) -> Outcome {
            for (const ManagedUserBan& ban : backend.userBans()) {
                backend.unbanUser(ban.player);
            }
            return bansNow(backend);
        });
}

[[nodiscard]] JsonValue ipBansNow(ManagementBackend& backend) {
    return encodeList<ManagedIpBan>(backend.ipBans(), &encodeIpBan);
}

void banAddress(ManagementBackend& backend, const IncomingIpBan& ban) {
    std::optional<std::string> address;
    if (ban.ip.has_value() && looksLikeAddress(*ban.ip)) {
        address = ban.ip;
    } else if (ban.player.has_value()) {
        address = backend.addressOf(*ban.player);
    }
    if (address.has_value()) {
        backend.banIp(ManagedIpBan{*address, ban.reason, sourceOf(ban.source), ban.expiresEpochSeconds});
    }
}

void addIpBans(std::vector<IncomingMethod>& out) {
    const JsonValue bans = schema::array(schema::ref("ip_ban"));
    add(out, "ip_bans", "Get the ip ban list", "", {}, "banlist", bans,
        [](ManagementBackend& backend, const JsonValue*) -> Outcome { return ipBansNow(backend); });
    add(out, "ip_bans/set", "Set the ip banlist", "banlist", bans, "banlist", bans,
        [](ManagementBackend& backend, const JsonValue* param) -> Outcome {
            const auto wanted = parseList<ManagedIpBan>(*param, parseIpBan);
            if (!wanted.has_value()) {
                return std::unexpected{invalid(wanted.error())};
            }
            std::vector<ManagedIpBan> target;
            std::set<std::string> keep;
            for (ManagedIpBan ban : *wanted) {
                if (looksLikeAddress(ban.ip)) {
                    keep.insert(ban.ip);
                    ban.source = sourceOf(ban.source);
                    target.push_back(std::move(ban));
                }
            }
            std::set<std::string> have;
            for (const ManagedIpBan& ban : backend.ipBans()) {
                have.insert(ban.ip);
                if (!keep.contains(ban.ip)) {
                    backend.unbanIp(ban.ip);
                }
            }
            for (const ManagedIpBan& ban : target) {
                if (!have.contains(ban.ip)) {
                    backend.banIp(ban);
                }
            }
            return ipBansNow(backend);
        });
    add(out, "ip_bans/add", "Add ip to ban list", "add", schema::array(schema::ref("incoming_ip_ban")), "banlist", bans,
        [](ManagementBackend& backend, const JsonValue* param) -> Outcome {
            const auto wanted = parseList<IncomingIpBan>(*param, parseIncomingIpBan);
            if (!wanted.has_value()) {
                return std::unexpected{invalid(wanted.error())};
            }
            for (const IncomingIpBan& ban : *wanted) {
                banAddress(backend, ban);
            }
            return ipBansNow(backend);
        });
    add(out, "ip_bans/remove", "Remove ip from ban list", "ip", schema::array(schema::type("string")), "banlist", bans,
        [](ManagementBackend& backend, const JsonValue* param) -> Outcome {
            const auto wanted = parseList<std::string>(*param, parseString);
            if (!wanted.has_value()) {
                return std::unexpected{invalid(wanted.error())};
            }
            for (const std::string& ip : *wanted) {
                backend.unbanIp(ip);
            }
            return ipBansNow(backend);
        });
    add(out, "ip_bans/clear", "Clear all ips in ban list", "", {}, "banlist", bans,
        [](ManagementBackend& backend, const JsonValue*) -> Outcome {
            for (const ManagedIpBan& ban : backend.ipBans()) {
                backend.unbanIp(ban.ip);
            }
            return ipBansNow(backend);
        });
}

struct KickRequest {
    ManagedPlayer player;
    std::optional<ManagedMessage> message;
};

[[nodiscard]] Parsed<KickRequest> parseKick(const JsonValue& value) {
    const auto* object = value.get<wire::common::JsonObject>();
    const JsonValue* player = object == nullptr ? nullptr : object->find("player");
    if (player == nullptr) {
        return std::unexpected{std::string{"No key player in MapLike[kick_player]"}};
    }
    auto parsed = parsePlayer(*player);
    if (!parsed.has_value()) {
        return std::unexpected{parsed.error()};
    }
    KickRequest request{std::move(*parsed), std::nullopt};
    if (const JsonValue* message = object->find("message"); message != nullptr) {
        auto text = parseMessage(*message);
        if (!text.has_value()) {
            return std::unexpected{text.error()};
        }
        request.message = std::move(*text);
    }
    return request;
}

void addPlayers(std::vector<IncomingMethod>& out) {
    const JsonValue players = schema::array(schema::ref("player"));
    add(out, "players", "Get all connected players", "", {}, "players", players,
        [](ManagementBackend& backend, const JsonValue*) -> Outcome { return playerList(backend.onlinePlayers()); });
    add(out, "players/kick", "Kick players", "kick", schema::array(schema::ref("kick_player")), "kicked", players,
        [](ManagementBackend& backend, const JsonValue* param) -> Outcome {
            const auto wanted = parseList<KickRequest>(*param, parseKick);
            if (!wanted.has_value()) {
                return std::unexpected{invalid(wanted.error())};
            }
            std::vector<ManagedPlayer> kicked;
            for (const KickRequest& request : *wanted) {
                if (const auto found = backend.resolveUser(request.player);
                    found.has_value() && backend.kick(*found, request.message)) {
                    kicked.push_back(*found);
                }
            }
            return playerList(kicked);
        });
}

[[nodiscard]] JsonValue operatorsNow(ManagementBackend& backend) {
    return encodeList<ManagedOperator>(backend.operators(), &encodeOperator);
}

[[nodiscard]] std::vector<ManagedOperator> resolvedOperators(ManagementBackend& backend,
                                                             const std::vector<ManagedOperator>& wanted) {
    std::vector<ManagedOperator> out;
    for (ManagedOperator entry : wanted) {
        if (std::optional<ManagedPlayer> found = backend.resolveUser(entry.player); found.has_value()) {
            entry.player = std::move(*found);
            out.push_back(std::move(entry));
        }
    }
    return out;
}

void addOperators(std::vector<IncomingMethod>& out) {
    const JsonValue operators = schema::array(schema::ref("operator"));
    const JsonValue players = schema::array(schema::ref("player"));
    add(out, "operators", "Get all oped players", "", {}, "operators", operators,
        [](ManagementBackend& backend, const JsonValue*) -> Outcome { return operatorsNow(backend); });
    add(out, "operators/set", "Set all oped players", "operators", operators, "operators", operators,
        [](ManagementBackend& backend, const JsonValue* param) -> Outcome {
            const auto wanted = parseList<ManagedOperator>(*param, parseOperator);
            if (!wanted.has_value()) {
                return std::unexpected{invalid(wanted.error())};
            }
            const std::vector<ManagedOperator> target = resolvedOperators(backend, *wanted);
            std::set<std::string> keep;
            for (const ManagedOperator& entry : target) {
                keep.insert(entry.player.id.value_or(""));
            }
            std::set<std::string> have;
            for (const ManagedOperator& entry : backend.operators()) {
                have.insert(entry.player.id.value_or(""));
                if (!keep.contains(entry.player.id.value_or(""))) {
                    backend.deop(entry.player);
                }
            }
            for (const ManagedOperator& entry : target) {
                if (!have.contains(entry.player.id.value_or(""))) {
                    backend.op(entry.player, entry.permissionLevel, entry.bypassesPlayerLimit);
                }
            }
            return operatorsNow(backend);
        });
    add(out, "operators/add", "Op players", "add", operators, "operators", operators,
        [](ManagementBackend& backend, const JsonValue* param) -> Outcome {
            const auto wanted = parseList<ManagedOperator>(*param, parseOperator);
            if (!wanted.has_value()) {
                return std::unexpected{invalid(wanted.error())};
            }
            for (const ManagedOperator& entry : resolvedOperators(backend, *wanted)) {
                backend.op(entry.player, entry.permissionLevel, entry.bypassesPlayerLimit);
            }
            return operatorsNow(backend);
        });
    add(out, "operators/remove", "Deop players", "remove", players, "operators", operators,
        [](ManagementBackend& backend, const JsonValue* param) -> Outcome {
            const auto wanted = managementMethods::players(param);
            if (!wanted.has_value()) {
                return std::unexpected{invalid(wanted.error())};
            }
            for (const ManagedPlayer& player : resolved(backend, *wanted)) {
                backend.deop(player);
            }
            return operatorsNow(backend);
        });
    add(out, "operators/clear", "Deop all players", "", {}, "operators", operators,
        [](ManagementBackend& backend, const JsonValue*) -> Outcome {
            for (const ManagedOperator& entry : backend.operators()) {
                backend.deop(entry.player);
            }
            return operatorsNow(backend);
        });
}

} // namespace

void addListMethods(std::vector<IncomingMethod>& out) {
    addAllowlist(out);
    addBans(out);
    addIpBans(out);
    addPlayers(out);
    addOperators(out);
}

} // namespace basalt::server::remote::managementMethods
