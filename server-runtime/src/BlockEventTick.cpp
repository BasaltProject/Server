#include <basalt/server/runtime/BlockChangeBroadcaster.hpp>
#include <basalt/server/runtime/RedstoneTickSystem.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/sched/RegionScheduler.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>

#include <utility>
#include <vector>

namespace basalt::server::runtime {

void RedstoneTickSystem::tickBlockEvents(world::LoadedWorld& world, const sched::Region& region) {
    std::vector<world::BlockEvent> events = world.state().takeBlockEvents();
    if (events.empty()) {
        return;
    }
    const std::vector<player::PlayerSession*> watchers = watchersOf(world);
    // parity: `runBlockEvents` drains the queue rather than taking a turn's worth, so an event booked
    // by another event runs in this same tick. What falls outside what this turn ticks goes back in
    // the queue, which is what `blockEventsToReschedule` is for.
    std::vector<world::BlockEvent> again;
    while (!events.empty()) {
        for (const world::BlockEvent& event : events) {
            if (!region.owns(event.x >> 4, event.z >> 4)) {
                again.push_back(event);
                continue;
            }
            broadcaster_->runPistonEvent(world, event, watchers);
        }
        events = world.state().takeBlockEvents();
    }
    for (world::BlockEvent& event : again) {
        world.state().queueBlockEvent(std::move(event));
    }
}

} // namespace basalt::server::runtime
