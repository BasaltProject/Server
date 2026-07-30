#include "JniSupport.hpp"

#include <basalt/server/plugins/PluginWorldEnv.hpp>

#include <algorithm>

namespace basalt::server::bukkit {
namespace {
using plugins::PluginBorder;
using plugins::PluginDrop;
using plugins::PluginParticle;
using plugins::PluginWorldEnv;
using plugins::PluginWorldRef;
using Enchantments = std::vector<std::pair<std::string, std::int32_t>>;

struct JavaRef {
    explicit JavaRef(JNIEnv* env, jobject object) : ref(env->NewGlobalRef(object)) {}

    JavaRef(const JavaRef&) = delete;
    JavaRef& operator=(const JavaRef&) = delete;

    ~JavaRef() {
        if (JNIEnv* env = BukkitBridge::liveEnv(); env != nullptr && ref != nullptr) {
            env->DeleteGlobalRef(ref);
        }
    }

    jobject ref;
};

[[nodiscard]] std::shared_ptr<PluginWorldRef> worldOf(JNIEnv* env, jstring name) {
    return BukkitBridge::current().services().worlds().byName(toStd(env, name));
}

[[nodiscard]] Enchantments enchantmentsOf(JNIEnv* env, jobjectArray names, jintArray levels) {
    Enchantments out;
    const std::vector<std::string> ids = jni::stringsOf(env, names);
    const jsize count = levels == nullptr ? 0 : env->GetArrayLength(levels);
    std::vector<jint> values(static_cast<std::size_t>(count));
    if (count > 0) {
        env->GetIntArrayRegion(levels, 0, count, values.data());
    }
    for (std::size_t at = 0; at < ids.size() && at < values.size(); ++at) {
        out.emplace_back(ids[at], values[at]);
    }
    return out;
}

void JNICALL nativeSound(JNIEnv* env, jclass, jstring world, jstring sound, jstring category, jdouble x, jdouble y,
                         jdouble z, jfloat volume, jfloat pitch, jlong seed) {
    const auto found = worldOf(env, world);
    const std::string name = toStd(env, sound);
    const std::string source = toStd(env, category);
    if (found == nullptr) {
        return;
    }
    static_cast<void>(jni::onServer<bool>(false, [&] {
        if (PluginWorldEnv* target = found->env(); target != nullptr) {
            target->sound(name, source, x, y, z, volume, pitch, seed);
        }
        return true;
    }));
}

void JNICALL nativeParticle(JNIEnv* env, jclass, jstring world, jstring type, jdouble x, jdouble y, jdouble z,
                            jint count, jdouble offsetX, jdouble offsetY, jdouble offsetZ, jdouble speedX,
                            jdouble speedY, jdouble speedZ, jint randomization, jboolean force, jdoubleArray data,
                            jstring item, jstring receiver) {
    const auto found = worldOf(env, world);
    if (found == nullptr) {
        return;
    }
    PluginParticle particle{
        .type = toStd(env, type),
        .x = x,
        .y = y,
        .z = z,
        .count = count,
        .offsetX = static_cast<float>(offsetX),
        .offsetY = static_cast<float>(offsetY),
        .offsetZ = static_cast<float>(offsetZ),
        .speed = {static_cast<float>(speedX), static_cast<float>(speedY), static_cast<float>(speedZ)},
        .randomization = randomization,
        .force = force == JNI_TRUE,
        .item = item == nullptr ? std::string{} : toStd(env, item),
        .receiver = receiver == nullptr ? std::string{} : toStd(env, receiver)};
    const jsize length = data == nullptr ? 0 : env->GetArrayLength(data);
    particle.data.resize(static_cast<std::size_t>(length));
    if (length > 0) {
        env->GetDoubleArrayRegion(data, 0, length, particle.data.data());
    }
    static_cast<void>(jni::onServer<bool>(false, [&] {
        if (PluginWorldEnv* target = found->env(); target != nullptr) {
            target->particle(particle);
        }
        return true;
    }));
}

void JNICALL nativeLevelEvent(JNIEnv* env, jclass, jstring world, jint type, jint x, jint y, jint z, jint data,
                              jboolean global) {
    const auto found = worldOf(env, world);
    if (found == nullptr) {
        return;
    }
    static_cast<void>(jni::onServer<bool>(false, [&] {
        if (PluginWorldEnv* target = found->env(); target != nullptr) {
            target->levelEvent(type, x, y, z, data, global == JNI_TRUE);
        }
        return true;
    }));
}

void JNICALL nativeBlockEvent(JNIEnv* env, jclass, jstring world, jint x, jint y, jint z, jint first, jint second) {
    const auto found = worldOf(env, world);
    if (found == nullptr) {
        return;
    }
    static_cast<void>(jni::onServer<bool>(false, [&] {
        if (PluginWorldEnv* target = found->env(); target != nullptr) {
            target->blockEvent(x, y, z, first, second);
        }
        return true;
    }));
}

jboolean JNICALL nativeSculk(JNIEnv* env, jclass, jstring world, jstring action, jint x, jint y, jint z, jint sourceX,
                             jint sourceY, jint sourceZ, jint value, jstring player) {
    const auto found = worldOf(env, world);
    if (found == nullptr) {
        return JNI_FALSE;
    }
    const std::string what = toStd(env, action);
    const std::string who = player == nullptr ? std::string{} : toStd(env, player);
    return jni::onServer<bool>(false,
                               [&] {
                                   PluginWorldEnv* target = found->env();
                                   return target != nullptr &&
                                          target->sculk(what, x, y, z, sourceX, sourceY, sourceZ, value, who);
                               })
               ? JNI_TRUE
               : JNI_FALSE;
}

jstring JNICALL nativeCreakingHeart(JNIEnv* env, jclass, jstring world, jstring action, jint x, jint y, jint z) {
    const auto found = worldOf(env, world);
    if (found == nullptr) {
        return nullptr;
    }
    const std::string what = toStd(env, action);
    const std::string answer = jni::onServer<std::string>(std::string{}, [&] {
        PluginWorldEnv* target = found->env();
        return target == nullptr ? std::string{} : target->creakingHeart(what, x, y, z);
    });
    return answer.empty() ? nullptr : env->NewStringUTF(answer.c_str());
}

jdoubleArray JNICALL nativeBorder(JNIEnv* env, jclass, jstring world) {
    const auto found = worldOf(env, world);
    if (found == nullptr) {
        return nullptr;
    }
    const std::optional<PluginBorder> border = jni::onServer<std::optional<PluginBorder>>(std::nullopt, [&] {
        PluginWorldEnv* target = found->env();
        return target == nullptr ? std::optional<PluginBorder>{} : std::optional<PluginBorder>{target->border()};
    });
    if (!border.has_value()) {
        return nullptr;
    }
    const std::array<double, 9> values{border->centerX,
                                       border->centerZ,
                                       border->size,
                                       border->targetSize,
                                       static_cast<double>(border->lerpTicks),
                                       border->damagePerBlock,
                                       border->damageBuffer,
                                       static_cast<double>(border->warningBlocks),
                                       static_cast<double>(border->warningSeconds)};
    jdoubleArray array = env->NewDoubleArray(static_cast<jsize>(values.size()));
    env->SetDoubleArrayRegion(array, 0, static_cast<jsize>(values.size()), values.data());
    return array;
}

void JNICALL nativeSetBorder(JNIEnv* env, jclass, jstring world, jdoubleArray values, jdouble seconds) {
    const auto found = worldOf(env, world);
    std::array<double, 9> raw{};
    if (found == nullptr || values == nullptr || env->GetArrayLength(values) < 9) {
        return;
    }
    env->GetDoubleArrayRegion(values, 0, 9, raw.data());
    const PluginBorder border{.centerX = raw[0],
                              .centerZ = raw[1],
                              .size = raw[2],
                              .targetSize = raw[3],
                              .lerpTicks = static_cast<std::int64_t>(raw[4]),
                              .damagePerBlock = raw[5],
                              .damageBuffer = raw[6],
                              .warningBlocks = static_cast<std::int32_t>(raw[7]),
                              .warningSeconds = static_cast<std::int32_t>(raw[8])};
    static_cast<void>(jni::onServer<bool>(false, [&] {
        if (PluginWorldEnv* target = found->env(); target != nullptr) {
            target->setBorder(border, seconds);
        }
        return true;
    }));
}

jint JNICALL nativePlayerCount(JNIEnv* env, jclass, jstring world) {
    const auto found = worldOf(env, world);
    return found == nullptr ? 0 : jni::onServer<jint>(0, [&] {
        PluginWorldEnv* target = found->env();
        return target == nullptr ? 0 : target->playerCount();
    });
}

jint JNICALL nativeBlockEntityCount(JNIEnv* env, jclass, jstring world) {
    const auto found = worldOf(env, world);
    return found == nullptr ? 0 : jni::onServer<jint>(0, [&] {
        PluginWorldEnv* target = found->env();
        return target == nullptr ? 0 : target->blockEntityCount();
    });
}

jobjectArray JNICALL nativeBlockDrops(JNIEnv* env, jclass, jstring world, jint x, jint y, jint z, jstring tool,
                                      jobjectArray names, jintArray levels, jboolean byEntity) {
    const auto found = worldOf(env, world);
    if (found == nullptr) {
        return nullptr;
    }
    const std::string item = tool == nullptr ? std::string{} : toStd(env, tool);
    const Enchantments enchantments = enchantmentsOf(env, names, levels);
    const std::optional<std::vector<PluginDrop>> drops =
        jni::onServer<std::optional<std::vector<PluginDrop>>>(std::nullopt, [&] {
            PluginWorldEnv* target = found->env();
            return target == nullptr ? std::optional<std::vector<PluginDrop>>{}
                                     : target->blockDrops(x, y, z, item, enchantments, byEntity == JNI_TRUE);
        });
    if (!drops.has_value()) {
        return nullptr;
    }
    std::vector<std::string> flat;
    for (const PluginDrop& drop : *drops) {
        flat.push_back(drop.item);
        flat.push_back(std::to_string(drop.count));
        flat.push_back(drop.components);
    }
    return jni::stringArrayOf(env, flat);
}

jboolean JNICALL nativeBreakBlock(JNIEnv* env, jclass, jstring world, jint x, jint y, jint z, jstring tool,
                                  jobjectArray names, jintArray levels, jboolean drops) {
    const auto found = worldOf(env, world);
    if (found == nullptr) {
        return JNI_FALSE;
    }
    const std::string item = tool == nullptr ? std::string{} : toStd(env, tool);
    const Enchantments enchantments = enchantmentsOf(env, names, levels);
    return jni::onServer<bool>(false,
                               [&] {
                                   PluginWorldEnv* target = found->env();
                                   return target != nullptr &&
                                          target->breakBlock(x, y, z, item, enchantments, drops == JNI_TRUE);
                               })
               ? JNI_TRUE
               : JNI_FALSE;
}

jboolean JNICALL nativeBoneMeal(JNIEnv* env, jclass, jstring world, jint x, jint y, jint z) {
    const auto found = worldOf(env, world);
    return found != nullptr && jni::onServer<bool>(false,
                                                   [&] {
                                                       PluginWorldEnv* target = found->env();
                                                       return target != nullptr && target->boneMeal(x, y, z);
                                                   })
               ? JNI_TRUE
               : JNI_FALSE;
}

jint JNICALL nativeSignal(JNIEnv* env, jclass, jstring world, jint x, jint y, jint z, jint face, jboolean direct) {
    const auto found = worldOf(env, world);
    return found == nullptr ? 0 : jni::onServer<jint>(0, [&] {
        PluginWorldEnv* target = found->env();
        return target == nullptr ? 0 : target->signalFrom(x, y, z, face, direct == JNI_TRUE);
    });
}

jboolean JNICALL nativeSurvives(JNIEnv* env, jclass, jstring world, jint x, jint y, jint z, jint state) {
    const auto found = worldOf(env, world);
    return found == nullptr || jni::onServer<bool>(true,
                                                   [&] {
                                                       PluginWorldEnv* target = found->env();
                                                       return target == nullptr || target->survives(x, y, z, state);
                                                   })
               ? JNI_TRUE
               : JNI_FALSE;
}

jdouble JNICALL nativeTemperature(JNIEnv* env, jclass, jstring world, jint x, jint y, jint z) {
    const auto found = worldOf(env, world);
    return found == nullptr ? 0.8 : jni::onServer<double>(0.8, [&] {
        PluginWorldEnv* target = found->env();
        return target == nullptr ? 0.8 : target->temperature(x, y, z);
    });
}

jdouble JNICALL nativeBreakSpeed(JNIEnv* env, jclass, jstring world, jstring player, jint x, jint y, jint z) {
    const auto found = worldOf(env, world);
    const std::string who = toStd(env, player);
    return found == nullptr ? 0.0 : jni::onServer<double>(0.0, [&] {
        PluginWorldEnv* target = found->env();
        return target == nullptr ? 0.0 : target->breakSpeed(who, x, y, z);
    });
}

jstring JNICALL nativeSetting(JNIEnv* env, jclass, jstring key) {
    const std::string name = toStd(env, key);
    const std::optional<std::string> value = jni::onServer<std::optional<std::string>>(
        std::nullopt, [&] { return BukkitBridge::current().services().serverControl().setting(name); });
    return value.has_value() ? toJava(env, *value) : nullptr;
}

jboolean JNICALL nativeSetSetting(JNIEnv* env, jclass, jstring key, jstring value) {
    const std::string name = toStd(env, key);
    const std::string text = toStd(env, value);
    return jni::onServer<bool>(
               false, [&] { return BukkitBridge::current().services().serverControl().setSetting(name, text); })
               ? JNI_TRUE
               : JNI_FALSE;
}

jboolean JNICALL nativeCreateWorld(JNIEnv* env, jclass, jstring name, jstring dimension, jstring type, jlong seed,
                                   jboolean seeded) {
    const std::string world = toStd(env, name);
    const std::string kind = toStd(env, dimension);
    const std::string shape = toStd(env, type);
    return jni::onServer<bool>(false,
                               [&] {
                                   return BukkitBridge::current().services().worlds().create(
                                              world, kind, shape,
                                              seeded == JNI_TRUE ? std::optional<std::int64_t>{seed} : std::nullopt) !=
                                          nullptr;
                               })
               ? JNI_TRUE
               : JNI_FALSE;
}

jboolean JNICALL nativeUnloadWorld(JNIEnv* env, jclass, jstring name, jboolean save) {
    const std::string world = toStd(env, name);
    return jni::onServer<bool>(
               false, [&] { return BukkitBridge::current().services().worlds().unload(world, save == JNI_TRUE); })
               ? JNI_TRUE
               : JNI_FALSE;
}

jboolean JNICALL nativeStopping(JNIEnv*, jclass) {
    BukkitBridge* bridge = BukkitBridge::live();
    return bridge != nullptr && bridge->stopping() ? JNI_TRUE : JNI_FALSE;
}

template <typename Each>
std::string joinWith(const char separator, const Each& each, const std::size_t count) {
    std::string out;
    for (std::size_t at = 0; at < count; ++at) {
        out += (at == 0 ? "" : std::string(1, separator)) + each(at);
    }
    return out;
}

jobjectArray JNICALL nativeStructureStarts(JNIEnv* env, jclass, jstring world, jint chunkX, jint chunkZ) {
    const auto found = worldOf(env, world);
    std::vector<std::string> rows;
    if (found != nullptr) {
        const auto starts = jni::onServer<std::vector<plugins::PluginStructureStart>>({}, [&] {
            PluginWorldEnv* target = found->env();
            return target == nullptr ? std::vector<plugins::PluginStructureStart>{}
                                     : target->structureStarts(chunkX, chunkZ);
        });
        const auto box = [](const std::array<std::int32_t, 6>& bounds) {
            return joinWith(',', [&bounds](std::size_t at) { return std::to_string(bounds[at]); }, bounds.size());
        };
        for (const plugins::PluginStructureStart& start : starts) {
            rows.push_back(start.structure + "|" + std::to_string(start.chunkX) + "|" + std::to_string(start.chunkZ) +
                           "|" + box(start.bounds) + "|" +
                           joinWith(';', [&](std::size_t at) { return box(start.pieces[at]); }, start.pieces.size()));
        }
    }
    return jni::stringArrayOf(env, rows);
}

jobjectArray JNICALL nativeRaids(JNIEnv* env, jclass, jstring world) {
    const auto found = worldOf(env, world);
    std::vector<std::string> rows;
    if (found != nullptr) {
        const auto raids = jni::onServer<std::vector<plugins::PluginRaid>>({}, [&] {
            PluginWorldEnv* target = found->env();
            return target == nullptr ? std::vector<plugins::PluginRaid>{} : target->raids();
        });
        for (const plugins::PluginRaid& raid : raids) {
            rows.push_back(
                std::to_string(raid.id) + "|" + (raid.started ? "1" : "0") + "|" + std::to_string(raid.ticksActive) +
                "|" + std::to_string(raid.omenLevel) + "|" + std::to_string(raid.center[0]) + "," +
                std::to_string(raid.center[1]) + "," + std::to_string(raid.center[2]) + "|" + raid.status + "|" +
                std::to_string(raid.groupsSpawned) + "|" + std::to_string(raid.totalGroups) + "|" +
                std::to_string(raid.totalHealth) + "|" +
                joinWith(
                    ',', [&](std::size_t at) { return raid.heroes[at]; }, raid.heroes.size()) +
                "|" +
                joinWith(',', [&](std::size_t at) { return std::to_string(raid.raiders[at]); }, raid.raiders.size()));
        }
    }
    return jni::stringArrayOf(env, rows);
}

jboolean JNICALL nativeSetRaid(JNIEnv* env, jclass, jstring world, jint id, jint omen, jint groups) {
    const auto found = worldOf(env, world);
    return found != nullptr && jni::onServer<bool>(false,
                                                   [&] {
                                                       PluginWorldEnv* target = found->env();
                                                       return target != nullptr && target->setRaid(id, omen, groups);
                                                   })
               ? JNI_TRUE
               : JNI_FALSE;
}

jstring JNICALL nativeFluidAt(JNIEnv* env, jclass, jstring world, jint x, jint y, jint z) {
    const auto found = worldOf(env, world);
    if (found == nullptr) {
        return nullptr;
    }
    const plugins::PluginFluid fluid = jni::onServer<plugins::PluginFluid>({}, [&] {
        PluginWorldEnv* target = found->env();
        return target == nullptr ? plugins::PluginFluid{} : target->fluidAt(x, y, z);
    });
    return toJava(env, fluid.fluid + "|" + (fluid.source ? "1" : "0") + "|" + std::to_string(fluid.level) + "|" +
                           (fluid.falling ? "1" : "0"));
}

jdoubleArray JNICALL nativeFluidFlow(JNIEnv* env, jclass, jstring world, jint x, jint y, jint z) {
    const auto found = worldOf(env, world);
    if (found == nullptr) {
        return nullptr;
    }
    const std::array<double, 3> flow = jni::onServer<std::array<double, 3>>({}, [&] {
        PluginWorldEnv* target = found->env();
        return target == nullptr ? std::array<double, 3>{} : target->fluidFlow(x, y, z);
    });
    jdoubleArray array = env->NewDoubleArray(3);
    env->SetDoubleArrayRegion(array, 0, 3, flow.data());
    return array;
}

jintArray JNICALL nativeLightningTarget(JNIEnv* env, jclass, jstring world, jint x, jint z) {
    const auto found = worldOf(env, world);
    if (found == nullptr) {
        return nullptr;
    }
    const auto at = jni::onServer<std::optional<std::array<std::int32_t, 3>>>(std::nullopt, [&] {
        PluginWorldEnv* target = found->env();
        return target == nullptr ? std::optional<std::array<std::int32_t, 3>>{} : target->lightningTarget(x, z);
    });
    if (!at.has_value()) {
        return nullptr;
    }
    jintArray array = env->NewIntArray(3);
    env->SetIntArrayRegion(array, 0, 3, at->data());
    return array;
}

void JNICALL nativeGameEvent(JNIEnv* env, jclass, jstring world, jstring event, jdouble x, jdouble y, jdouble z,
                             jstring source) {
    const auto found = worldOf(env, world);
    const std::string name = toStd(env, event);
    const std::string who = source == nullptr ? std::string{} : toStd(env, source);
    if (found == nullptr) {
        return;
    }
    static_cast<void>(jni::onServer<bool>(false, [&] {
        if (PluginWorldEnv* target = found->env(); target != nullptr) {
            target->gameEvent(name, x, y, z, who);
        }
        return true;
    }));
}

jboolean JNICALL nativeGrowTree(JNIEnv* env, jclass, jstring world, jint x, jint y, jint z, jstring sapling) {
    const auto found = worldOf(env, world);
    const std::string name = toStd(env, sapling);
    return found != nullptr && jni::onServer<bool>(false,
                                                   [&] {
                                                       PluginWorldEnv* target = found->env();
                                                       return target != nullptr && target->growTree(x, y, z, name);
                                                   })
               ? JNI_TRUE
               : JNI_FALSE;
}

jobjectArray JNICALL nativePointsOfInterest(JNIEnv* env, jclass, jstring world, jobjectArray types, jint x, jint y,
                                            jint z, jint radius, jint occupancy) {
    const auto found = worldOf(env, world);
    std::vector<std::string> rows;
    if (found != nullptr) {
        const std::vector<std::string> wanted = jni::stringsOf(env, types);
        const auto records = jni::onServer<std::vector<plugins::PluginPoi>>({}, [&] {
            PluginWorldEnv* target = found->env();
            return target == nullptr ? std::vector<plugins::PluginPoi>{}
                                     : target->pointsOfInterest(wanted, x, y, z, radius, occupancy);
        });
        for (const plugins::PluginPoi& poi : records) {
            rows.push_back(poi.type + "|" + std::to_string(poi.x) + "|" + std::to_string(poi.y) + "|" +
                           std::to_string(poi.z) + "|" + std::to_string(poi.freeTickets));
        }
    }
    return jni::stringArrayOf(env, rows);
}

jintArray JNICALL nativeSpawnTunables(JNIEnv* env, jclass, jstring world) {
    const auto found = worldOf(env, world);
    if (found == nullptr) {
        return nullptr;
    }
    const plugins::PluginSpawnTunables held = jni::onServer<plugins::PluginSpawnTunables>({}, [&] {
        PluginWorldEnv* target = found->env();
        return target == nullptr ? plugins::PluginSpawnTunables{} : target->spawnTunables();
    });
    std::array<jint, 18> values{};
    std::ranges::copy(held.limits, values.begin());
    std::ranges::copy(held.ticksPer, values.begin() + 8);
    values[16] = held.allowMonsters ? 1 : 0;
    values[17] = held.allowAnimals ? 1 : 0;
    jintArray array = env->NewIntArray(18);
    env->SetIntArrayRegion(array, 0, 18, values.data());
    return array;
}

void JNICALL nativeSetSpawnTunables(JNIEnv* env, jclass, jstring world, jintArray values) {
    const auto found = worldOf(env, world);
    std::array<jint, 18> raw{};
    if (found == nullptr || values == nullptr || env->GetArrayLength(values) < 18) {
        return;
    }
    env->GetIntArrayRegion(values, 0, 18, raw.data());
    plugins::PluginSpawnTunables held;
    std::copy_n(raw.begin(), 8, held.limits.begin());
    std::copy_n(raw.begin() + 8, 8, held.ticksPer.begin());
    held.allowMonsters = raw[16] != 0;
    held.allowAnimals = raw[17] != 0;
    static_cast<void>(jni::onServer<bool>(false, [&] {
        if (PluginWorldEnv* target = found->env(); target != nullptr) {
            target->setSpawnTunables(held);
        }
        return true;
    }));
}

jint JNICALL nativeViewDistance(JNIEnv* env, jclass, jstring world, jint set) {
    const auto found = worldOf(env, world);
    return found == nullptr ? 10 : jni::onServer<jint>(10, [&] {
        PluginWorldEnv* target = found->env();
        if (target == nullptr) {
            return 10;
        }
        if (set > 0) {
            target->setViewDistance(set);
        }
        return target->viewDistance();
    });
}

jintArray JNICALL nativeWorldFacts(JNIEnv* env, jclass, jstring world) {
    const auto found = worldOf(env, world);
    if (found == nullptr) {
        return nullptr;
    }
    const std::array<jint, 4> values = jni::onServer<std::array<jint, 4>>({}, [&] {
        PluginWorldEnv* target = found->env();
        if (target == nullptr) {
            return std::array<jint, 4>{63, 0, 0, 0};
        }
        const std::array<std::int32_t, 3> times = target->weatherTimes();
        return std::array<jint, 4>{target->seaLevel(), times[0], times[1], times[2]};
    });
    jintArray array = env->NewIntArray(4);
    env->SetIntArrayRegion(array, 0, 4, values.data());
    return array;
}

void JNICALL nativeSetWeatherTimes(JNIEnv* env, jclass, jstring world, jint rain, jint thunder, jint clear) {
    const auto found = worldOf(env, world);
    if (found == nullptr) {
        return;
    }
    static_cast<void>(jni::onServer<bool>(false, [&] {
        if (PluginWorldEnv* target = found->env(); target != nullptr) {
            target->setWeatherTimes(rain, thunder, clear);
        }
        return true;
    }));
}

jintArray JNICALL nativeForcedChunks(JNIEnv* env, jclass, jstring world) {
    const auto found = worldOf(env, world);
    if (found == nullptr) {
        return nullptr;
    }
    const std::vector<std::int32_t> flat = jni::onServer<std::vector<std::int32_t>>({}, [&] {
        PluginWorldEnv* target = found->env();
        return target == nullptr ? std::vector<std::int32_t>{} : target->forcedChunks();
    });
    jintArray array = env->NewIntArray(static_cast<jsize>(flat.size()));
    env->SetIntArrayRegion(array, 0, static_cast<jsize>(flat.size()), flat.data());
    return array;
}

void JNICALL nativeSetChunkForced(JNIEnv* env, jclass, jstring world, jint chunkX, jint chunkZ, jboolean forced) {
    const auto found = worldOf(env, world);
    if (found == nullptr) {
        return;
    }
    static_cast<void>(jni::onServer<bool>(false, [&] {
        if (PluginWorldEnv* target = found->env(); target != nullptr) {
            target->setChunkForced(chunkX, chunkZ, forced == JNI_TRUE);
        }
        return true;
    }));
}

jlong JNICALL nativeInhabitedTime(JNIEnv* env, jclass, jstring world, jint chunkX, jint chunkZ, jlong set) {
    const auto found = worldOf(env, world);
    return found == nullptr ? 0 : jni::onServer<jlong>(0, [&] {
        PluginWorldEnv* target = found->env();
        if (target == nullptr) {
            return jlong{0};
        }
        if (set >= 0) {
            target->setInhabitedTime(chunkX, chunkZ, set);
        }
        return static_cast<jlong>(target->inhabitedTime(chunkX, chunkZ));
    });
}

jobjectArray JNICALL nativeGameRuleSpecs(JNIEnv* env, jclass) {
    return jni::stringArrayOf(env, BukkitBridge::current().services().worlds().gameRuleSpecs());
}

jint JNICALL nativeGameEventFrequency(JNIEnv* env, jclass, jstring event) {
    return BukkitBridge::current().services().worlds().gameEventFrequency(toStd(env, event));
}

void JNICALL nativeCommandReply(JNIEnv* env, jclass, jstring message) {
    BukkitBridge::replyToCommand(toStd(env, message));
}

jboolean JNICALL nativeUseGenerator(JNIEnv* env, jclass, jstring world) {
    const auto found = worldOf(env, world);
    const std::string name = toStd(env, world);
    jclass local = env->FindClass("org/basaltmc/bukkit/worldgen/CustomGenerators");
    if (found == nullptr || local == nullptr) {
        env->ExceptionClear();
        return JNI_FALSE;
    }
    const auto holder = std::make_shared<JavaRef>(env, local);
    const jmethodID fill = env->GetStaticMethodID(local, "fill", "(Ljava/lang/String;II)[I");
    if (fill == nullptr) {
        env->ExceptionClear();
        return JNI_FALSE;
    }
    return jni::onServer<bool>(
               false,
               [&] {
                   PluginWorldEnv* target = found->env();
                   return target != nullptr &&
                          target->useChunkFill([holder, fill, name](std::int32_t chunkX, std::int32_t chunkZ,
                                                                    std::vector<std::int32_t>& blocks,
                                                                    std::vector<std::int32_t>& biomes) {
                              JNIEnv* callEnv = BukkitBridge::liveEnv();
                              if (callEnv == nullptr) {
                                  return;
                              }
                              jstring who = toJava(callEnv, name);
                              auto* packed = static_cast<jintArray>(callEnv->CallStaticObjectMethod(
                                  static_cast<jclass>(holder->ref), fill, who, chunkX, chunkZ));
                              callEnv->DeleteLocalRef(who);
                              if (callEnv->ExceptionCheck() == JNI_TRUE) {
                                  callEnv->ExceptionDescribe();
                                  callEnv->ExceptionClear();
                                  return;
                              }
                              if (packed == nullptr) {
                                  return;
                              }
                              std::vector<jint> all(static_cast<std::size_t>(callEnv->GetArrayLength(packed)));
                              callEnv->GetIntArrayRegion(packed, 0, static_cast<jsize>(all.size()), all.data());
                              callEnv->DeleteLocalRef(packed);
                              if (all.size() < 2) {
                                  return;
                              }
                              const auto blockCount = static_cast<std::size_t>(all[0]);
                              const auto biomeCount = static_cast<std::size_t>(all[1]);
                              if (all.size() < 2 + blockCount + biomeCount) {
                                  return;
                              }
                              blocks.assign(all.begin() + 2, all.begin() + 2 + static_cast<std::ptrdiff_t>(blockCount));
                              biomes.assign(all.begin() + 2 + static_cast<std::ptrdiff_t>(blockCount),
                                            all.begin() + 2 + static_cast<std::ptrdiff_t>(blockCount + biomeCount));
                          });
               })
               ? JNI_TRUE
               : JNI_FALSE;
}

jint JNICALL nativeTransformState(JNIEnv*, jclass, jint state, jint rotation, jint mirror) {
    return BukkitBridge::current().services().worlds().transformState(state, rotation, mirror);
}

jdoubleArray JNICALL nativeToolFacts(JNIEnv* env, jclass, jstring item, jint state) {
    const std::vector<double> facts = BukkitBridge::current().services().worlds().toolFacts(
        item == nullptr ? std::string{} : toStd(env, item), state);
    if (facts.empty()) {
        return nullptr;
    }
    jdoubleArray array = env->NewDoubleArray(static_cast<jsize>(facts.size()));
    env->SetDoubleArrayRegion(array, 0, static_cast<jsize>(facts.size()), facts.data());
    return array;
}
} // namespace

void registerWorldEnvNatives(JNIEnv* env, jclass natives) {
    const auto method = [](const char* name, const char* signature, void* function) {
        return JNINativeMethod{const_cast<char*>(name), const_cast<char*>(signature), function};
    };
    const std::array methods{
        method("sound", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;DDDFFJ)V",
               reinterpret_cast<void*>(&nativeSound)),
        method("particle", "(Ljava/lang/String;Ljava/lang/String;DDDIDDDDDDIZ[DLjava/lang/String;Ljava/lang/String;)V",
               reinterpret_cast<void*>(&nativeParticle)),
        method("levelEvent", "(Ljava/lang/String;IIIIIZ)V", reinterpret_cast<void*>(&nativeLevelEvent)),
        method("blockEvent", "(Ljava/lang/String;IIIII)V", reinterpret_cast<void*>(&nativeBlockEvent)),
        method("creakingHeart", "(Ljava/lang/String;Ljava/lang/String;III)Ljava/lang/String;",
               reinterpret_cast<void*>(&nativeCreakingHeart)),
        method("sculk", "(Ljava/lang/String;Ljava/lang/String;IIIIIIILjava/lang/String;)Z",
               reinterpret_cast<void*>(&nativeSculk)),
        method("border", "(Ljava/lang/String;)[D", reinterpret_cast<void*>(&nativeBorder)),
        method("setBorder", "(Ljava/lang/String;[DD)V", reinterpret_cast<void*>(&nativeSetBorder)),
        method("playerCount", "(Ljava/lang/String;)I", reinterpret_cast<void*>(&nativePlayerCount)),
        method("blockEntityCount", "(Ljava/lang/String;)I", reinterpret_cast<void*>(&nativeBlockEntityCount)),
        method("blockDrops", "(Ljava/lang/String;IIILjava/lang/String;[Ljava/lang/String;[IZ)[Ljava/lang/String;",
               reinterpret_cast<void*>(&nativeBlockDrops)),
        method("breakBlock", "(Ljava/lang/String;IIILjava/lang/String;[Ljava/lang/String;[IZ)Z",
               reinterpret_cast<void*>(&nativeBreakBlock)),
        method("boneMeal", "(Ljava/lang/String;III)Z", reinterpret_cast<void*>(&nativeBoneMeal)),
        method("signal", "(Ljava/lang/String;IIIIZ)I", reinterpret_cast<void*>(&nativeSignal)),
        method("survives", "(Ljava/lang/String;IIII)Z", reinterpret_cast<void*>(&nativeSurvives)),
        method("temperature", "(Ljava/lang/String;III)D", reinterpret_cast<void*>(&nativeTemperature)),
        method("breakSpeed", "(Ljava/lang/String;Ljava/lang/String;III)D", reinterpret_cast<void*>(&nativeBreakSpeed)),
        method("setting", "(Ljava/lang/String;)Ljava/lang/String;", reinterpret_cast<void*>(&nativeSetting)),
        method("setSetting", "(Ljava/lang/String;Ljava/lang/String;)Z", reinterpret_cast<void*>(&nativeSetSetting)),
        method("createWorld", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;JZ)Z",
               reinterpret_cast<void*>(&nativeCreateWorld)),
        method("unloadWorld", "(Ljava/lang/String;Z)Z", reinterpret_cast<void*>(&nativeUnloadWorld)),
        method("stopping", "()Z", reinterpret_cast<void*>(&nativeStopping)),
        method("structureStarts", "(Ljava/lang/String;II)[Ljava/lang/String;",
               reinterpret_cast<void*>(&nativeStructureStarts)),
        method("raids", "(Ljava/lang/String;)[Ljava/lang/String;", reinterpret_cast<void*>(&nativeRaids)),
        method("setRaid", "(Ljava/lang/String;III)Z", reinterpret_cast<void*>(&nativeSetRaid)),
        method("fluidAt", "(Ljava/lang/String;III)Ljava/lang/String;", reinterpret_cast<void*>(&nativeFluidAt)),
        method("fluidFlow", "(Ljava/lang/String;III)[D", reinterpret_cast<void*>(&nativeFluidFlow)),
        method("lightningTarget", "(Ljava/lang/String;II)[I", reinterpret_cast<void*>(&nativeLightningTarget)),
        method("gameEvent", "(Ljava/lang/String;Ljava/lang/String;DDDLjava/lang/String;)V",
               reinterpret_cast<void*>(&nativeGameEvent)),
        method("growTree", "(Ljava/lang/String;IIILjava/lang/String;)Z", reinterpret_cast<void*>(&nativeGrowTree)),
        method("pointsOfInterest", "(Ljava/lang/String;[Ljava/lang/String;IIIII)[Ljava/lang/String;",
               reinterpret_cast<void*>(&nativePointsOfInterest)),
        method("spawnTunables", "(Ljava/lang/String;)[I", reinterpret_cast<void*>(&nativeSpawnTunables)),
        method("setSpawnTunables", "(Ljava/lang/String;[I)V", reinterpret_cast<void*>(&nativeSetSpawnTunables)),
        method("viewDistance", "(Ljava/lang/String;I)I", reinterpret_cast<void*>(&nativeViewDistance)),
        method("worldFacts", "(Ljava/lang/String;)[I", reinterpret_cast<void*>(&nativeWorldFacts)),
        method("setWeatherTimes", "(Ljava/lang/String;III)V", reinterpret_cast<void*>(&nativeSetWeatherTimes)),
        method("forcedChunks", "(Ljava/lang/String;)[I", reinterpret_cast<void*>(&nativeForcedChunks)),
        method("setChunkForced", "(Ljava/lang/String;IIZ)V", reinterpret_cast<void*>(&nativeSetChunkForced)),
        method("inhabitedTime", "(Ljava/lang/String;IIJ)J", reinterpret_cast<void*>(&nativeInhabitedTime)),
        method("gameRuleSpecs", "()[Ljava/lang/String;", reinterpret_cast<void*>(&nativeGameRuleSpecs)),
        method("gameEventFrequency", "(Ljava/lang/String;)I", reinterpret_cast<void*>(&nativeGameEventFrequency)),
        method("commandReply", "(Ljava/lang/String;)V", reinterpret_cast<void*>(&nativeCommandReply)),
        method("useGenerator", "(Ljava/lang/String;)Z", reinterpret_cast<void*>(&nativeUseGenerator)),
        method("transformState", "(III)I", reinterpret_cast<void*>(&nativeTransformState)),
        method("toolFacts", "(Ljava/lang/String;I)[D", reinterpret_cast<void*>(&nativeToolFacts)),
    };
    if (env->RegisterNatives(natives, methods.data(), static_cast<jint>(methods.size())) != 0) {
        BukkitBridge::current().log().error(
            [] { return std::string{"WorldEnvNatives has a native the runtime does not declare"}; });
        env->ExceptionDescribe();
        env->ExceptionClear();
    }
}
} // namespace basalt::server::bukkit
