#include <basalt/server/runtime/config/GroupsConfig.hpp>
#include <basalt/server/runtime/permission/ServerPermissions.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>

#include <array>
#include <memory>
#include <mutex>

namespace basalt::server::runtime::permission {
namespace {
namespace chainApi = wire::permissions;
namespace contract = server::plugins::permissions;

constexpr std::int32_t kMaxDeclaredDepth = 4;
constexpr std::size_t kUuidLength = 36;

struct Asking {
    const player::PlayerSession* player = nullptr;
    const config::GroupsConfig* groups = nullptr;
};

thread_local const Asking* tAsking = nullptr;

class AskingScope {
public:
    explicit AskingScope(const Asking& asking) noexcept : previous_(tAsking) { tAsking = &asking; }

    AskingScope(const AskingScope&) = delete;
    AskingScope& operator=(const AskingScope&) = delete;

    ~AskingScope() { tAsking = previous_; }

private:
    const Asking* previous_;
};

[[nodiscard]] std::array<char, kUuidLength> uuidText(const wire::common::Uuid& uuid) noexcept {
    constexpr std::string_view kHex = "0123456789abcdef";
    std::array<char, kUuidLength> text{};
    std::size_t at = 0;
    for (std::int32_t nibble = 0; nibble < 32; ++nibble) {
        if (nibble == 8 || nibble == 12 || nibble == 16 || nibble == 20) {
            text[at++] = '-';
        }
        const std::uint64_t half = nibble < 16 ? uuid.high() : uuid.low();
        const std::int32_t shift = 60 - ((nibble % 16) * 4);
        text[at++] = kHex[(half >> shift) & 0xFU];
    }
    return text;
}

[[nodiscard]] std::int32_t contractOf(chainApi::Tristate value) noexcept {
    switch (value) {
    case chainApi::Tristate::kTrue:
        return contract::kTrue;
    case chainApi::Tristate::kFalse:
        return contract::kFalse;
    case chainApi::Tristate::kUndefined:
        break;
    }
    return contract::kUndefined;
}

[[nodiscard]] chainApi::Tristate tristateOf(std::int32_t value) noexcept {
    if (value == contract::kTrue) {
        return chainApi::Tristate::kTrue;
    }
    return value == contract::kFalse ? chainApi::Tristate::kFalse : chainApi::Tristate::kUndefined;
}

[[nodiscard]] std::string_view uuidOf(const chainApi::Subject& subject) noexcept {
    return subject.isConsole() ? std::string_view{} : subject.uuid;
}
} // namespace

ServerPermissions::ServerPermissions(Options options) : options_(std::move(options)) {
    default_ =
        chain_.provide({.id = "groups",
                        .priority = chainApi::kPriorityDefault,
                        .check =
                            [this](const chainApi::Query& query) {
                                if (query.subject().isConsole()) {
                                    return chainApi::Tristate::kTrue;
                                }
                                if (const Asking* asking = tAsking; asking != nullptr) {
                                    return chainApi::tristateOf(answer(*asking->player, *asking->groups, query.node()));
                                }
                                const player::PlayerSession* player =
                                    options_.online ? options_.online(query.subject().uuid) : nullptr;
                                if (player == nullptr || !options_.groups) {
                                    return chainApi::Tristate::kUndefined;
                                }
                                return chainApi::tristateOf(answer(*player, options_.groups(), query.node()));
                            },
                        .meta = {}});
}

ServerPermissions::~ServerPermissions() { alive_->store(false, std::memory_order_release); }

bool ServerPermissions::plain() const noexcept {
    return chain_.onlyDefault() && !anyDeclared_.load(std::memory_order_acquire);
}

bool ServerPermissions::byDefault(const player::PlayerSession& player, const config::GroupsConfig& groups,
                                  std::string_view node) {
    return player.permissionGrant(node).value_or(groups.resolve(player.group, node));
}

bool ServerPermissions::has(const player::PlayerSession& player, const config::GroupsConfig& groups,
                            std::string_view node) const {
    if (plain()) {
        return byDefault(player, groups, node);
    }
    return check(player, groups, node) == chainApi::Tristate::kTrue;
}

chainApi::Tristate ServerPermissions::check(const player::PlayerSession& player, const config::GroupsConfig& groups,
                                            std::string_view node) const {
    const Asking asking{.player = &player, .groups = &groups};
    const AskingScope scope{asking};
    const std::array<char, kUuidLength> uuid = uuidText(player.uuid());
    const chainApi::Context context{.server = {}, .world = player.worldName, .extra = {}};
    return chain_.check(chainApi::Subject::player({uuid.data(), uuid.size()}, player.name()), node, context);
}

bool ServerPermissions::answer(const player::PlayerSession& player, const config::GroupsConfig& groups,
                               std::string_view node) const {
    if (!anyDeclared_.load(std::memory_order_acquire)) {
        return byDefault(player, groups, node);
    }
    const std::shared_lock<std::shared_mutex> reading{declarationsMutex_};
    return declared(player, groups, node, 0).value_or(groups.resolve(player.group, node));
}

std::optional<bool> ServerPermissions::declared(const player::PlayerSession& player, const config::GroupsConfig& groups,
                                                std::string_view node, std::int32_t depth) const {
    if (const std::optional<bool> granted = player.permissionGrant(node); granted.has_value()) {
        return granted;
    }
    // parity: Bukkit defaults give way to a rule that names the node, but not to a bare `*`.
    if (const std::optional<bool> named = groups.decide(player.group, node, 1); named.has_value()) {
        return named;
    }
    if (depth < kMaxDeclaredDepth) {
        if (const auto parents = parents_.find(node); parents != parents_.end()) {
            for (const auto& [parent, value] : parents->second) {
                if (const std::optional<bool> held = declared(player, groups, parent, depth + 1); held.has_value()) {
                    return *held ? value : !value;
                }
            }
        }
    }
    const auto found = declarations_.find(node);
    if (found == declarations_.end()) {
        return std::nullopt;
    }
    const bool op = groups.resolve(player.group, "*");
    switch (found->second.defaultKind) {
    case contract::kDefaultTrue:
        return true;
    case contract::kDefaultFalse:
        return false;
    case contract::kDefaultNotOp:
        return !op;
    default:
        return op;
    }
}

server::plugins::PluginHandle ServerPermissions::provide(std::string_view id, std::int32_t priority,
                                                         contract::Check check, contract::Meta meta) {
    chainApi::Provider provider{.id = std::string{id}, .priority = priority, .check = {}, .meta = {}};
    if (check) {
        provider.check = [check = std::move(check)](const chainApi::Query& query) {
            return tristateOf(check(uuidOf(query.subject()), query.node()));
        };
    }
    if (meta) {
        provider.meta = [meta = std::move(meta)](const chainApi::Subject& subject, std::string_view key,
                                                 const chainApi::Context&) { return meta(uuidOf(subject), key); };
    }
    auto handle = std::make_shared<chainApi::ProviderHandle>(chain_.provide(std::move(provider)));
    changed();
    return server::plugins::PluginHandle{[this, handle, alive = alive_] {
        handle->remove();
        if (alive->load(std::memory_order_acquire)) {
            changed();
        }
    }};
}

void ServerPermissions::declare(std::string_view node, std::int32_t defaultKind,
                                std::span<const std::string_view> childNodes, std::span<const bool> childValues) {
    {
        const std::unique_lock<std::shared_mutex> writing{declarationsMutex_};
        Declared declaration{.defaultKind = defaultKind, .children = {}};
        for (std::size_t at = 0; at < childNodes.size(); ++at) {
            const bool value = at < childValues.size() ? childValues[at] : true;
            declaration.children.emplace_back(std::string{childNodes[at]}, value);
        }
        const std::string key{node};
        for (auto& [child, parents] : parents_) {
            std::erase_if(parents, [&key](const auto& entry) { return entry.first == key; });
        }
        for (const auto& [child, value] : declaration.children) {
            parents_[child].emplace_back(key, value);
        }
        declarations_.insert_or_assign(key, std::move(declaration));
        anyDeclared_.store(true, std::memory_order_release);
    }
    changed();
}

void ServerPermissions::undeclare(std::string_view node) {
    {
        const std::unique_lock<std::shared_mutex> writing{declarationsMutex_};
        if (const auto found = declarations_.find(node); found != declarations_.end()) {
            declarations_.erase(found);
        }
        for (auto& [child, parents] : parents_) {
            std::erase_if(parents, [node](const auto& entry) { return entry.first == node; });
        }
        std::erase_if(parents_, [](const auto& entry) { return entry.second.empty(); });
        anyDeclared_.store(!declarations_.empty(), std::memory_order_release);
    }
    changed();
}

std::int32_t ServerPermissions::check(std::string_view uuid, std::string_view node) {
    if (uuid.empty()) {
        return contractOf(chain_.check(chainApi::Subject::console(), node));
    }
    const player::PlayerSession* player = options_.online ? options_.online(uuid) : nullptr;
    if (player == nullptr || !options_.groups) {
        return contract::kUndefined;
    }
    return contractOf(check(*player, options_.groups(), node));
}

std::optional<std::string> ServerPermissions::meta(std::string_view uuid, std::string_view key) {
    if (uuid.empty()) {
        return chain_.meta(chainApi::Subject::console(), key);
    }
    const player::PlayerSession* player = options_.online ? options_.online(uuid) : nullptr;
    if (player == nullptr) {
        return std::nullopt;
    }
    const std::array<char, kUuidLength> text = uuidText(player->uuid());
    const chainApi::Context context{.server = {}, .world = player->worldName, .extra = {}};
    return chain_.meta(chainApi::Subject::player({text.data(), text.size()}, player->name()), key, context);
}

void ServerPermissions::changed() {
    chain_.changed();
    if (options_.changed) {
        options_.changed();
    }
}

bool has(const player::PlayerSession& player, const config::GroupsConfig& groups, std::string_view node) {
    return player.permissions == nullptr ? ServerPermissions::byDefault(player, groups, node)
                                         : player.permissions->has(player, groups, node);
}

} // namespace basalt::server::runtime::permission
