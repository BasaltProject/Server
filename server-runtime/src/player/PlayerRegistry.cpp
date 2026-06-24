#include <basalt/server/runtime/player/PlayerRegistry.hpp>

#include <algorithm>
#include <cstdint>

namespace basalt::server::runtime::player {

std::int32_t PlayerRegistry::allocateEntityId() {
    const std::lock_guard<std::mutex> guard{lock_};
    return nextEntityId_++;
}

std::int32_t PlayerRegistry::allocateEntityIds(const std::int32_t count) {
    const std::lock_guard<std::mutex> guard{lock_};
    const std::int32_t first = nextEntityId_;
    nextEntityId_ += std::max(count, 1);
    return first;
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

void PlayerRegistry::retire(std::unique_ptr<PlayerSession> session) {
    if (session == nullptr) {
        return;
    }
    const std::lock_guard<std::mutex> guard{lock_};
    retired_.emplace_back(std::chrono::steady_clock::now(), std::move(session));
}

void PlayerRegistry::keepAlive(std::shared_ptr<void> owner) {
    if (owner == nullptr) {
        return;
    }
    const std::lock_guard<std::mutex> guard{lock_};
    kept_.emplace_back(std::chrono::steady_clock::now(), std::move(owner));
}

void PlayerRegistry::collect() {
    const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    // Moved out and dropped outside the lock: a session's destructor may take as long as it likes.
    std::vector<std::unique_ptr<PlayerSession>> letGo;
    std::vector<std::shared_ptr<void>> closed;
    {
        const std::lock_guard<std::mutex> guard{lock_};
        for (auto& gone : retired_) {
            if (now - gone.first > kRetirement) {
                letGo.push_back(std::move(gone.second));
            }
        }
        std::erase_if(retired_, [](const auto& gone) { return gone.second == nullptr; });
        for (auto& gone : kept_) {
            if (now - gone.first > kRetirement) {
                closed.push_back(std::move(gone.second));
            }
        }
        std::erase_if(kept_, [](const auto& gone) { return gone.second == nullptr; });
    }
}

PlayerSession* PlayerRegistry::get(const wire::common::Uuid& uuid) const {
    const std::lock_guard<std::mutex> guard{lock_};
    const auto found = sessions_.find(uuid);
    return found == sessions_.end() ? nullptr : found->second;
}

std::size_t PlayerRegistry::count() const {
    const std::lock_guard<std::mutex> guard{lock_};
    return order_.size();
}

std::vector<PlayerSession*> PlayerRegistry::snapshot() const {
    const std::lock_guard<std::mutex> guard{lock_};
    return order_;
}

} // namespace basalt::server::runtime::player
