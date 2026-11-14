#include <basalt/server/entity/Breeding.hpp>
#include <basalt/server/entity/Collision.hpp>
#include <basalt/server/entity/Combat.hpp>
#include <basalt/server/entity/Leash.hpp>
#include <basalt/server/entity/LevelValue.hpp>
#include <basalt/server/entity/Projectiles.hpp>
#include <basalt/server/plugins/PluginMechanics.hpp>
#include <basalt/server/registry/CreativeTabs.hpp>
#include <basalt/server/world/FolderRegions.hpp>
#include <basalt/server/world/climate/OverworldBiomes.hpp>

namespace basalt::server::plugins::mechanics {
namespace {
namespace geometry = entity::geometry;
namespace projectile = entity::projectile;
namespace leash = entity::leash;

constexpr std::size_t kColliderStride = 7;
constexpr std::size_t kCandidateStride = 9;

geometry::Point pointOf(std::span<const double> values, std::size_t at = 0) {
    return values.size() < at + 3 ? geometry::Point{} : geometry::Point{values[at], values[at + 1], values[at + 2]};
}

geometry::Box boxOf(std::span<const double> raw) {
    return raw.size() < 6 ? geometry::Box{} : geometry::makeBox(raw[0], raw[1], raw[2], raw[3], raw[4], raw[5]);
}

leash::Body bodyOf(std::span<const double> values) {
    leash::Body body;
    if (values.size() >= 6) {
        body.position = pointOf(values);
        body.width = static_cast<float>(values[3]);
        body.height = static_cast<float>(values[4]);
        body.yaw = static_cast<float>(values[5]);
    }
    return body;
}

std::vector<entity::collision::Collider> collidersOf(std::span<const double> raw) {
    std::vector<entity::collision::Collider> out;
    for (std::size_t at = 0; at + kColliderStride <= raw.size(); at += kColliderStride) {
        out.push_back({geometry::makeBox(raw[at], raw[at + 1], raw[at + 2], raw[at + 3], raw[at + 4], raw[at + 5]),
                       static_cast<std::int32_t>(raw[at + 6])});
    }
    return out;
}

std::vector<projectile::Candidate> candidatesOf(std::span<const double> raw) {
    std::vector<projectile::Candidate> out;
    for (std::size_t at = 0; at + kCandidateStride <= raw.size(); at += kCandidateStride) {
        projectile::Candidate candidate;
        candidate.box = geometry::makeBox(raw[at], raw[at + 1], raw[at + 2], raw[at + 3], raw[at + 4], raw[at + 5]);
        candidate.pickRadius = static_cast<float>(raw[at + 6]);
        candidate.pickableFromInside = raw[at + 7] != 0.0;
        candidate.rootVehicle = static_cast<std::int32_t>(raw[at + 8]);
        out.push_back(candidate);
    }
    return out;
}

std::optional<std::array<double, 4>> hitOf(const projectile::Hit& hit) {
    if (hit.index < 0) {
        return std::nullopt;
    }
    return std::array<double, 4>{static_cast<double>(hit.index), hit.at[0], hit.at[1], hit.at[2]};
}
} // namespace

Point collide(std::span<const double> colliders, std::span<const double> box, const Point& movement) {
    return entity::collision::sweep(collidersOf(colliders), boxOf(box), movement);
}

std::optional<Point> stepUp(std::span<const double> colliders, std::span<const double> grounded, double groundedOffsetY,
                            const Point& movement, float maxUpStep, double reachSquared) {
    return entity::collision::stepUp(collidersOf(colliders), boxOf(grounded), groundedOffsetY, movement, maxUpStep,
                                     reachSquared);
}

double offspringAttribute(double parentA, double parentB, double min, double max, double first, double second,
                          double third) {
    return entity::breeding::offspringAttribute(parentA, parentB, min, max, first, second, third);
}

float levelValue(std::span<const double> program, int level) { return entity::levelValue::evaluate(program, level); }

float afterArmour(float damage, float armour, float toughness, float fractionAdd) {
    return entity::combat::afterArmour(damage, armour, toughness, fractionAdd);
}

float afterProtection(float damage, float protection) { return entity::combat::afterProtection(damage, protection); }

float afterResistance(float damage, std::int32_t amplifier) {
    return entity::combat::afterResistance(damage, amplifier);
}

bool scalesWithDifficulty(std::string_view scaling, bool causedByLivingNonPlayer) {
    return entity::combat::scalesWithDifficulty(scaling, causedByLivingNonPlayer);
}

std::array<float, 2> absorb(float damage, float absorption) {
    const entity::combat::Absorbed split = entity::combat::absorb(damage, absorption);
    return {split.health, split.absorption};
}

std::optional<std::array<float, 4>> rotateTowardsMovement(double motionX, double motionY, double motionZ, float xRotO,
                                                          float yRotO, float xRot, float yRot, float speed) {
    const auto turned = projectile::rotateTowardsMovement(motionX, motionY, motionZ, xRotO, yRotO, xRot, yRot, speed);
    if (!turned.has_value()) {
        return std::nullopt;
    }
    return std::array<float, 4>{turned->xRotO, turned->yRotO, turned->xRot, turned->yRot};
}

std::optional<std::array<double, 4>> closestHit(std::span<const double> candidates, const Point& from, const Point& to,
                                                float margin) {
    return hitOf(projectile::closestHit(candidatesOf(candidates), from, to, margin));
}

std::optional<std::array<double, 4>> pickedHit(std::span<const double> candidates, std::int32_t exceptRootVehicle,
                                               const Point& from, const Point& to, double maxValue) {
    return hitOf(projectile::pickedHit(candidatesOf(candidates), exceptRootVehicle, from, to, maxValue));
}

float projectileMargin(std::int32_t tickCount) { return projectile::margin(tickCount); }

std::array<double, 6> shulkerBox(float size, std::int32_t direction, float progressFrom, float progressTo,
                                 const Point& position) {
    const geometry::Box box = projectile::shulkerProgressBox(size, direction, progressFrom, progressTo, position);
    return {box.minX, box.minY, box.minZ, box.maxX, box.maxY, box.maxZ};
}

float angularFriction(bool onGround, float blockFriction, bool inLiquid) {
    return leash::angularFriction(onGround, blockFriction, inLiquid);
}

std::optional<std::array<double, 4>> leashPull(std::span<const double> self, std::span<const double> holder, bool quad,
                                               double slack, const Point& selfMotion, const Point& holderMotion,
                                               const Point& knownMotion) {
    const leash::Pull pull =
        leash::elasticPull(bodyOf(self), bodyOf(holder), quad, slack, selfMotion, holderMotion, knownMotion);
    if (!pull.pulled) {
        return std::nullopt;
    }
    return std::array<double, 4>{pull.torque, pull.push[0], pull.push[1], pull.push[2]};
}

std::vector<std::string> creativeTabItems(std::string_view tab) {
    const auto ids = registry::creativeTabItems(tab);
    return std::vector<std::string>(ids.begin(), ids.end());
}

std::optional<std::vector<std::uint8_t>> regionRead(const std::string& folder, std::int32_t chunkX,
                                                    std::int32_t chunkZ) {
    return world::folderRegions::read(folder, chunkX, chunkZ);
}

void regionWrite(const std::string& folder, std::int32_t chunkX, std::int32_t chunkZ, std::vector<std::uint8_t> data,
                 std::int32_t timestamp) {
    world::folderRegions::write(folder, chunkX, chunkZ, std::move(data), timestamp);
}

std::int32_t regionTimestamp(const std::string& folder, std::int32_t chunkX, std::int32_t chunkZ) {
    return world::folderRegions::timestamp(folder, chunkX, chunkZ);
}

bool regionFlush(const std::string& folder, bool release) {
    return release ? world::folderRegions::release(folder) : world::folderRegions::flush(folder);
}

BiomePoints overworldBiomes() {
    const auto points = world::climate::overworldBiomes::build();
    BiomePoints out;
    out.parameters.reserve(points.size() * 13);
    out.names.reserve(points.size());
    for (const auto& [point, name] : points) {
        for (const auto* axis : {&point.temperature, &point.humidity, &point.continentalness, &point.erosion,
                                 &point.depth, &point.weirdness}) {
            out.parameters.push_back(axis->min);
            out.parameters.push_back(axis->max);
        }
        out.parameters.push_back(point.offset);
        out.names.emplace_back(name);
    }
    return out;
}

} // namespace basalt::server::plugins::mechanics
