#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/entity/Horses.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>

#include <algorithm>
#include <cmath>
#include <string>

namespace basalt::server::runtime::entity::horses {
namespace {

constexpr std::string_view kMaxHealth = "minecraft:max_health";
constexpr std::string_view kJumpStrength = "minecraft:jump_strength";
constexpr std::string_view kMovementSpeed = "minecraft:movement_speed";

/// parity: `Variant` and `Markings`, seven coats and five markings.
constexpr std::int32_t kVariants = 7;
constexpr std::int32_t kMarkings = 5;

/// parity: the ends of `generateMaxHealth`.
constexpr double kMinHealth = 15.0;
constexpr double kMaxHealthBound = 30.0;
const double kMinJump = static_cast<double>(static_cast<float>(static_cast<double>(0.4F)));
const double kMaxJump = static_cast<double>(static_cast<float>(static_cast<double>(0.4F) + 0.2 + 0.2 + 0.2));
const double kMinSpeed = static_cast<double>(static_cast<float>(static_cast<double>(0.45F) * 0.25));
const double kMaxSpeed = static_cast<double>(static_cast<float>((static_cast<double>(0.45F) + 0.3 + 0.3 + 0.3) * 0.25));

[[nodiscard]] bool camelKind(std::string_view path) { return path == "camel" || path == "camel_husk"; }

[[nodiscard]] std::int32_t variantOf(const ServerEntity& horse) {
    return horse.sourceNbt.has_value() ? horse.sourceNbt->getInt("Variant") : 0;
}

[[nodiscard]] std::int32_t wrapped(std::int32_t id, std::int32_t size) { return ((id % size) + size) % size; }

void setBase(ServerEntity& baby, std::string_view name, double value) {
    baby.attributeValues.insert_or_assign(std::string{name}, value);
}

void inheritAttributes(ServerEntity& baby, const ServerEntity& horse, const ServerEntity& partner,
                       random::RandomSource& random) {
    const auto roll = [&](std::string_view name, double min, double max) {
        setBase(baby, name, offspringAttribute(horse.attribute(name), partner.attribute(name), min, max, random));
    };
    roll(kMaxHealth, kMinHealth, kMaxHealthBound);
    roll(kJumpStrength, kMinJump, kMaxJump);
    roll(kMovementSpeed, kMinSpeed, kMaxSpeed);
    baby.health = baby.attribute(kMaxHealth);
}

} // namespace

bool canParent(const ServerEntity& horse) {
    return horse.passengerIds.empty() && !horse.vehicleId.has_value() && tamed(horse) && !baby(horse) &&
           horse.health >= horse.attributeWithModifiers(kMaxHealth) && horse.loveTicks > 0;
}

bool canMate(const ServerEntity& horse, const ServerEntity& partner) {
    if (horse.entityId() == partner.entityId()) {
        return false;
    }
    const std::string_view mine = horse.type().path();
    const std::string_view theirs = partner.type().path();
    bool kinds = false;
    if (mine == "horse" || mine == "donkey") {
        kinds = theirs == "horse" || theirs == "donkey";
    } else if (isLlama(horse.type())) {
        kinds = isLlama(partner.type());
    } else if (camelKind(mine)) {
        kinds = camelKind(theirs);
    }
    return kinds && canParent(horse) && canParent(partner);
}

wire::common::Identifier offspringType(const ServerEntity& horse, const ServerEntity& partner) {
    const std::string_view mine = horse.type().path();
    const std::string_view theirs = partner.type().path();
    if ((mine == "horse" && theirs == "donkey") || (mine == "donkey" && theirs == "horse")) {
        return wire::common::Identifier::vanilla("mule");
    }
    if (isLlama(horse.type())) {
        return wire::common::Identifier::vanilla("llama");
    }
    if (camelKind(mine)) {
        return wire::common::Identifier::vanilla("camel");
    }
    return horse.type();
}

void inherit(ServerEntity& baby, const ServerEntity& horse, const ServerEntity& partner, random::RandomSource& random) {
    const std::string_view mine = horse.type().path();
    if (camelKind(mine)) {
        return;
    }
    if (!baby.sourceNbt.has_value()) {
        baby.sourceNbt.emplace();
    }
    if (mine == "horse" && baby.type().path() == "horse") {
        // parity: `Horse.getBreedOffspring`: four in nine this coat, four the partner's, one any.
        const std::int32_t coatRoll = random.nextInt(9);
        std::int32_t coat = 0;
        if (coatRoll < 4) {
            coat = wrapped(variantOf(horse) & 255, kVariants);
        } else if (coatRoll < 8) {
            coat = wrapped(variantOf(partner) & 255, kVariants);
        } else {
            coat = random.nextInt(kVariants);
        }
        const std::int32_t markingRoll = random.nextInt(5);
        std::int32_t marking = 0;
        if (markingRoll < 2) {
            marking = wrapped((variantOf(horse) & 0xff00) >> 8, kMarkings);
        } else if (markingRoll < 4) {
            marking = wrapped((variantOf(partner) & 0xff00) >> 8, kMarkings);
        } else {
            marking = random.nextInt(kMarkings);
        }
        baby.sourceNbt->putInt("Variant", (coat & 255) | ((marking << 8) & 0xff00));
    }
    inheritAttributes(baby, horse, partner, random);
    if (isLlama(horse.type())) {
        // parity: `Llama.getBreedOffspring`'s strength, one in thirty-three a point stronger.
        const std::int32_t strongest =
            std::max(horse.sourceNbt.has_value() ? horse.sourceNbt->getInt("Strength") : 1,
                     partner.sourceNbt.has_value() ? partner.sourceNbt->getInt("Strength") : 1);
        std::int32_t strength = random.nextInt(std::max(1, strongest)) + 1;
        if (random.nextFloat() < 0.03F) {
            ++strength;
        }
        baby.sourceNbt->putInt("Strength", std::clamp(strength, 1, 5));
        baby.sourceNbt->putInt("Variant", random.nextBoolean() ? variantOf(horse) : variantOf(partner));
    }
}

double offspringAttribute(double parentA, double parentB, double min, double max, random::RandomSource& random) {
    const double a = std::clamp(parentA, min, max);
    const double b = std::clamp(parentB, min, max);
    const double margin = 0.15 * (max - min);
    const double range = std::abs(a - b) + (margin * 2.0);
    const double average = (a + b) / 2.0;
    const double first = random.nextDouble();
    const double second = random.nextDouble();
    const double third = random.nextDouble();
    const double quality = ((first + second + third) / 3.0) - 0.5;
    const double value = average + (range * quality);
    if (value > max) {
        return max - (value - max);
    }
    if (value < min) {
        return min + (min - value);
    }
    return value;
}

} // namespace basalt::server::runtime::entity::horses
