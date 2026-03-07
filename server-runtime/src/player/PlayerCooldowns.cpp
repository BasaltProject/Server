#include <basalt/server/registry/Items.hpp>
#include <basalt/server/runtime/player/PlayerCooldowns.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>

#include <string>

namespace basalt::server::runtime::player {

bool onCooldownFor(PlayerSession& session, const wire::common::Identifier& item, const std::int64_t gameTime) {
    const auto found = session.itemCooldowns.find(item.path());
    if (found == session.itemCooldowns.end()) {
        return false;
    }
    if (found->second > gameTime) {
        return true;
    }
    session.itemCooldowns.erase(found);
    return false;
}

void startCooldownFor(PlayerSession& session, const wire::common::Identifier& item, const std::int64_t gameTime) {
    const std::int32_t ticks = server::registry::itemStacking::cooldownTicks(item);
    if (ticks <= 0) {
        return;
    }
    session.itemCooldowns[std::string{item.path()}] = gameTime + ticks;
    if (session.connection().startItemCooldown) {
        session.connection().startItemCooldown(item, ticks);
    }
}

} // namespace basalt::server::runtime::player
