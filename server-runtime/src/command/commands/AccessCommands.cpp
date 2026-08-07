#include <basalt/server/runtime/command/ArgumentTypes.hpp>
#include <basalt/server/runtime/command/commands/AccessCommands.hpp>
#include <basalt/server/storage/AccessRepository.hpp>
#include <basalt/server/storage/PlayerRepository.hpp>

#include <algorithm>
#include <cstdint>
#include <set>
#include <string>
#include <vector>

namespace basalt::server::runtime::command::commands {
namespace {

using storage::BanKind;
using storage::BanRecord;
using wire::text::Component;

constexpr std::string_view kDefaultReason = "Banned by an operator.";

[[nodiscard]] bool sameName(std::string_view left, std::string_view right) {
    return std::ranges::equal(left, right, [](char a, char b) {
        return static_cast<char>(std::tolower(static_cast<unsigned char>(a))) ==
               static_cast<char>(std::tolower(static_cast<unsigned char>(b)));
    });
}

[[nodiscard]] std::expected<storage::AccessRepository*, CommandSyntaxError> accessOf(const CommandContext& context,
                                                                                     std::string_view verb) {
    storage::AccessRepository* access = context.environment().options().access;
    if (access == nullptr) {
        return std::unexpected{CommandSyntaxError{
            .component = Component::text("This server has no player database to " + std::string{verb} + ".")}};
    }
    return access;
}

[[nodiscard]] player::PlayerSession* onlineNamed(const CommandContext& context, std::string_view name) {
    for (player::PlayerSession* player : context.environment().players().snapshot()) {
        if (sameName(player->name(), name)) {
            return player;
        }
    }
    return nullptr;
}

/// A name nobody has played under still resolves: the UUID is the offline one, as vanilla does.
[[nodiscard]] std::string uuidOf(const CommandContext& context, const std::string& name) {
    if (const player::PlayerSession* online = onlineNamed(context, name); online != nullptr) {
        return online->uuid().toString();
    }
    if (storage::PlayerRepository* repository = context.environment().options().playerRepository;
        repository != nullptr) {
        if (const std::optional<storage::StoredPlayer> stored = repository->loadByName(name); stored.has_value()) {
            return stored->uuid;
        }
    }
    return wire::common::Uuid::offlinePlayer(name).toString();
}

[[nodiscard]] std::string reasonOf(const CommandContext& context, bool given) {
    const auto* reason = given ? context.get<std::string>("reason") : nullptr;
    return reason == nullptr ? std::string{kDefaultReason} : *reason;
}

/// parity: `BanIpCommands.banIp` asks Guava's `InetAddresses.isInetAddress` whether the argument is an address or a.
[[nodiscard]] bool looksLikeAddress(std::string_view text) {
    std::int32_t parts = 0;
    std::size_t at = 0;
    while (at <= text.size()) {
        const std::size_t dot = text.find('.', at);
        const std::string_view part = text.substr(at, dot == std::string_view::npos ? text.npos : dot - at);
        if (part.empty() || part.size() > 3 ||
            !std::ranges::all_of(part, [](char c) { return std::isdigit(static_cast<unsigned char>(c)) != 0; })) {
            return false;
        }
        if (std::stoi(std::string{part}) > 255) {
            return false;
        }
        parts++;
        if (dot == std::string_view::npos) {
            break;
        }
        at = dot + 1;
    }
    return parts == 4;
}

} // namespace

BanCommand::BanCommand(std::function<std::int64_t()> nowEpochSeconds)
    : Command("ban", permissionLevel::kAdmins), nowEpochSeconds_(std::move(nowEpochSeconds)) {}

void BanCommand::build(const CommandNodeBuilder& root) const {
    const auto ban = [this](const CommandContext& context, bool reasoned) -> Parsed<std::int32_t> {
        const auto access = accessOf(context, "ban into");
        if (!access.has_value()) {
            return std::unexpected{access.error()};
        }
        const auto* name = context.get<std::string>("targets");
        if (name == nullptr) {
            return 0;
        }
        player::PlayerSession* online = onlineNamed(context, *name);
        const std::string uuid = uuidOf(context, *name);
        if ((*access)->banOf(uuid, BanKind::Player).has_value()) {
            return std::unexpected{CommandSyntaxError{.component = Component::translatable("commands.ban.failed")}};
        }
        const std::string text = reasonOf(context, reasoned);
        (*access)->ban(BanRecord{.subject = uuid,
                                 .kind = BanKind::Player,
                                 .name = online == nullptr ? *name : online->name(),
                                 .reason = text,
                                 .source = context.source().name(),
                                 .createdAtEpochSeconds = nowEpochSeconds_ ? nowEpochSeconds_() : 0});
        if (online != nullptr && online->connection().disconnect) {
            online->connection().disconnect(
                Component::translatable("multiplayer.disconnect.banned.reason", {Component::text(text)}));
        }
        context.success(
            Component::translatable("commands.ban.success", {Component::text(*name), Component::text(text)}), true);
        return 1;
    };
    root.argument("targets", gameProfileArgument(), [ban](const CommandNodeBuilder& targets) {
        targets.executes([ban](const CommandContext& context) { return ban(context, false); });
        targets.argument("reason", messageArgument(), [ban](const CommandNodeBuilder& reason) {
            reason.executes([ban](const CommandContext& context) { return ban(context, true); });
        });
    });
}

BanIpCommand::BanIpCommand(std::function<std::int64_t()> nowEpochSeconds)
    : Command("ban-ip", permissionLevel::kAdmins), nowEpochSeconds_(std::move(nowEpochSeconds)) {}

void BanIpCommand::build(const CommandNodeBuilder& root) const {
    const auto ban = [this](const CommandContext& context, bool reasoned) -> Parsed<std::int32_t> {
        const auto access = accessOf(context, "ban into");
        if (!access.has_value()) {
            return std::unexpected{access.error()};
        }
        const auto* target = context.get<std::string>("target");
        if (target == nullptr) {
            return 0;
        }
        const std::vector<player::PlayerSession*> online = context.environment().players().snapshot();
        std::string address;
        if (looksLikeAddress(*target)) {
            address = *target;
        } else if (const player::PlayerSession* named = onlineNamed(context, *target);
                   named != nullptr && named->connection().address.has_value()) {
            address = *named->connection().address;
        } else {
            return std::unexpected{CommandSyntaxError{.component = Component::translatable("commands.banip.invalid")}};
        }
        if ((*access)->banOf(address, BanKind::Ip).has_value()) {
            return std::unexpected{CommandSyntaxError{.component = Component::translatable("commands.banip.failed")}};
        }
        const std::string text = reasonOf(context, reasoned);
        (*access)->ban(BanRecord{.subject = address,
                                 .kind = BanKind::Ip,
                                 .reason = text,
                                 .source = context.source().name(),
                                 .createdAtEpochSeconds = nowEpochSeconds_ ? nowEpochSeconds_() : 0});
        std::int32_t affected = 0;
        for (const player::PlayerSession* player : online) {
            if (player->connection().address == address) {
                affected++;
                if (player->connection().disconnect) {
                    player->connection().disconnect(
                        Component::translatable("multiplayer.disconnect.banned_ip.reason", {Component::text(text)}));
                }
            }
        }
        context.success(
            Component::translatable("commands.banip.success", {Component::text(address), Component::text(text)}), true);
        return affected + 1;
    };
    root.argument("target", wordArgument(), [ban](const CommandNodeBuilder& target) {
        target.executes([ban](const CommandContext& context) { return ban(context, false); });
        target.argument("reason", messageArgument(), [ban](const CommandNodeBuilder& reason) {
            reason.executes([ban](const CommandContext& context) { return ban(context, true); });
        });
    });
}

PardonCommand::PardonCommand(bool ip) : Command(ip ? "pardon-ip" : "pardon", permissionLevel::kAdmins), ip_(ip) {}

void PardonCommand::build(const CommandNodeBuilder& root) const {
    const bool ip = ip_;
    const std::string name = ip ? "target" : "targets";
    root.argument(name, ip ? wordArgument() : gameProfileArgument(), [ip, name](const CommandNodeBuilder& target) {
        // parity: `PardonCommand` completes with who is actually banned, not with who is online.
        target.suggestsFromServer([ip](const SuggestionContext& context) {
            std::vector<std::string> banned;
            if (storage::AccessRepository* access = context.environment().options().access; access != nullptr) {
                for (const BanRecord& record : access->bans(ip ? BanKind::Ip : BanKind::Player)) {
                    if (ip) {
                        banned.push_back(record.subject);
                    } else if (record.name.has_value()) {
                        banned.push_back(*record.name);
                    }
                }
            }
            return context.matching(banned);
        });
        target.executes([ip, name](const CommandContext& context) -> Parsed<std::int32_t> {
            const auto access = accessOf(context, "pardon from");
            if (!access.has_value()) {
                return std::unexpected{access.error()};
            }
            const auto* asked = context.get<std::string>(name);
            if (asked == nullptr) {
                return 0;
            }
            const std::string subject = ip ? *asked : uuidOf(context, *asked);
            if (!(*access)->pardon(subject, ip ? BanKind::Ip : BanKind::Player)) {
                return std::unexpected{CommandSyntaxError{
                    .component = Component::translatable(ip ? "commands.pardonip.failed" : "commands.pardon.failed")}};
            }
            context.success(Component::translatable(ip ? "commands.pardonip.success" : "commands.pardon.success",
                                                    {Component::text(*asked)}),
                            true);
            return 1;
        });
    });
}

BanListCommand::BanListCommand() : Command("banlist", permissionLevel::kAdmins) {}

void BanListCommand::build(const CommandNodeBuilder& root) const {
    const auto list = [](const CommandContext& context, std::optional<BanKind> kind) -> Parsed<std::int32_t> {
        const auto access = accessOf(context, "read bans from");
        if (!access.has_value()) {
            return std::unexpected{access.error()};
        }
        std::vector<BanRecord> bans;
        if (kind.has_value()) {
            bans = (*access)->bans(*kind);
        } else {
            bans = (*access)->bans(BanKind::Player);
            for (BanRecord& record : (*access)->bans(BanKind::Ip)) {
                bans.push_back(std::move(record));
            }
        }
        if (bans.empty()) {
            context.success(Component::translatable("commands.banlist.none"));
            return 0;
        }
        context.success(
            Component::translatable("commands.banlist.list", {Component::text(std::to_string(bans.size()))}));
        for (const BanRecord& record : bans) {
            context.success(Component::translatable("commands.banlist.entry",
                                                    {Component::text(record.name.value_or(record.subject)),
                                                     Component::text(record.source), Component::text(record.reason)}));
        }
        return static_cast<std::int32_t>(bans.size());
    };
    root.executes([list](const CommandContext& context) { return list(context, std::nullopt); });
    root.literal("players", [list](const CommandNodeBuilder& form) {
        form.executes([list](const CommandContext& context) { return list(context, BanKind::Player); });
    });
    root.literal("ips", [list](const CommandNodeBuilder& form) {
        form.executes([list](const CommandContext& context) { return list(context, BanKind::Ip); });
    });
}

WhitelistCommand::WhitelistCommand(std::function<bool()> enforced, std::function<void(bool)> setEnforced,
                                   std::function<std::int64_t()> nowEpochSeconds, std::function<void()> kickUnlisted)
    : Command("whitelist", permissionLevel::kAdmins), kickUnlisted_(std::move(kickUnlisted)),
      enforced_(std::move(enforced)), setEnforced_(std::move(setEnforced)),
      nowEpochSeconds_(std::move(nowEpochSeconds)) {}

void WhitelistCommand::build(const CommandNodeBuilder& root) const {
    for (const bool on : {true, false}) {
        root.literal(on ? "on" : "off", [this, on](const CommandNodeBuilder& form) {
            form.executes([this, on](const CommandContext& context) -> Parsed<std::int32_t> {
                if ((enforced_ && enforced_()) == on) {
                    return std::unexpected{
                        CommandSyntaxError{.component = Component::translatable(on ? "commands.whitelist.alreadyOn"
                                                                                   : "commands.whitelist.alreadyOff")}};
                }
                if (setEnforced_) {
                    setEnforced_(on);
                }
                context.success(
                    Component::translatable(on ? "commands.whitelist.enabled" : "commands.whitelist.disabled"), true);
                if (on && kickUnlisted_) {
                    kickUnlisted_();
                }
                return 1;
            });
        });
    }

    root.literal("list", [](const CommandNodeBuilder& form) {
        form.executes([](const CommandContext& context) -> Parsed<std::int32_t> {
            const auto access = accessOf(context, "whitelist with");
            if (!access.has_value()) {
                return std::unexpected{access.error()};
            }
            const std::vector<storage::WhitelistEntry> entries = (*access)->whitelist();
            std::string named;
            for (const storage::WhitelistEntry& entry : entries) {
                named += named.empty() ? entry.name : ", " + entry.name;
            }
            context.success(entries.empty() ? Component::translatable("commands.whitelist.none")
                                            : Component::translatable("commands.whitelist.list",
                                                                      {Component::text(std::to_string(entries.size())),
                                                                       Component::text(named)}));
            return static_cast<std::int32_t>(entries.size());
        });
    });

    root.literal("add", [this](const CommandNodeBuilder& form) {
        form.argument("targets", gameProfileArgument(), [this](const CommandNodeBuilder& targets) {
            targets.suggestsFromServer([](const SuggestionContext& context) {
                std::set<std::string> listed;
                if (storage::AccessRepository* access = context.environment().options().access; access != nullptr) {
                    for (const storage::WhitelistEntry& entry : access->whitelist()) {
                        listed.insert(entry.name);
                    }
                }
                std::vector<std::string> candidates;
                for (const player::PlayerSession* player : context.environment().players().snapshot()) {
                    if (!listed.contains(player->name())) {
                        candidates.push_back(player->name());
                    }
                }
                return context.matching(candidates);
            });
            targets.executes([this](const CommandContext& context) -> Parsed<std::int32_t> {
                const auto access = accessOf(context, "whitelist with");
                if (!access.has_value()) {
                    return std::unexpected{access.error()};
                }
                const auto* name = context.get<std::string>("targets");
                if (name == nullptr) {
                    return 0;
                }
                if ((*access)->whitelistedByName(*name).has_value()) {
                    return std::unexpected{
                        CommandSyntaxError{.component = Component::translatable("commands.whitelist.add.failed")}};
                }
                (*access)->addToWhitelist(
                    storage::WhitelistEntry{.uuid = uuidOf(context, *name),
                                            .name = *name,
                                            .addedAtEpochSeconds = nowEpochSeconds_ ? nowEpochSeconds_() : 0});
                context.success(Component::translatable("commands.whitelist.add.success", {Component::text(*name)}),
                                true);
                return 1;
            });
        });
    });

    root.literal("remove", [this](const CommandNodeBuilder& form) {
        form.argument("targets", gameProfileArgument(), [this](const CommandNodeBuilder& targets) {
            targets.suggestsFromServer([](const SuggestionContext& context) {
                std::vector<std::string> listed;
                if (storage::AccessRepository* access = context.environment().options().access; access != nullptr) {
                    for (const storage::WhitelistEntry& entry : access->whitelist()) {
                        listed.push_back(entry.name);
                    }
                }
                return context.matching(listed);
            });
            targets.executes([this](const CommandContext& context) -> Parsed<std::int32_t> {
                const auto access = accessOf(context, "whitelist with");
                if (!access.has_value()) {
                    return std::unexpected{access.error()};
                }
                const auto* name = context.get<std::string>("targets");
                const std::optional<storage::WhitelistEntry> entry =
                    name == nullptr ? std::nullopt : (*access)->whitelistedByName(*name);
                if (!entry.has_value()) {
                    return std::unexpected{
                        CommandSyntaxError{.component = Component::translatable("commands.whitelist.remove.failed")}};
                }
                static_cast<void>((*access)->removeFromWhitelist(entry->uuid));
                context.success(Component::translatable("commands.whitelist.remove.success", {Component::text(*name)}),
                                true);
                if (kickUnlisted_) {
                    kickUnlisted_();
                }
                return 1;
            });
        });
    });

    root.literal("reload", [](const CommandNodeBuilder& form) {
        form.executes([](const CommandContext& context) -> Parsed<std::int32_t> {
            context.success(Component::translatable("commands.whitelist.reloaded"));
            return 1;
        });
    });
}

} // namespace basalt::server::runtime::command::commands
