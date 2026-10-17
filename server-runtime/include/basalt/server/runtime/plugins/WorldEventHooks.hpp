#pragma once

#include <basalt/server/runtime/BlockChangeBroadcaster.hpp>
#include <basalt/server/runtime/EnvironmentTickSystem.hpp>
#include <basalt/server/runtime/ExplosionSystem.hpp>
#include <basalt/server/runtime/MobActionSystem.hpp>
#include <basalt/server/runtime/NaturalSpawningSystem.hpp>
#include <basalt/server/runtime/RedstoneTickSystem.hpp>
#include <basalt/server/runtime/plugins/EventSink.hpp>

#include <functional>
#include <string>

/// The callbacks of the systems that run for a whole world, turned into the events plugins listen to.
namespace basalt::server::runtime::plugins::worldEventHooks {

/// The name of the world an entity stands in; empty if none does.
using WorldOf = std::function<std::string(const entity::ServerEntity& entity)>;

void explosions(ExplosionSystem::Hooks& hooks, const EventSink& sink);
void redstone(RedstoneTickSystem::Hooks& hooks, const EventSink& sink);
void environment(EnvironmentTickSystem::Hooks& hooks, const EventSink& sink);
void spawning(NaturalSpawningSystem::Hooks& hooks, const EventSink& sink);
void pistons(BlockChangeBroadcaster::Hooks& hooks, const EventSink& sink);
void mobs(MobActionSystem::Hooks& hooks, const EventSink& sink, WorldOf worldOf);

} // namespace basalt::server::runtime::plugins::worldEventHooks
