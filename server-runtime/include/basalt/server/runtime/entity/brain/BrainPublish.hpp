#pragma once

namespace basalt::server::runtime::entity {
class ServerEntity;

namespace ai {
class MobContext;
}
} // namespace basalt::server::runtime::entity

namespace basalt::server::runtime::entity::brain {

class BrainState;

/// True for the species whose behaviour is a native brain, which owns the activity and memories of its BrainState.
[[nodiscard]] bool hasNativeBrain(const ServerEntity& mob);

/// Marks the state as native-owned and registers the memories the species publishes.
void adopt(const ServerEntity& mob, BrainState& state);

/// Copies the species' own activity, targets and sightings into the state after the species has ticked.
void publish(ai::MobContext& context, BrainState& state);

/// The same for a villager, whose brain keeps its own memory table.
void publishVillager(const ServerEntity& villager, BrainState& state);

} // namespace basalt::server::runtime::entity::brain
