#include <basalt/server/runtime/command/CommandSource.hpp>
#include <basalt/wire/text/TranslationResolver.hpp>

#include <atomic>
#include <cstdint>
#include <utility>

namespace basalt::server::runtime::command {

namespace {
std::atomic<std::int32_t> gOperatorLevel{permissionLevel::kOwners};
} // namespace

std::int32_t permissionLevel::operatorLevel() noexcept { return gOperatorLevel.load(std::memory_order_relaxed); }

void permissionLevel::setOperatorLevel(std::int32_t level) noexcept {
    gOperatorLevel.store(level, std::memory_order_relaxed);
}

ConsoleCommandSource::ConsoleCommandSource(wire::common::Logger logger, wire::text::LangTable lang, SpawnAt spawn)
    : logger_(std::move(logger)), lang_(std::move(lang)), spawn_(std::move(spawn)) {}

wire::text::Component ConsoleCommandSource::displayName() const { return wire::text::Component::text("Server"); }

std::array<double, 3> ConsoleCommandSource::position() const {
    return spawn_ ? spawn_() : std::array<double, 3>{0.0, 0.0, 0.0};
}

void ConsoleCommandSource::sendSystemMessage(const wire::text::Component& message) const {
    logger_.info([&] { return wire::text::resolvePlainText(message, lang_); });
}

bool ConsoleCommandSource::hasPermission(std::optional<std::string_view> node) const {
    static_cast<void>(node);
    return true;
}

PlayerCommandSource::PlayerCommandSource(player::PlayerSession& player, const config::GroupsConfig& groups,
                                         const chat::SignedArguments* signedArguments)
    : player_(&player), groups_(&groups), signedArguments_(signedArguments) {}

std::string PlayerCommandSource::name() const { return player_->name(); }

wire::text::Component PlayerCommandSource::displayName() const { return wire::text::Component::text(player_->name()); }

std::int32_t PlayerCommandSource::permissionLevel() const {
    const std::optional<bool> granted = player_->permissionGrant(kAdminPermission);
    return granted.value_or(groups_->resolve(player_->group, kAdminPermission)) ? permissionLevel::operatorLevel()
                                                                                : permissionLevel::kAll;
}

std::array<double, 3> PlayerCommandSource::position() const { return {player_->x, player_->y, player_->z}; }

std::pair<float, float> PlayerCommandSource::rotation() const { return {player_->yaw, player_->pitch}; }

void PlayerCommandSource::sendSystemMessage(const wire::text::Component& message) const {
    player_->sendSystemMessage(message);
}

bool PlayerCommandSource::hasPermission(std::optional<std::string_view> node) const {
    if (!node.has_value()) {
        return false;
    }
    return player_->permissionGrant(*node).value_or(groups_->resolve(player_->group, *node));
}

CommandBlockSource::CommandBlockSource(std::array<double, 3> at, std::string name, bool speaks,
                                       wire::common::Logger logger, wire::text::LangTable lang, std::int32_t level)
    : level_(level), at_(at), name_(std::move(name)), speaks_(speaks), logger_(std::move(logger)),
      lang_(std::move(lang)) {}

wire::text::Component CommandBlockSource::displayName() const { return wire::text::Component::text(name_); }

void CommandBlockSource::sendSystemMessage(const wire::text::Component& message) const {
    if (!speaks_) {
        return;
    }
    logger_.info([&] { return wire::text::resolvePlainText(message, lang_); });
}

bool CommandBlockSource::hasPermission(std::optional<std::string_view> node) const {
    static_cast<void>(node);
    return true;
}

} // namespace basalt::server::runtime::command
