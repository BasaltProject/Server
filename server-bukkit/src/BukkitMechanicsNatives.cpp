#include "JniSupport.hpp"
#include "NativeTable.hpp"

#include <basalt/server/entity/Breeding.hpp>
#include <basalt/server/entity/Collision.hpp>
#include <basalt/server/entity/Combat.hpp>
#include <basalt/server/entity/Leash.hpp>
#include <basalt/server/entity/LevelValue.hpp>
#include <basalt/server/entity/Projectiles.hpp>
#include <basalt/server/plugins/PluginWorldEnv.hpp>

#include <array>

namespace basalt::server::bukkit {
namespace {
namespace combat = entity::combat;
namespace geometry = entity::geometry;
namespace projectile = entity::projectile;
namespace leash = entity::leash;

constexpr std::size_t kCandidateStride = 9;

std::vector<double> doublesOf(JNIEnv* env, jdoubleArray array) {
    std::vector<double> out(array == nullptr ? 0 : static_cast<std::size_t>(env->GetArrayLength(array)));
    if (!out.empty()) {
        env->GetDoubleArrayRegion(array, 0, static_cast<jsize>(out.size()), out.data());
    }
    return out;
}

jdoubleArray arrayOfDoubles(JNIEnv* env, std::span<const double> values) {
    jdoubleArray out = env->NewDoubleArray(static_cast<jsize>(values.size()));
    if (out != nullptr) {
        env->SetDoubleArrayRegion(out, 0, static_cast<jsize>(values.size()), values.data());
    }
    return out;
}

std::vector<projectile::Candidate> candidatesOf(JNIEnv* env, jdoubleArray flat) {
    const std::vector<double> raw = doublesOf(env, flat);
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

jdoubleArray hitArray(JNIEnv* env, const projectile::Hit& hit) {
    if (hit.index < 0) {
        return nullptr;
    }
    const std::array<double, 4> out{static_cast<double>(hit.index), hit.at[0], hit.at[1], hit.at[2]};
    return arrayOfDoubles(env, out);
}

geometry::Point pointOf(const std::vector<double>& values, std::size_t at = 0) {
    return values.size() < at + 3 ? geometry::Point{} : geometry::Point{values[at], values[at + 1], values[at + 2]};
}

leash::Body bodyOf(const std::vector<double>& values) {
    leash::Body body;
    if (values.size() >= 6) {
        body.position = pointOf(values);
        body.width = static_cast<float>(values[3]);
        body.height = static_cast<float>(values[4]);
        body.yaw = static_cast<float>(values[5]);
    }
    return body;
}

std::vector<entity::collision::Collider> collidersOf(JNIEnv* env, jdoubleArray flat) {
    const std::vector<double> raw = doublesOf(env, flat);
    std::vector<entity::collision::Collider> out;
    for (std::size_t at = 0; at + 7 <= raw.size(); at += 7) {
        out.push_back({geometry::makeBox(raw[at], raw[at + 1], raw[at + 2], raw[at + 3], raw[at + 4], raw[at + 5]),
                       static_cast<std::int32_t>(raw[at + 6])});
    }
    return out;
}

geometry::Box boxOf(const std::vector<double>& raw) {
    return raw.size() < 6 ? geometry::Box{} : geometry::makeBox(raw[0], raw[1], raw[2], raw[3], raw[4], raw[5]);
}

jdoubleArray JNICALL nativeCollide(JNIEnv* env, jclass, jdoubleArray colliders, jdoubleArray box, jdouble mx,
                                   jdouble my, jdouble mz) {
    const geometry::Point swept =
        entity::collision::sweep(collidersOf(env, colliders), boxOf(doublesOf(env, box)), {mx, my, mz});
    return arrayOfDoubles(env, swept);
}

jdoubleArray JNICALL nativeStepUp(JNIEnv* env, jclass, jdoubleArray colliders, jdoubleArray grounded,
                                  jdouble groundedOffsetY, jdouble mx, jdouble my, jdouble mz, jfloat maxUpStep,
                                  jdouble reachSquared) {
    const auto stepped = entity::collision::stepUp(collidersOf(env, colliders), boxOf(doublesOf(env, grounded)),
                                                   groundedOffsetY, {mx, my, mz}, maxUpStep, reachSquared);
    return stepped.has_value() ? arrayOfDoubles(env, *stepped) : nullptr;
}

jdouble JNICALL nativeOffspringAttribute(JNIEnv*, jclass, jdouble parentA, jdouble parentB, jdouble min, jdouble max,
                                         jdouble first, jdouble second, jdouble third) {
    return entity::breeding::offspringAttribute(parentA, parentB, min, max, first, second, third);
}

jfloat JNICALL nativeLevelValue(JNIEnv* env, jclass, jdoubleArray program, jint level) {
    return entity::levelValue::evaluate(doublesOf(env, program), level);
}

jintArray JNICALL nativeBeaconScan(JNIEnv* env, jclass, jstring world, jint x, jint y, jint z) {
    const std::string name = toStd(env, world);
    const plugins::PluginBeacon scan = jni::onServer<plugins::PluginBeacon>({}, [&] {
        const auto found = BukkitBridge::current().services().worlds().byName(name);
        plugins::PluginWorldEnv* target = found == nullptr ? nullptr : found->env();
        return target == nullptr ? plugins::PluginBeacon{} : target->beaconScan(x, y, z);
    });
    std::vector<jint> flat{scan.levels};
    for (const auto& section : scan.sections) {
        flat.push_back(section[0]);
        flat.push_back(section[1]);
    }
    jintArray out = env->NewIntArray(static_cast<jsize>(flat.size()));
    if (out != nullptr) {
        env->SetIntArrayRegion(out, 0, static_cast<jsize>(flat.size()), flat.data());
    }
    return out;
}

jfloat JNICALL nativeAfterArmor(JNIEnv*, jclass, jfloat damage, jfloat armor, jfloat toughness, jfloat fractionAdd) {
    return combat::afterArmour(damage, armor, toughness, fractionAdd);
}

jboolean JNICALL nativeScalesWithDifficulty(JNIEnv* env, jclass, jstring scaling, jboolean livingNonPlayer) {
    return combat::scalesWithDifficulty(toStd(env, scaling), livingNonPlayer == JNI_TRUE) ? JNI_TRUE : JNI_FALSE;
}

jfloat JNICALL nativeAfterProtection(JNIEnv*, jclass, jfloat damage, jfloat protection) {
    return combat::afterProtection(damage, protection);
}

jfloat JNICALL nativeAfterResistance(JNIEnv*, jclass, jfloat damage, jint amplifier) {
    return combat::afterResistance(damage, amplifier);
}

jfloatArray JNICALL nativeAbsorb(JNIEnv* env, jclass, jfloat damage, jfloat absorption) {
    const combat::Absorbed split = combat::absorb(damage, absorption);
    const std::array<jfloat, 2> out{split.health, split.absorption};
    jfloatArray array = env->NewFloatArray(2);
    if (array != nullptr) {
        env->SetFloatArrayRegion(array, 0, 2, out.data());
    }
    return array;
}

jfloatArray JNICALL nativeRotateTowards(JNIEnv* env, jclass, jdouble mx, jdouble my, jdouble mz, jfloat xRotO,
                                        jfloat yRotO, jfloat xRot, jfloat yRot, jfloat speed) {
    const auto turned = projectile::rotateTowardsMovement(mx, my, mz, xRotO, yRotO, xRot, yRot, speed);
    if (!turned.has_value()) {
        return nullptr;
    }
    const std::array<jfloat, 4> out{turned->xRotO, turned->yRotO, turned->xRot, turned->yRot};
    jfloatArray array = env->NewFloatArray(4);
    if (array != nullptr) {
        env->SetFloatArrayRegion(array, 0, 4, out.data());
    }
    return array;
}

jdoubleArray JNICALL nativeClosestHit(JNIEnv* env, jclass, jdoubleArray candidates, jdouble fx, jdouble fy, jdouble fz,
                                      jdouble tx, jdouble ty, jdouble tz, jfloat margin) {
    const auto list = candidatesOf(env, candidates);
    return hitArray(env, projectile::closestHit(list, {fx, fy, fz}, {tx, ty, tz}, margin));
}

jdoubleArray JNICALL nativePickedHit(JNIEnv* env, jclass, jdoubleArray candidates, jint root, jdouble fx, jdouble fy,
                                     jdouble fz, jdouble tx, jdouble ty, jdouble tz, jdouble maxValue) {
    const auto list = candidatesOf(env, candidates);
    return hitArray(env, projectile::pickedHit(list, root, {fx, fy, fz}, {tx, ty, tz}, maxValue));
}

jfloat JNICALL nativeProjectileMargin(JNIEnv*, jclass, jint tickCount) { return projectile::margin(tickCount); }

jdoubleArray JNICALL nativeShulkerBox(JNIEnv* env, jclass, jfloat size, jint direction, jfloat progressFrom,
                                      jfloat progressTo, jdouble x, jdouble y, jdouble z) {
    const geometry::Box box = projectile::shulkerProgressBox(size, direction, progressFrom, progressTo, {x, y, z});
    const std::array<double, 6> out{box.minX, box.minY, box.minZ, box.maxX, box.maxY, box.maxZ};
    return arrayOfDoubles(env, out);
}

jfloat JNICALL nativeAngularFriction(JNIEnv*, jclass, jboolean onGround, jfloat blockFriction, jboolean inLiquid) {
    return leash::angularFriction(onGround == JNI_TRUE, blockFriction, inLiquid == JNI_TRUE);
}

jdoubleArray JNICALL nativeLeashPull(JNIEnv* env, jclass, jdoubleArray self, jdoubleArray holder, jboolean quad,
                                     jdouble slack, jdoubleArray selfMotion, jdoubleArray holderMotion,
                                     jdoubleArray knownMotion) {
    const leash::Pull pull =
        leash::elasticPull(bodyOf(doublesOf(env, self)), bodyOf(doublesOf(env, holder)), quad == JNI_TRUE, slack,
                           pointOf(doublesOf(env, selfMotion)), pointOf(doublesOf(env, holderMotion)),
                           pointOf(doublesOf(env, knownMotion)));
    if (!pull.pulled) {
        return nullptr;
    }
    const std::array<double, 4> out{pull.torque, pull.push[0], pull.push[1], pull.push[2]};
    return arrayOfDoubles(env, out);
}
} // namespace

void registerMechanicsNatives(JNIEnv* env, jclass natives) {
    const std::array methods{
        nativeMethod("afterArmor", "(FFFF)F", reinterpret_cast<void*>(&nativeAfterArmor)),
        nativeMethod("afterProtection", "(FF)F", reinterpret_cast<void*>(&nativeAfterProtection)),
        nativeMethod("scalesWithDifficulty", "(Ljava/lang/String;Z)Z",
                     reinterpret_cast<void*>(&nativeScalesWithDifficulty)),
        nativeMethod("afterResistance", "(FI)F", reinterpret_cast<void*>(&nativeAfterResistance)),
        nativeMethod("absorb", "(FF)[F", reinterpret_cast<void*>(&nativeAbsorb)),
        nativeMethod("rotateTowardsMovement", "(DDDFFFFF)[F", reinterpret_cast<void*>(&nativeRotateTowards)),
        nativeMethod("closestHit", "([DDDDDDDF)[D", reinterpret_cast<void*>(&nativeClosestHit)),
        nativeMethod("pickedHit", "([DIDDDDDDD)[D", reinterpret_cast<void*>(&nativePickedHit)),
        nativeMethod("projectileMargin", "(I)F", reinterpret_cast<void*>(&nativeProjectileMargin)),
        nativeMethod("shulkerBox", "(FIFFDDD)[D", reinterpret_cast<void*>(&nativeShulkerBox)),
        nativeMethod("angularFriction", "(ZFZ)F", reinterpret_cast<void*>(&nativeAngularFriction)),
        nativeMethod("collide", "([D[DDDD)[D", reinterpret_cast<void*>(&nativeCollide)),
        nativeMethod("stepUp", "([D[DDDDDFD)[D", reinterpret_cast<void*>(&nativeStepUp)),
        nativeMethod("offspringAttribute", "(DDDDDDD)D", reinterpret_cast<void*>(&nativeOffspringAttribute)),
        nativeMethod("levelValue", "([DI)F", reinterpret_cast<void*>(&nativeLevelValue)),
        nativeMethod("beaconScan", "(Ljava/lang/String;III)[I", reinterpret_cast<void*>(&nativeBeaconScan)),
        nativeMethod("leashPull", "([D[DZD[D[D[D)[D", reinterpret_cast<void*>(&nativeLeashPull)),
    };
    registerMethods(env, natives, methods.data(), static_cast<jint>(methods.size()));
}
} // namespace basalt::server::bukkit
