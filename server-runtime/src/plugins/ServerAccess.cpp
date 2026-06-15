#include <basalt/server/runtime/plugins/ServerAccess.hpp>
#include <basalt/wire/text/Component.hpp>

#include <chrono>

namespace basalt::server::runtime::plugins {
namespace {

using server::plugins::PluginBan;
using server::plugins::PluginKnownPlayer;

constexpr std::int64_t kMillisPerSecond = 1000;

[[nodiscard]] PluginKnownPlayer known(const storage::StoredPlayer& stored) {
    return PluginKnownPlayer{.uuid = stored.uuid,
                             .name = stored.name,
                             .firstSeenMillis = stored.firstSeenEpochSeconds * kMillisPerSecond,
                             .lastSeenMillis = stored.lastSeenEpochSeconds * kMillisPerSecond,
                             .world = stored.world,
                             .x = stored.x,
                             .y = stored.y,
                             .z = stored.z};
}

[[nodiscard]] PluginBan packed(const storage::BanRecord& record) {
    return PluginBan{.subject = record.subject,
                     .name = record.name.value_or(""),
                     .reason = record.reason,
                     .source = record.source,
                     .createdMillis = record.createdAtEpochSeconds * kMillisPerSecond,
                     .expiresMillis = record.expiresAtEpochSeconds.has_value()
                                          ? std::optional{*record.expiresAtEpochSeconds * kMillisPerSecond}
                                          : std::nullopt};
}

[[nodiscard]] storage::BanKind kindOf(bool ip) { return ip ? storage::BanKind::Ip : storage::BanKind::Player; }

} // namespace

storage::PlayerStore* ServerAccess::open() const {
    storage::PlayerStore* store = hooks_.store ? hooks_.store() : nullptr;
    return store != nullptr && store->isOpen() ? store : nullptr;
}

std::optional<PluginKnownPlayer> ServerAccess::playerByUuid(std::string_view uuid) {
    storage::PlayerStore* store = open();
    if (store == nullptr) {
        return std::nullopt;
    }
    const std::optional<storage::StoredPlayer> stored = store->players().load(uuid);
    return stored.has_value() ? std::optional{known(*stored)} : std::nullopt;
}

std::optional<PluginKnownPlayer> ServerAccess::playerByName(std::string_view name) {
    storage::PlayerStore* store = open();
    if (store == nullptr) {
        return std::nullopt;
    }
    const std::optional<storage::StoredPlayer> stored = store->players().loadByName(name);
    return stored.has_value() ? std::optional{known(*stored)} : std::nullopt;
}

std::vector<PluginKnownPlayer> ServerAccess::knownPlayers() {
    std::vector<PluginKnownPlayer> all;
    storage::PlayerStore* store = open();
    if (store == nullptr) {
        return all;
    }
    for (const storage::PlayerSummary& summary : store->players().findLastSeenBefore(INT64_MAX)) {
        all.push_back(PluginKnownPlayer{
            .uuid = summary.uuid, .name = summary.name, .lastSeenMillis = summary.value * kMillisPerSecond});
    }
    return all;
}

bool ServerAccess::isOperator(std::string_view uuid) {
    storage::PlayerStore* store = open();
    if (store == nullptr || !hooks_.groups) {
        return false;
    }
    for (const player::PlayerSession* session :
         hooks_.sessions ? hooks_.sessions() : std::vector<player::PlayerSession*>{}) {
        if (session->uuid().toString() == uuid) {
            return hooks_.groups().resolve(session->group, "*");
        }
    }
    const std::optional<storage::StoredPlayer> stored = store->players().load(uuid);
    return stored.has_value() && hooks_.groups().resolve(stored->group, "*");
}

bool ServerAccess::setOperator(std::string_view uuid, bool value) {
    storage::PlayerStore* store = open();
    if (store == nullptr || !hooks_.groups) {
        return false;
    }
    const std::string group = value ? hooks_.groups().operatorGroup().name : hooks_.groups().defaultGroup().name;
    if (!store->players().setGroup(uuid, group)) {
        return false;
    }
    for (player::PlayerSession* session : hooks_.sessions ? hooks_.sessions() : std::vector<player::PlayerSession*>{}) {
        if (session->uuid().toString() == uuid) {
            session->group = group;
            if (session->connection().sendCommandTree) {
                session->connection().sendCommandTree();
            }
        }
    }
    return true;
}

std::vector<PluginKnownPlayer> ServerAccess::operators() {
    std::vector<PluginKnownPlayer> all;
    storage::PlayerStore* store = open();
    if (store == nullptr || !hooks_.groups) {
        return all;
    }
    for (const storage::PlayerSummary& summary : store->players().findInGroup(hooks_.groups().operatorGroup().name)) {
        all.push_back(PluginKnownPlayer{.uuid = summary.uuid, .name = summary.name});
    }
    return all;
}

bool ServerAccess::isWhitelisted(std::string_view uuid) {
    storage::PlayerStore* store = open();
    return store != nullptr && store->access().isWhitelisted(uuid);
}

void ServerAccess::setWhitelisted(std::string_view uuid, std::string_view name, bool value) {
    storage::PlayerStore* store = open();
    if (store == nullptr) {
        return;
    }
    if (!value) {
        store->access().removeFromWhitelist(uuid);
        return;
    }
    const std::int64_t now =
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    store->access().addToWhitelist(
        storage::WhitelistEntry{.uuid = std::string{uuid}, .name = std::string{name}, .addedAtEpochSeconds = now});
}

std::vector<PluginKnownPlayer> ServerAccess::whitelisted() {
    std::vector<PluginKnownPlayer> all;
    storage::PlayerStore* store = open();
    if (store == nullptr) {
        return all;
    }
    for (const storage::WhitelistEntry& entry : store->access().whitelist()) {
        all.push_back(PluginKnownPlayer{.uuid = entry.uuid, .name = entry.name});
    }
    return all;
}

bool ServerAccess::whitelistEnforced() { return hooks_.whitelistEnforced && hooks_.whitelistEnforced(); }

void ServerAccess::setWhitelistEnforced(bool value) {
    if (hooks_.setWhitelistEnforced) {
        hooks_.setWhitelistEnforced(value);
    }
}

std::optional<PluginBan> ServerAccess::banOf(std::string_view subject, bool ip) {
    storage::PlayerStore* store = open();
    if (store == nullptr) {
        return std::nullopt;
    }
    const std::optional<storage::BanRecord> record = store->access().banOf(subject, kindOf(ip));
    return record.has_value() ? std::optional{packed(*record)} : std::nullopt;
}

std::vector<PluginBan> ServerAccess::bans(bool ip) {
    std::vector<PluginBan> all;
    storage::PlayerStore* store = open();
    if (store == nullptr) {
        return all;
    }
    for (const storage::BanRecord& record : store->access().bans(kindOf(ip))) {
        all.push_back(packed(record));
    }
    return all;
}

void ServerAccess::ban(const PluginBan& record, bool ip) {
    storage::PlayerStore* store = open();
    if (store == nullptr) {
        return;
    }
    storage::BanRecord stored{.subject = record.subject,
                              .kind = kindOf(ip),
                              .name = record.name.empty() ? std::nullopt : std::optional{record.name},
                              .reason = record.reason,
                              .source = record.source,
                              .createdAtEpochSeconds = record.createdMillis / kMillisPerSecond};
    if (record.expiresMillis.has_value()) {
        stored.expiresAtEpochSeconds = *record.expiresMillis / kMillisPerSecond;
    }
    store->access().ban(stored);
    const auto reason = wire::text::Component::translatable(ip ? "multiplayer.disconnect.banned_ip.reason"
                                                               : "multiplayer.disconnect.banned.reason",
                                                            {wire::text::Component::text(record.reason)});
    for (player::PlayerSession* session : hooks_.sessions ? hooks_.sessions() : std::vector<player::PlayerSession*>{}) {
        const bool hit = ip ? session->address().has_value() && *session->address() == record.subject
                            : session->uuid().toString() == record.subject;
        if (hit && session->connection().disconnect) {
            session->connection().disconnect(reason);
        }
    }
}

bool ServerAccess::pardon(std::string_view subject, bool ip) {
    storage::PlayerStore* store = open();
    return store != nullptr && store->access().pardon(subject, kindOf(ip));
}

} // namespace basalt::server::runtime::plugins
