#include <basalt/server/runtime/BlockChangeBroadcaster.hpp>
#include <basalt/server/runtime/RedstoneTickSystem.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/sched/RegionScheduler.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>

#include <optional>
#include <utility>
#include <vector>

namespace basalt::server::runtime {

void RedstoneTickSystem::tickBlockEvents(world::LoadedWorld& world, const sched::Region& region) {
    std::optional<world::BlockEvent> event = world.state().takeFirstBlockEvent();
    if (!event.has_value()) {
        return;
    }
    const std::vector<player::PlayerSession*> watchers = watchersOf(world);
    // parity: `runBlockEvents` takes the oldest one and drains until nothing is left, so an event
    // booked by another event runs in this same tick, and one booked while the queue still holds an
    // identical one collapses against it. What falls outside what this turn ticks goes back in the
    // queue, which is what `blockEventsToReschedule` is for.
    std::vector<world::BlockEvent> again;
    while (event.has_value()) {
        if (region.owns(event->x >> 4, event->z >> 4)) {
            broadcaster_->runPistonEvent(world, *event, watchers);
        } else {
            again.push_back(std::move(*event));
        }
        event = world.state().takeFirstBlockEvent();
    }
    for (world::BlockEvent& booked : again) {
        world.state().queueBlockEvent(std::move(booked));
    }
}

} // namespace basalt::server::runtime
