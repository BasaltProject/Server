#include <basalt/server/runtime/command/CommandTarget.hpp>

#include <cstdint>
#include <utility>

namespace basalt::server::runtime::command {
namespace {

using wire::common::Identifier;

[[nodiscard]] const Identifier& playerType() {
    static const Identifier type = Identifier::vanilla("player");
    return type;
}

} // namespace

const Identifier& CommandTarget::itemType() {
    static const Identifier type = Identifier::vanilla("item");
    return type;
}

CommandTarget::CommandTarget(player::PlayerSession& session) : what_(&session), worldName_(session.worldName) {}

CommandTarget::CommandTarget(entity::ServerEntity& entity, std::string worldName)
    : what_(&entity), worldName_(std::move(worldName)) {}

CommandTarget::CommandTarget(const world::DroppedItem& item, std::string worldName)
    : what_(std::make_shared<const world::DroppedItem>(item)), worldName_(std::move(worldName)) {}

player::PlayerSession* CommandTarget::player() const {
    player::PlayerSession* const* session = std::get_if<player::PlayerSession*>(&what_);
    return session == nullptr ? nullptr : *session;
}

entity::ServerEntity* CommandTarget::entity() const {
    entity::ServerEntity* const* found = std::get_if<entity::ServerEntity*>(&what_);
    return found == nullptr ? nullptr : *found;
}

const world::DroppedItem* CommandTarget::item() const {
    const auto* found = std::get_if<std::shared_ptr<const world::DroppedItem>>(&what_);
    return found == nullptr ? nullptr : found->get();
}

std::string CommandTarget::name() const {
    if (player::PlayerSession* session = player(); session != nullptr) {
        return session->name();
    }
    if (entity::ServerEntity* found = entity(); found != nullptr) {
        return found->customName.has_value() ? *found->customName : std::string{found->type().path()};
    }
    return std::string{std::get<std::shared_ptr<const world::DroppedItem>>(what_)->item.path()};
}

std::int32_t CommandTarget::entityId() const {
    if (player::PlayerSession* session = player(); session != nullptr) {
        return session->entityId();
    }
    if (entity::ServerEntity* found = entity(); found != nullptr) {
        return found->entityId();
    }
    return std::get<std::shared_ptr<const world::DroppedItem>>(what_)->entityId;
}

std::string CommandTarget::uuid() const {
    if (player::PlayerSession* session = player(); session != nullptr) {
        return session->uuid().toString();
    }
    if (entity::ServerEntity* found = entity(); found != nullptr) {
        return found->uuid().toString();
    }
    return std::get<std::shared_ptr<const world::DroppedItem>>(what_)->uuid.toString();
}

Identifier CommandTarget::type() const {
    if (player() != nullptr) {
        return playerType();
    }
    if (entity::ServerEntity* found = entity(); found != nullptr) {
        return found->type();
    }
    return itemType();
}

const std::string& CommandTarget::worldName() const {
    if (player::PlayerSession* session = player(); session != nullptr) {
        return session->worldName;
    }
    return worldName_;
}

double CommandTarget::x() const {
    if (player::PlayerSession* session = player(); session != nullptr) {
        return session->x;
    }
    if (entity::ServerEntity* found = entity(); found != nullptr) {
        return found->x;
    }
    return std::get<std::shared_ptr<const world::DroppedItem>>(what_)->x;
}

double CommandTarget::y() const {
    if (player::PlayerSession* session = player(); session != nullptr) {
        return session->y;
    }
    if (entity::ServerEntity* found = entity(); found != nullptr) {
        return found->y;
    }
    return std::get<std::shared_ptr<const world::DroppedItem>>(what_)->y;
}

double CommandTarget::z() const {
    if (player::PlayerSession* session = player(); session != nullptr) {
        return session->z;
    }
    if (entity::ServerEntity* found = entity(); found != nullptr) {
        return found->z;
    }
    return std::get<std::shared_ptr<const world::DroppedItem>>(what_)->z;
}

float CommandTarget::yaw() const {
    if (player::PlayerSession* session = player(); session != nullptr) {
        return session->yaw;
    }
    entity::ServerEntity* found = entity();
    return found == nullptr ? 0.0F : found->yaw;
}

float CommandTarget::pitch() const {
    if (player::PlayerSession* session = player(); session != nullptr) {
        return session->pitch;
    }
    entity::ServerEntity* found = entity();
    return found == nullptr ? 0.0F : found->pitch;
}

std::set<std::string> CommandTarget::tags() const {
    if (player::PlayerSession* session = player(); session != nullptr) {
        return {session->tags.begin(), session->tags.end()};
    }
    if (entity::ServerEntity* found = entity(); found != nullptr) {
        return {found->tags.begin(), found->tags.end()};
    }
    return {};
}

bool CommandTarget::alive() const {
    if (player::PlayerSession* session = player(); session != nullptr) {
        return !session->dead;
    }
    if (entity::ServerEntity* found = entity(); found != nullptr) {
        return !found->removed && found->health > 0.0;
    }
    return true;
}

std::string CommandTarget::scoreboardName() const {
    player::PlayerSession* session = player();
    return session == nullptr ? uuid() : session->name();
}

double CommandTarget::width() const {
    if (player() != nullptr) {
        return kPlayerWidth;
    }
    if (entity::ServerEntity* found = entity(); found != nullptr) {
        return found->width();
    }
    return kItemWidth;
}

double CommandTarget::height() const {
    if (player() != nullptr) {
        return kPlayerHeight;
    }
    if (entity::ServerEntity* found = entity(); found != nullptr) {
        return found->height();
    }
    return kItemHeight;
}

} // namespace basalt::server::runtime::command
