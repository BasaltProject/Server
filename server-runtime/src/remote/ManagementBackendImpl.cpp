#include <basalt/server/runtime/BuildInfo.hpp>
#include <basalt/server/runtime/command/CommandDispatcher.hpp>
#include <basalt/server/runtime/command/CommandSource.hpp>
#include <basalt/server/runtime/remote/RemoteHostImpl.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/storage/AccessRepository.hpp>
#include <basalt/server/storage/PlayerRepository.hpp>
#include <basalt/wire/common/Uuid.hpp>

#include <algorithm>
#include <chrono>
#include <map>
#include <set>

namespace basalt::server::runtime::remoteHost {
namespace {

using server::remote::ManagedGameRule;
using server::remote::ManagedIpBan;
using server::remote::ManagedMessage;
using server::remote::ManagedOperator;
using server::remote::ManagedPlayer;
using server::remote::ManagedUserBan;
using storage::BanKind;
using storage::BanRecord;
using wire::text::Component;

constexpr std::string_view kSource = "Management server";
constexpr std::string_view kDefaultReason = "Banned by an operator.";

[[nodiscard]] std::int64_t nowSeconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch())
        .count();
}

[[nodiscard]] bool sameName(std::string_view left, std::string_view right) {
    return std::ranges::equal(left, right, [](char a, char b) {
        return static_cast<char>(std::tolower(static_cast<unsigned char>(a))) ==
               static_cast<char>(std::tolower(static_cast<unsigned char>(b)));
    });
}

[[nodiscard]] Component componentOf(const ManagedMessage& message) {
    if (message.translatable.has_value()) {
        std::vector<Component> params;
        for (const std::string& param : message.translatableParams.value_or(std::vector<std::string>{})) {
            params.push_back(Component::text(param));
        }
        return Component::translatable(*message.translatable, std::move(params));
    }
    return Component::text(message.literal.value_or(""));
}

[[nodiscard]] std::string_view settingKey(std::string_view key) {
    if (key == "use_allowlist") {
        return "whitelist";
    }
    if (key == "spawn_protection_radius") {
        return "spawn_protection";
    }
    if (key == "player_idle_timeout") {
        return "idle_timeout";
    }
    if (key == "game_mode") {
        return "gamemode";
    }
    return key;
}

[[nodiscard]] ManagedUserBan userBanOfChange(const storage::AccessChange& change) {
    const BanRecord record = change.ban.value_or(BanRecord{});
    return ManagedUserBan{
        ManagedPlayer{change.subject, change.name.empty() ? std::nullopt : std::optional{change.name}}, record.reason,
        record.source, record.expiresAtEpochSeconds};
}

[[nodiscard]] ManagedIpBan ipBanOfChange(const storage::AccessChange& change) {
    const BanRecord record = change.ban.value_or(BanRecord{});
    return ManagedIpBan{change.subject, record.reason, record.source, record.expiresAtEpochSeconds};
}

class Backend final : public server::remote::ManagementBackend {
public:
    explicit Backend(ManagementSources sources) : s_(std::move(sources)) {}

    bool ready() override { return s_.ready && s_.ready(); }

    std::string versionName() override { return std::string{buildInfo::kMinecraftVersion}; }

    std::int32_t protocolVersion() override { return buildInfo::kProtocolVersion; }

    std::vector<ManagedPlayer> onlinePlayers() override {
        std::vector<ManagedPlayer> found;
        for (const player::PlayerSession* session : s_.players->snapshot()) {
            found.push_back(ManagedPlayer{session->uuid().toString(), session->name()});
        }
        return found;
    }

