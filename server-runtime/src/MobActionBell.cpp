#include <basalt/server/plugins/PluginHookEventTypes.hpp>
#include <basalt/server/runtime/BlockChangeBroadcaster.hpp>
#include <basalt/server/runtime/MobActionSystem.hpp>
#include <basalt/server/runtime/RaidSystem.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/plugins/HookEvents.hpp>
#include <basalt/server/runtime/world/Bells.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>

#include <cstdlib>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace basalt::server::runtime {
namespace {

[[nodiscard]] bool within(const entity::ServerEntity& mob, std::int32_t x, std::int32_t y, std::int32_t z,
                          double radius) {
    const double dx = (x + 0.5) - mob.x;
    const double dy = (y + 0.5) - mob.y;
    const double dz = (z + 0.5) - mob.z;
    return (dx * dx) + (dy * dy) + (dz * dz) < radius * radius;
}

[[nodiscard]] bool livingRaider(const entity::ServerEntity& mob) {
    return !mob.removed && !mob.killed && RaidSystem::isRaider(mob);
}

} // namespace

bool MobActionSystem::tickBell(world::LoadedWorld& world, const std::int32_t x, const std::int32_t y,
                               const std::int32_t z, wire::nbt::NbtCompound& data) {
    if (!world::bells::ringing(data)) {
        return false;
    }
    world::bells::State state = world::bells::read(data);
    bool raidersNearby = false;
    if (state.ticks + 1 >= world::bells::kTicksBeforeResonation && state.resonationTicks == 0) {
        for (const entity::ServerEntity* mob : world.state().entities()) {
            if (mob != nullptr && livingRaider(*mob) && within(*mob, x, y, z, world::bells::kHearRadius)) {
                raidersNearby = true;
                break;
            }
        }
    }
    const world::bells::Step step = world::bells::advance(state, raidersNearby);
    world::bells::write(state, data);
    if (step.startedResonating && broadcaster_ != nullptr) {
        broadcaster_->sendEffects(watchersOf(world), x, y, z, world.state().blockState(x, y, z).value_or(0),
                                  {world::blockEffect::Sound{.name = "block.bell.resonate",
                                                             .source = wire::protocol::play::SoundSource::Blocks,
                                                             .volume = 1.0F,
                                                             .pitch = 1.0F}});
    }
    if (step.endedResonating) {
        std::vector<entity::ServerEntity*> revealed;
        for (entity::ServerEntity* mob : world.state().entities()) {
            if (mob != nullptr && livingRaider(*mob) && within(*mob, x, y, z, world::bells::kHighlightRadius)) {
                revealed.push_back(mob);
            }
        }
        std::string ids;
        for (const entity::ServerEntity* mob : revealed) {
            ids += ids.empty() ? std::to_string(mob->entityId()) : "," + std::to_string(mob->entityId());
        }
        const server::plugins::PluginEvent* answered = world.events().ask(
            &server::plugins::pluginEventTypes::blockBellResonate, [&](server::plugins::PluginEvent& event) {
                plugins::hookEvents::position(event, world.name(), x, y, z);
                event.set("entities", ids);
            });
        if (answered != nullptr) {
            if (const auto* kept = std::get_if<std::string>(&answered->get("entities")); kept != nullptr) {
                std::set<std::int32_t> allowed;
                std::istringstream parts{*kept};
                for (std::string part; std::getline(parts, part, ',');) {
                    allowed.insert(static_cast<std::int32_t>(std::strtol(part.c_str(), nullptr, 10)));
                }
                std::erase_if(revealed,
                              [&](const entity::ServerEntity* mob) { return !allowed.contains(mob->entityId()); });
            }
        }
        for (entity::ServerEntity* mob : revealed) {
            if (world.events().allowed(&server::plugins::pluginEventTypes::blockBellRevealRaider,
                                       [&](server::plugins::PluginEvent& event) {
                                           plugins::hookEvents::position(event, world.name(), x, y, z);
                                           plugins::hookEvents::entity(event, world.name(), *mob);
                                       })) {
                villagersIn(world).addEffect(*mob, "glowing", world::bells::kGlowTicks, 0);
            }
        }
    }
    return true;
}

} // namespace basalt::server::runtime
