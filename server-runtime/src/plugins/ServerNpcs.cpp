#include <basalt/server/runtime/plugins/ServerNpcs.hpp>

#include <cstdint>
#include <utility>

namespace basalt::server::runtime::plugins {

void NpcRegistry::add(std::shared_ptr<Npc> npc, OnClick onClick) {
    const std::int32_t entityId = npc->entityId;
    npcs_.insert_or_assign(entityId, std::move(npc));
    if (onClick) {
        clicks_.insert_or_assign(entityId, std::move(onClick));
    }
}

void NpcRegistry::remove(std::int32_t entityId) {
    npcs_.erase(entityId);
    clicks_.erase(entityId);
}

std::vector<std::shared_ptr<Npc>> NpcRegistry::all() const {
    std::vector<std::shared_ptr<Npc>> alive;
    for (const auto& [entityId, npc] : npcs_) {
        if (!npc->removed()) {
            alive.push_back(npc);
        }
    }
    return alive;
}

std::shared_ptr<Npc> NpcRegistry::of(std::int32_t entityId) const {
    const auto found = npcs_.find(entityId);
    if (found == npcs_.end() || found->second->removed()) {
        return nullptr;
    }
    return found->second;
}

bool NpcRegistry::clicked(std::int32_t entityId, const std::string& playerUuid) {
    const auto handler = clicks_.find(entityId);
    if (handler == clicks_.end()) {
        return npcs_.contains(entityId);
    }
    handler->second(playerUuid);
    return true;
}

void NpcRegistry::sweep() {
    std::vector<std::int32_t> gone;
    for (const auto& [entityId, npc] : npcs_) {
        if (npc->removed()) {
            gone.push_back(entityId);
        }
    }
    for (const std::int32_t entityId : gone) {
        npcs_.erase(entityId);
        clicks_.erase(entityId);
    }
}

std::vector<std::int32_t> NpcRegistry::removedSince(const std::set<std::int32_t>& known) const {
    std::vector<std::int32_t> gone;
    for (const std::int32_t entityId : known) {
        const auto found = npcs_.find(entityId);
        if (found == npcs_.end() || found->second->removed()) {
            gone.push_back(entityId);
        }
    }
    return gone;
}

std::string NpcHandle::name() const { return label_ != nullptr ? label_->text() : npc_->name; }

void NpcHandle::setName(std::string_view value) {
    if (label_ != nullptr) {
        label_->setText(value);
        return;
    }
    npc_->name = value;
    npc_->changed();
}

void NpcHandle::skin(std::string_view texture, const std::optional<std::string>& signature) {
    npc_->skinTexture = std::string{texture};
    npc_->skinSignature = signature;
    npc_->changed();
}

bool NpcHandle::skinFrom(std::string_view playerName) {
    if (!profiles_) {
        return false;
    }
    const std::optional<std::pair<std::string, std::optional<std::string>>> found = profiles_(std::string{playerName});
    if (!found.has_value()) {
        return false;
    }
    skin(found->first, found->second);
    return true;
}

void NpcHandle::look(float yaw, float pitch) {
    npc_->yaw = yaw;
    npc_->pitch = pitch;
    // Pointed somewhere on purpose, so it stops following people.
    npc_->looksAtPlayers = false;
    npc_->changed();
}

void NpcHandle::looksAtPlayers(bool follows) {
    npc_->looksAtPlayers = follows;
    npc_->changed();
}

bool NpcHandle::teleport(double x, double y, double z) {
    npc_->x = x;
    npc_->y = y;
    npc_->z = z;
    npc_->changed();
    if (label_ != nullptr) {
        static_cast<void>(label_->teleport(x, y + kLabelHeight, z));
    }
    return true;
}

void NpcHandle::showOnlyTo(const std::vector<std::string>& uuids) {
    npc_->visibleTo = std::set<std::string>{uuids.begin(), uuids.end()};
    npc_->changed();
    if (label_ != nullptr) {
        label_->showOnlyTo(uuids);
    }
}

void NpcHandle::showToAll() {
    npc_->visibleTo.reset();
    npc_->changed();
    if (label_ != nullptr) {
        label_->showToAll();
    }
}

std::optional<std::vector<std::string>> NpcHandle::audience() const {
    if (!npc_->visibleTo.has_value()) {
        return std::nullopt;
    }
    return std::vector<std::string>{npc_->visibleTo->begin(), npc_->visibleTo->end()};
}

void NpcHandle::remove() {
    npc_->remove();
    if (label_ != nullptr) {
        label_->remove();
    }
}

} // namespace basalt::server::runtime::plugins