    std::optional<ManagedPlayer> resolveUser(const ManagedPlayer& hint) override {
        for (const player::PlayerSession* session : s_.players->snapshot()) {
            if ((hint.id.has_value() && *hint.id == session->uuid().toString()) ||
                (!hint.id.has_value() && hint.name.has_value() && sameName(*hint.name, session->name()))) {
                return ManagedPlayer{session->uuid().toString(), session->name()};
            }
        }
        if (storage::PlayerRepository* players = options().playerRepository; players != nullptr) {
            const std::optional<storage::StoredPlayer> stored =
                hint.id.has_value() ? players->load(*hint.id) : players->loadByName(hint.name.value_or(""));
            if (stored.has_value()) {
                return ManagedPlayer{stored->uuid, stored->name};
            }
        }
        if (hint.id.has_value() && hint.name.has_value()) {
            return ManagedPlayer{hint.id, hint.name};
        }
        if (hint.name.has_value() && !hint.name->empty()) {
            return ManagedPlayer{wire::common::Uuid::offlinePlayer(*hint.name).toString(), hint.name};
        }
        return std::nullopt;
    }

    std::vector<ManagedPlayer> allowlist() override {
        std::vector<ManagedPlayer> found;
        if (storage::AccessRepository* access = options().access; access != nullptr) {
            for (const storage::WhitelistEntry& entry : access->whitelist()) {
                found.push_back(ManagedPlayer{entry.uuid, entry.name});
            }
        }
        return found;
    }

    void allowlistAdd(const ManagedPlayer& player) override {
        if (storage::AccessRepository* access = options().access; access != nullptr && player.id.has_value()) {
            access->addToWhitelist(storage::WhitelistEntry{*player.id, player.name.value_or(""), nowSeconds()});
        }
    }

    void allowlistRemove(const ManagedPlayer& player) override {
        if (storage::AccessRepository* access = options().access; access != nullptr && player.id.has_value()) {
            access->removeFromWhitelist(*player.id);
        }
    }

    void kickUnlisted() override {
        if (s_.settings != nullptr) {
            static_cast<void>(s_.settings->set("enforce_allowlist", *s_.settings->get("enforce_allowlist")));
        }
    }

    std::vector<ManagedUserBan> userBans() override {
        std::vector<ManagedUserBan> found;
        if (storage::AccessRepository* access = options().access; access != nullptr) {
            for (const BanRecord& record : access->bans(BanKind::Player)) {
                found.push_back(userBanOf(record));
            }
        }
        return found;
    }

    void banUser(const ManagedUserBan& ban) override {
        storage::AccessRepository* access = options().access;
        if (access == nullptr || !ban.player.id.has_value()) {
            return;
        }
        const std::string reason{ban.reason.value_or(std::string{kDefaultReason})};
        access->ban(BanRecord{.subject = *ban.player.id,
                              .kind = BanKind::Player,
                              .name = ban.player.name,
                              .reason = reason,
                              .source = ban.source.value_or(std::string{kSource}),
                              .createdAtEpochSeconds = nowSeconds(),
                              .expiresAtEpochSeconds = ban.expiresEpochSeconds});
        for (player::PlayerSession* session : s_.players->snapshot()) {
            if (session->uuid().toString() == *ban.player.id && session->connection().disconnect) {
                session->connection().disconnect(
                    Component::translatable("multiplayer.disconnect.banned.reason", {Component::text(reason)}));
            }
        }
    }

    void unbanUser(const ManagedPlayer& player) override {
        if (storage::AccessRepository* access = options().access; access != nullptr && player.id.has_value()) {
            access->pardon(*player.id, BanKind::Player);
        }
    }

    std::vector<ManagedIpBan> ipBans() override {
        std::vector<ManagedIpBan> found;
        if (storage::AccessRepository* access = options().access; access != nullptr) {
            for (const BanRecord& record : access->bans(BanKind::Ip)) {
                found.push_back(ipBanOf(record));
            }
        }
        return found;
    }

