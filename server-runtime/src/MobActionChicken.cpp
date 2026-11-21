#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/MobActionSystem.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>

namespace basalt::server::runtime {

using entity::ServerEntity;
using wire::protocol::play::SoundSource;

namespace {

[[nodiscard]] std::int32_t nextEggTime(random::RandomSource& random) { return random.nextInt(6000) + 6000; }

} // namespace

void MobActionSystem::tickChicken(world::LoadedWorld& world, ServerEntity& chicken, random::RandomSource& random) {
    // A saved or plugin-written `EggLayTime` is taken once and dropped, so the live counter owns the value.
    if (chicken.sourceNbt.has_value() && chicken.sourceNbt->contains("EggLayTime")) {
        chicken.eggLayTime = chicken.sourceNbt->getInt("EggLayTime");
        chicken.sourceNbt->remove("EggLayTime");
    } else if (chicken.eggLayTime < 0) {
        chicken.eggLayTime = nextEggTime(random);
    }
    const bool jockey = chicken.sourceNbt.has_value() && chicken.sourceNbt->getBoolean("IsChickenJockey");
    // parity: `Chicken.aiStep`, an egg every five to ten minutes unless it is a baby or a jockey.
    bool gate1 = chicken.health <= 0.0 || chicken.removed || chicken.age < 0 || jockey;
    if (!gate1) {
        gate1 = --chicken.eggLayTime > 0;
    }
    if (gate1) {
        return;
    }
    if (armadilloDrops(world, chicken, "gameplay/chicken_lay")) {
        if (!chicken.sourceNbt.has_value() || !chicken.sourceNbt->getBoolean("Silent")) {
            playSound(world, chicken, "entity.chicken.egg", 1.0F,
                      ((random.nextFloat() - random.nextFloat()) * 0.2F) + 1.0F, SoundSource::Neutral);
        }
        world::vibrations::emit(world.state(), "entity_place", chicken.x, chicken.y, chicken.z, world.gameTime(),
                                {.entity = chicken.uuid()});
    }
    chicken.eggLayTime = nextEggTime(random);
}

} // namespace basalt::server::runtime
