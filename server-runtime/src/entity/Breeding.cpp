#include <basalt/server/runtime/entity/Breeding.hpp>
#include <basalt/server/runtime/entity/MobBrain.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>

#include <algorithm>
#include <map>
#include <utility>

namespace basalt::server::runtime::entity::breeding {
namespace {

/// parity: the `isFood` of each animal class. Only the ones this build can actually spawn are
/// listed; anything else refuses the food rather than pretending.
[[nodiscard]] const std::map<std::string, std::set<std::string, std::less<>>, std::less<>>& foods() {
    static const std::map<std::string, std::set<std::string, std::less<>>, std::less<>> all{
        {"cow", {"wheat"}},
        {"mooshroom", {"wheat"}},
        {"sheep", {"wheat"}},
        {"goat", {"wheat"}},
        {"pig", {"carrot", "potato", "beetroot"}},
        {"chicken", {"wheat_seeds", "melon_seeds", "pumpkin_seeds", "beetroot_seeds", "torchflower_seeds"}},
        {"rabbit", {"carrot", "golden_carrot", "dandelion"}},
        {"horse", {"golden_carrot", "golden_apple", "enchanted_golden_apple"}},
        {"donkey", {"golden_carrot", "golden_apple", "enchanted_golden_apple"}},
        {"llama", {"hay_block"}},
        {"wolf", {"beef", "porkchop", "chicken", "mutton", "rabbit"}},
        {"cat", {"cod", "salmon"}},
        {"ocelot", {"cod", "salmon"}},
        {"fox", {"sweet_berries", "glow_berries"}},
        {"panda", {"bamboo"}},
        {"turtle", {"seagrass"}},
        {"bee", {"dandelion", "poppy", "sunflower", "allium"}},
        {"axolotl", {"tropical_fish_bucket"}},
        {"frog", {"slime_ball"}},
        {"camel", {"cactus"}},
        {"sniffer", {"torchflower_seeds"}},
        {"armadillo", {"spider_eye"}},
        {"strider", {"warped_fungus"}},
        {"hoglin", {"crimson_fungus"}}};
    return all;
}

} // namespace

const std::set<std::string, std::less<>>& foodFor(const wire::common::Identifier& type) {
    static const std::set<std::string, std::less<>> none;
    const auto found = foods().find(type.path());
    return found == foods().end() ? none : found->second;
}

bool isFood(const wire::common::Identifier& type, const wire::common::Identifier& item) {
    return foodFor(type).contains(item.path());
}

bool canFallInLove(const ServerEntity& animal) { return animal.loveTicks <= 0 && animal.age == 0; }

void feed(ServerEntity& animal, std::optional<std::string> cause) {
    animal.loveTicks = kLoveTicks;
    animal.loveCause = std::move(cause);
}

bool canMate(const ServerEntity& animal, const ServerEntity& other) {
    return animal.entityId() != other.entityId() && animal.type() == other.type() && animal.loveTicks > 0 &&
           other.loveTicks > 0;
}

void growUp(ServerEntity& animal) {
    if (animal.age < 0) {
        animal.age++;
    } else if (animal.age > 0) {
        animal.age--;
    }
}

ServerEntity* tick(ServerEntity& animal, const std::vector<ServerEntity*>& nearby) {
    if (animal.breedCooldown > 0) {
        animal.breedCooldown--;
    }
    // parity: a baby's love is cleared every tick, so feeding one does nothing at all.
    if (animal.age != 0) {
        animal.loveTicks = 0;
        growUp(animal);
        return nullptr;
    }
    if (animal.loveTicks <= 0) {
        animal.courtshipTicks = 0;
        return nullptr;
    }
    animal.loveTicks--;

    ServerEntity* partner = nullptr;
    double bestDistance = 0.0;
    for (ServerEntity* candidate : nearby) {
        if (candidate == nullptr || !canMate(animal, *candidate)) {
            continue;
        }
        const double distance = mobBrain::distanceTo(*candidate, animal.x, animal.y, animal.z);
        if (partner == nullptr || distance < bestDistance) {
            partner = candidate;
            bestDistance = distance;
        }
    }
    if (partner == nullptr || bestDistance > kPartnerRange) {
        animal.courtshipTicks = 0;
        return nullptr;
    }
    animal.courtshipTicks++;
    if (animal.courtshipTicks < kCourtshipTicks || bestDistance * bestDistance >= kBreedDistanceSquared) {
        return nullptr;
    }
    return partner;
}

void afterBreeding(ServerEntity& animal, ServerEntity& partner) {
    for (ServerEntity* parent : {&animal, &partner}) {
        parent->loveTicks = 0;
        parent->loveCause.reset();
        parent->courtshipTicks = 0;
        parent->age = kParentCooldown;
        parent->breedCooldown = kParentCooldown;
    }
}

} // namespace basalt::server::runtime::entity::breeding