    void banIp(const ManagedIpBan& ban) override {
        storage::AccessRepository* access = options().access;
        if (access == nullptr) {
            return;
        }
        const std::string reason{ban.reason.value_or(std::string{kDefaultReason})};
        access->ban(BanRecord{.subject = ban.ip,
                              .kind = BanKind::Ip,
                              .reason = reason,
                              .source = ban.source.value_or(std::string{kSource}),
                              .createdAtEpochSeconds = nowSeconds(),
                              .expiresAtEpochSeconds = ban.expiresEpochSeconds});
        for (player::PlayerSession* session : s_.players->snapshot()) {
            if (session->connection().address == ban.ip && session->connection().disconnect) {
                session->connection().disconnect(
                    Component::translatable("multiplayer.disconnect.banned_ip.reason", {Component::text(reason)}));
            }
        }
    }

    void unbanIp(std::string_view ip) override {
        if (storage::AccessRepository* access = options().access; access != nullptr) {
            access->pardon(ip, BanKind::Ip);
        }
    }

    std::optional<std::string> addressOf(const ManagedPlayer& player) override {
        for (const player::PlayerSession* session : s_.players->snapshot()) {
            if (player.id.has_value() && *player.id == session->uuid().toString()) {
                return session->connection().address;
            }
        }
        if (storage::PlayerRepository* players = options().playerRepository;
            players != nullptr && player.name.has_value()) {
            if (const std::optional<storage::StoredPlayer> stored = players->loadByName(*player.name);
                stored.has_value()) {
                return std::nullopt;
            }
        }
        return std::nullopt;
    }

    std::vector<ManagedOperator> operators() override {
        const std::string& group = groups().operatorGroup().name;
        std::map<std::string, ManagedPlayer> known;
        if (storage::PlayerRepository* players = options().playerRepository; players != nullptr) {
            for (const storage::PlayerSummary& summary : players->findInGroup(group)) {
                known[summary.uuid] = ManagedPlayer{summary.uuid, summary.name};
            }
        }
        for (const player::PlayerSession* session : s_.players->snapshot()) {
            if (session->group == group) {
                known[session->uuid().toString()] = ManagedPlayer{session->uuid().toString(), session->name()};
            } else {
                known.erase(session->uuid().toString());
            }
        }
        std::vector<ManagedOperator> found;
        for (const auto& [id, player] : known) {
            found.push_back(operatorOf(player));
        }
        return found;
    }

    void op(const ManagedPlayer& player, std::optional<std::int32_t>, std::optional<bool>) override {
        assign(player, groups().operatorGroup().name);
        trackOperators();
    }

    void deop(const ManagedPlayer& player) override {
        assign(player, groups().defaultGroup().name);
        trackOperators();
    }

    bool kick(const ManagedPlayer& player, const std::optional<ManagedMessage>& reason) override {
        for (player::PlayerSession* session : s_.players->snapshot()) {
            if (player.id.has_value() && *player.id == session->uuid().toString() && session->connection().disconnect) {
                session->connection().disconnect(reason.has_value()
                                                     ? componentOf(*reason)
                                                     : Component::translatable("multiplayer.disconnect.kicked"));
                return true;
            }
        }
        return false;
    }

    bool save(bool) override {
        if (s_.saveAll) {
            s_.saveAll();
        }
        return true;
    }

    void stop() override {
        if (s_.stop) {
            s_.stop();
        }
    }

    void heartbeat() override {
        if (s_.notifier != nullptr) {
            s_.notifier->heartbeat(ready(), onlinePlayers(), versionName(), protocolVersion());
        }
    }

    void poll() override {
        std::set<std::string> now;
        std::map<std::string, ManagedPlayer> byId;
        for (const ManagedPlayer& player : onlinePlayers()) {
            now.insert(*player.id);
            byId[*player.id] = player;
        }
        bool changed = false;
        if (known_) {
            for (const auto& [id, player] : byId) {
                if (!online_.contains(id)) {
                    changed = true;
                    s_.notifier->playerJoined(player);
                }
            }
            for (const auto& [id, player] : online_) {
                if (!byId.contains(id)) {
                    changed = true;
                    s_.notifier->playerLeft(player);
                }
            }
        }
        online_ = std::move(byId);
        if (changed) {
            s_.notifier->activity();
        }
        trackGameRules();
        trackOperators();
        known_ = true;
    }

