#include <basalt/server/runtime/command/ExecutionSource.hpp>

#include <utility>

namespace basalt::server::runtime::command {

ExecutionSource::ExecutionSource(const CommandSource& base, Changes changes)
    : base_(&base), player_(changes.player.value_or(base.player())),
      position_(changes.position.value_or(base.position())), onResult_(std::move(changes.onResult)) {
    const ExecutionSource* inner = base.asExecution();
    world_ = changes.world.value_or(inner == nullptr ? nullptr : inner->world());
    rotation_ = changes.rotation.value_or(inner == nullptr ? std::pair{0.0F, 0.0F} : inner->rotation());
    anchorEyes_ = changes.anchorEyes.value_or(inner != nullptr && inner->anchorEyes());
    entity_ = changes.entity.has_value() ? changes.entity : (inner == nullptr ? std::nullopt : inner->entity());
}

std::string ExecutionSource::name() const { return player_ == nullptr ? base_->name() : player_->name(); }

wire::text::Component ExecutionSource::displayName() const {
    return player_ == nullptr ? base_->displayName() : wire::text::Component::text(player_->name());
}

std::int32_t ExecutionSource::permissionLevel() const { return base_->permissionLevel(); }

void ExecutionSource::sendSystemMessage(const wire::text::Component& message) const {
    base_->sendSystemMessage(message);
}

bool ExecutionSource::hasPermission(std::optional<std::string_view> node) const { return base_->hasPermission(node); }

const CommandSource& ExecutionSource::root() const {
    const ExecutionSource* inner = base_->asExecution();
    return inner == nullptr ? *base_ : inner->root();
}

void ExecutionSource::report(std::int32_t result) const {
    if (onResult_) {
        onResult_(result != 0, result);
    }
    if (const ExecutionSource* inner = base_->asExecution(); inner != nullptr) {
        inner->report(result);
    }
}

world::LoadedWorld* worldIn(const CommandSource& source, const CommandEnvironment& environment) {
    if (const ExecutionSource* execution = source.asExecution();
        execution != nullptr && execution->world() != nullptr) {
        return execution->world();
    }
    const std::map<std::string, world::LoadedWorld*, std::less<>>& worlds = environment.options().worlds;
    if (const player::PlayerSession* player = source.player(); player != nullptr) {
        if (const auto found = worlds.find(player->worldName); found != worlds.end()) {
            return found->second;
        }
    }
    if (const auto found = worlds.find(environment.options().defaultWorldName); found != worlds.end()) {
        return found->second;
    }
    return worlds.empty() ? nullptr : worlds.begin()->second;
}

} // namespace basalt::server::runtime::command
