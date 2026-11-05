#pragma once

namespace basalt::server::runtime::entity::ai {
class MobContext;
}

namespace basalt::server::runtime::entity::brain {

class BrainState;

/// parity: `NearestLivingEntitySensor` and `PlayerSensor`, each on its own scan interval.
void runSensors(ai::MobContext& mob, BrainState& state);

/// parity: `PlayerSensor`, stored as `nearest_players` and `nearest_visible_player`.
void nearestPlayers(ai::MobContext& mob, BrainState& state, double range);

} // namespace basalt::server::runtime::entity::brain