    std::vector<ManagedGameRule> gameRules() override {
        std::vector<ManagedGameRule> found;
        const world::LoadedWorld* world = defaultWorld();
        for (const world::GameRule& rule : world::gameRules::all()) {
            found.push_back(ruleOf(rule, world));
        }
        return found;
    }

    std::optional<std::string> updateGameRule(const ManagedGameRule& rule) override {
        const world::GameRule* known = world::gameRules::byId(rule.key);
        if (known == nullptr) {
            return "Unknown game rule: " + rule.key;
        }
        const bool boolean = known->kind == world::GameRule::Kind::Boolean;
        if (boolean != rule.boolean) {
            return "Game rule " + rule.key + " is " + (boolean ? "a boolean" : "an integer");
        }
        if (!boolean && (rule.number < known->min || rule.number > known->max)) {
            return "Value for " + rule.key + " must be between " + std::to_string(known->min) + " and " +
                   std::to_string(known->max);
        }
        const std::string value = boolean ? (rule.flag ? "true" : "false") : std::to_string(rule.number);
        for (const auto& [name, world] : options().worlds) {
            world->gameRules().set(*known, value);
        }
        trackGameRules();
        return std::nullopt;
    }

    bool systemMessage(const ManagedMessage& message, bool overlay,
                       const std::optional<std::vector<ManagedPlayer>>& receivers) override {
        const Component text = componentOf(message);
        bool sent = false;
        for (player::PlayerSession* session : s_.players->snapshot()) {
            if (receivers.has_value() && std::ranges::none_of(*receivers, [session](const ManagedPlayer& wanted) {
                    return (wanted.id.has_value() && *wanted.id == session->uuid().toString()) ||
                           (!wanted.id.has_value() && wanted.name.has_value() &&
                            sameName(*wanted.name, session->name()));
                })) {
                continue;
            }
            if (overlay && session->connection().sendActionBar) {
                session->connection().sendActionBar(text);
            } else {
                session->sendSystemMessage(text);
            }
            sent = true;
        }
        return sent || (receivers.has_value() && receivers->empty());
    }

    std::optional<std::string> setting(std::string_view key) override {
        return s_.settings == nullptr ? std::nullopt : s_.settings->get(settingKey(key));
    }

    std::optional<std::string> changeSetting(std::string_view key, std::string_view value) override {
        if (s_.settings == nullptr || !s_.settings->set(settingKey(key), value)) {
            return "The server refused " + std::string{key} + " = " + std::string{value};
        }
        return std::nullopt;
    }

private:
    [[nodiscard]] const command::CommandEnvironment::Options& options() const {
        return s_.dispatcher->environment().options();
    }

    [[nodiscard]] const config::GroupsConfig& groups() const { return s_.dispatcher->environment().groups(); }

    [[nodiscard]] const world::LoadedWorld* defaultWorld() const {
        const auto found = options().worlds.find(options().defaultWorldName);
        if (found != options().worlds.end()) {
            return found->second;
        }
        return options().worlds.empty() ? nullptr : options().worlds.begin()->second;
    }

    [[nodiscard]] static ManagedGameRule ruleOf(const world::GameRule& rule, const world::LoadedWorld* world) {
        ManagedGameRule out;
        out.key = std::string{rule.id};
        out.boolean = rule.kind == world::GameRule::Kind::Boolean;
        if (world != nullptr) {
            out.flag = out.boolean && world->gameRules().get(rule);
            out.number = out.boolean ? 0 : world->gameRules().number(rule);
        } else {
            out.flag = rule.defaultFlag;
            out.number = rule.defaultNumber;
        }
        return out;
    }

