#include <basalt/server/runtime/player/PlayerRegistry.hpp>

#include <algorithm>

namespace basalt::server::runtime::player {

std::int32_t PlayerRegistry::allocateEntityId() {
    const std::lock_guard<std::mutex> guard{lock_};
    return nextEntityId_++;
}

void PlayerRegistry::add(PlayerSession* session) {
    if (session == nullptr) {
        return;
    }
    const std::lock_guard<std::mutex> guard{lock_};
    const auto found = sessions_.find(session->uuid());
    if (found != sessions_.end()) {
        std::erase(order_, found->second);
        found->second = session;
    } else {
        sessions_.emplace(session->uuid(), session);
    }
    order_.push_back(session);
}

void PlayerRegistry::remove(const wire::common::Uuid& uuid) {
    const std::lock_guard<std::mutex> guard{lock_};
    const auto found = sessions_.find(uuid);
    if (found == sessions_.end()) {
        return;
    }
    std::erase(order_, found->second);
    sessions_.erase(found);
}

PlayerSession* PlayerRegistry::get(const wire::common::Uuid& uuid) const {
    const std::lock_guard<std::mutex> guard{lock_};
    const auto found = sessions_.find(uuid);
    return found == sessions_.end() ? nullptr : found->second;
}

std::vector<PlayerSession*> PlayerRegistry::snapshot() const {
    const std::lock_guard<std::mutex> guard{lock_};
    return order_;
}

} // namespace basalt::server::runtime::player
