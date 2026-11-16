#pragma once

#include <basalt/server/plugins/PluginEvents.hpp>
#include <basalt/server/plugins/PluginHookEventTypes.hpp>
#include <basalt/server/runtime/entity/EntityCombat.hpp>
#include <basalt/server/runtime/plugins/EventSink.hpp>
#include <basalt/wire/protocol/play/BlockPos.hpp>

#include <cstdint>
#include <string>
#include <string_view>

namespace basalt::server::runtime::entity {
class ServerEntity;
}

namespace basalt::server::runtime::player {
class PlayerSession;
}

namespace basalt::server::runtime::world {
class LoadedWorld;
struct DroppedItem;
} // namespace basalt::server::runtime::world

namespace basalt::server::runtime::plugins::hookEvents {

void entity(server::plugins::PluginEvent& event, std::string_view world, const entity::ServerEntity& who);

/// The same fields for something that is not a `ServerEntity`, like a player.
void entity(server::plugins::PluginEvent& event, std::string_view world, std::int32_t id, std::string_view type,
            double x, double y, double z);

void position(server::plugins::PluginEvent& event, std::string_view world, std::int32_t x, std::int32_t y,
              std::int32_t z);

/// Asks the plugins whether `who` may turn the block at these coordinates from one state into another.
[[nodiscard]] bool changeBlockAllowed(const EventSink& sink, std::string_view world, const entity::ServerEntity& who,
                                      std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t fromState,
                                      std::int32_t toState, std::string_view cause = "other");

/// What lit a fire: an entity by id, a block by position, or nothing the plugins can name.
struct Igniter {
    std::int32_t entityId = -1;
    bool block = false;
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;
};

/// Asks `entity.combust` for a fire something lit; false when a listener refused it, `ticks` carries their change.
[[nodiscard]] bool combustAllowed(const EventSink& sink, std::string_view world, std::int32_t id, std::string_view type,
                                  std::int32_t& ticks, std::string_view cause, const Igniter& igniter);

/// Fills the `entity.knockback` fields that describe `knock`.
void fillKnock(server::plugins::PluginEvent& event, const entity::entityCombat::Knock& knock);

/// Reads what the plugins did to an `entity.knockback`: false when cancelled, else `knock` carries their edits.
[[nodiscard]] bool readKnock(const server::plugins::PluginEvent* answered, entity::entityCombat::Knock& knock);

/// Raises `player.spawn_change`: false when cancelled, else `x`, `y`, `z` carry the edits.
[[nodiscard]] bool spawnChangeAllowed(const EventSink& sink, const player::PlayerSession& who, std::int32_t& x,
                                      std::int32_t& y, std::int32_t& z, std::string_view cause);

/// Raises `block.cauldron_level`: false when cancelled, else `to` carries the state a listener chose.
[[nodiscard]] bool cauldronChange(const EventSink& sink, std::string_view world, std::int32_t x, std::int32_t y,
                                  std::int32_t z, std::string_view reason, std::int32_t from, std::int32_t& to,
                                  const player::PlayerSession* who = nullptr, std::int32_t entityId = -1);

/// Raises `entity.enter_love` and, unless cancelled, puts `animal` in love for the ticks a listener left.
bool enterLove(const EventSink& sink, std::string_view world, entity::ServerEntity& animal,
               const player::PlayerSession* cause);

/// Raises `entity.egg_hatch` for any thrown egg, and takes what a listener left in the three outputs.
void eggHatch(const EventSink& sink, std::string_view world, const entity::ServerEntity& egg, bool& hatching,
              std::int32_t& count, std::string& type);

/// Raises `player.sign_open`: false when a listener kept the sign editor from opening.
[[nodiscard]] bool signOpenAllowed(const EventSink& sink, const player::PlayerSession& who, std::int32_t x,
                                   std::int32_t y, std::int32_t z, bool front, std::string_view cause);

/// Spawns what `who` dropped and takes it back if a plugin cancels the `entity.drop_item` it raises.
void dropFor(world::LoadedWorld& world, const entity::ServerEntity& who, world::DroppedItem dropped);

/// Raises `entity.pose_change`, the moment something that is not a mob changes the pose it is shown in.
void poseChange(const EventSink& sink, std::string_view world, std::int32_t id, std::string_view type, double x,
                double y, double z, wire::protocol::play::Pose from, wire::protocol::play::Pose to);

/// Raises `entity.pose_change` for a mob whose own pose moved since the last time it was looked at.
void watchPose(const EventSink& sink, std::string_view world, entity::ServerEntity& mob);

/// The id of the block a state belongs to, `minecraft:air` for an unknown one.
[[nodiscard]] std::string blockName(std::int32_t stateId);

} // namespace basalt::server::runtime::plugins::hookEvents