    void trackOperators() {
        std::map<std::string, ManagedOperator> now;
        for (const ManagedOperator& entry : operators()) {
            now[*entry.player.id] = entry;
        }
        if (known_) {
            for (const auto& [id, entry] : now) {
                if (!ops_.contains(id)) {
                    s_.notifier->operatorAdded(entry);
                }
            }
            for (const auto& [id, entry] : ops_) {
                if (!now.contains(id)) {
                    s_.notifier->operatorRemoved(entry);
                }
            }
        }
        ops_ = std::move(now);
    }

    void trackGameRules() {
        std::map<std::string, std::string> now;
        const world::LoadedWorld* world = defaultWorld();
        for (const world::GameRule& rule : world::gameRules::all()) {
            now[std::string{rule.id}] = world == nullptr ? std::string{} : world->gameRules().serialized(rule);
        }
        if (known_) {
            for (const auto& [key, value] : now) {
                const auto before = rules_.find(key);
                if (before != rules_.end() && before->second != value) {
                    s_.notifier->gameRuleChanged(ruleOf(*world::gameRules::byId(key), world));
                }
            }
        }
        rules_ = std::move(now);
    }

    [[nodiscard]] static ManagedUserBan userBanOf(const BanRecord& record) {
        return ManagedUserBan{ManagedPlayer{record.subject, record.name}, record.reason, record.source,
                              record.expiresAtEpochSeconds};
    }

    [[nodiscard]] static ManagedIpBan ipBanOf(const BanRecord& record) {
        return ManagedIpBan{record.subject, record.reason, record.source, record.expiresAtEpochSeconds};
    }

    [[nodiscard]] ManagedOperator operatorOf(const ManagedPlayer& player) const {
        return ManagedOperator{player, command::permissionLevel::operatorLevel(), false};
    }

    void assign(const ManagedPlayer& player, const std::string& group) {
        storage::PlayerRepository* players = options().playerRepository;
        if (players == nullptr || !player.id.has_value()) {
            return;
        }
        players->setGroup(*player.id, group);
        for (player::PlayerSession* session : s_.players->snapshot()) {
            if (session->uuid().toString() == *player.id) {
                session->group = group;
                if (session->connection().sendCommandTree) {
                    session->connection().sendCommandTree();
                }
            }
        }
    }

    ManagementSources s_;
    std::map<std::string, ManagedPlayer> online_;
    std::map<std::string, std::string> rules_;
    std::map<std::string, ManagedOperator> ops_;
    bool known_ = false;
};

} // namespace

std::unique_ptr<server::remote::ManagementBackend> createManagement(ManagementSources sources) {
    return std::make_unique<Backend>(std::move(sources));
}

void observeAccess(ManagementSources sources) {
    server::remote::ManagementNotifier* notifier = sources.notifier;
    const command::CommandEnvironment::Options& options = sources.dispatcher->environment().options();
    if (storage::AccessRepository* access = options.access; access != nullptr) {
        access->observe([notifier](const storage::AccessChange& change) {
            using Type = storage::AccessChange::Type;
            const ManagedPlayer who{change.subject, change.name.empty() ? std::nullopt : std::optional{change.name}};
            switch (change.type) {
            case Type::WhitelistAdded:
                notifier->allowlistAdded(who);
                break;
            case Type::WhitelistRemoved:
                notifier->allowlistRemoved(who);
                break;
            case Type::Banned:
                if (change.kind == BanKind::Ip) {
                    notifier->ipBanned(ipBanOfChange(change));
                } else {
                    notifier->banned(userBanOfChange(change));
                }
                break;
            case Type::Pardoned:
                if (change.kind == BanKind::Ip) {
                    notifier->ipUnbanned(change.subject);
                } else {
                    notifier->unbanned(who);
                }
                break;
            }
        });
    }
}

} // namespace basalt::server::runtime::remoteHost
