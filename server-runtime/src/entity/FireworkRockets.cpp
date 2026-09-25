#include <basalt/server/runtime/entity/FireworkRockets.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>

#include <array>
#include <memory>
#include <utility>

namespace basalt::server::runtime::entity::fireworkRockets {
namespace {

using wire::nbt::NbtCompound;
using wire::nbt::NbtTag;

/// parity: `random.triangle(0, 0.002297)`, the sideways drift a rocket starts with.
constexpr double kDrift = 0.002297;
constexpr double kClimb = 0.05;
constexpr std::int32_t kTicksPerFlight = 10;

/// parity: `DyeColor.fireworkColor`, in the dyes' own order.
constexpr std::array<std::int32_t, 16> kFireworkColours{15790320, 15435844, 12801229, 6719955, 14602026, 4312372,
                                                        14188952, 4408131,  11250603, 2651799, 8073150,  2437522,
                                                        5320730,  3887386,  11743532, 1973019};

[[nodiscard]] const NbtCompound* fireworksOf(const NbtCompound& item) {
    const NbtCompound* components = item.getCompound("components");
    return components == nullptr ? nullptr : components->getCompound("minecraft:fireworks");
}

} // namespace

const wire::common::Identifier& type() {
    static const wire::common::Identifier kType = wire::common::Identifier::vanilla("firework_rocket");
    return kType;
}

bool isRocket(const ServerEntity& entity) { return entity.type() == type(); }

NbtCompound plainRocket() {
    NbtCompound item;
    item.putString("id", "minecraft:firework_rocket");
    item.putInt("count", 1);
    return item;
}

NbtCompound celebrationRocket(std::int32_t dye, std::int32_t flight) {
    NbtCompound explosion;
    explosion.putString("shape", "burst");
    explosion.putIntArray("colors", {kFireworkColours[static_cast<std::size_t>(dye) % kFireworkColours.size()]});
    NbtCompound fireworks;
    fireworks.putByte("flight_duration", static_cast<std::int8_t>(flight));
    fireworks.set("explosions", NbtTag::ofList(wire::nbt::NbtList{wire::nbt::NbtType::Compound,
                                                                  {NbtTag::ofCompound(std::move(explosion))}}));
    NbtCompound components;
    components.set("minecraft:fireworks", NbtTag::ofCompound(std::move(fireworks)));
    NbtCompound item = plainRocket();
    item.set("components", NbtTag::ofCompound(std::move(components)));
    return item;
}

std::int32_t flightDuration(const NbtCompound& item) {
    const NbtCompound* fireworks = fireworksOf(item);
    return fireworks == nullptr ? 0 : fireworks->getByte("flight_duration") & 0xFF;
}

std::int32_t explosionCount(const NbtCompound& item) {
    const NbtCompound* fireworks = fireworksOf(item);
    const wire::nbt::NbtList* explosions = fireworks == nullptr ? nullptr : fireworks->getList("explosions");
    return explosions == nullptr ? 0 : static_cast<std::int32_t>(explosions->size());
}

RocketState& start(ServerEntity& rocket, NbtCompound item, random::RandomSource& random) {
    rocket.firework = std::make_shared<RocketState>();
    RocketState& state = *rocket.firework;
    const std::int32_t flights = 1 + flightDuration(item);
    state.item = std::move(item);
    rocket.velocityX = random.triangle(0.0, kDrift);
    rocket.velocityY = kClimb;
    rocket.velocityZ = random.triangle(0.0, kDrift);
    state.lifetime = (kTicksPerFlight * flights) + random.nextInt(6) + random.nextInt(7);
    return state;
}

void write(const ServerEntity& rocket, NbtCompound& tag) {
    if (rocket.firework == nullptr) {
        return;
    }
    tag.putInt("Life", rocket.firework->life);
    tag.putInt("LifeTime", rocket.firework->lifetime);
    tag.set("FireworksItem", NbtTag::ofCompound(rocket.firework->item));
    tag.putBoolean("ShotAtAngle", rocket.firework->shotAtAngle);
}

void read(const NbtCompound& tag, ServerEntity& rocket) {
    rocket.firework = std::make_shared<RocketState>();
    rocket.firework->life = tag.getInt("Life");
    rocket.firework->lifetime = tag.getInt("LifeTime");
    const NbtCompound* item = tag.getCompound("FireworksItem");
    rocket.firework->item = item == nullptr ? plainRocket() : *item;
    rocket.firework->shotAtAngle = tag.getBoolean("ShotAtAngle");
}

} // namespace basalt::server::runtime::entity::fireworkRockets
