#include <basalt/server/runtime/BlockChangeBroadcaster.hpp>
#include <basalt/server/runtime/MobActionSystem.hpp>
#include <basalt/server/runtime/RaidSystem.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/world/Bells.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>

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
        for (entity::ServerEntity* mob : world.state().entities()) {
            if (mob != nullptr && livingRaider(*mob) && within(*mob, x, y, z, world::bells::kHighlightRadius)) {
                villagersIn(world).addEffect(*mob, "glowing", world::bells::kGlowTicks, 0);
            }
        }
    }
    return true;
}

} // namespace basalt::server::runtime
