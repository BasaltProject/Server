#include "BukkitWorldEnvShared.hpp"

#include <basalt/server/plugins/PluginWorldEnv.hpp>

#include <algorithm>

namespace basalt::server::bukkit {
namespace {
using plugins::PluginBorder;
using plugins::PluginDrop;
using plugins::PluginParticle;
using plugins::PluginWorldEnv;

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

jobjectArray JNICALL nativeRaids(JNIEnv* env, jclass, jstring world) {
    const auto found = envWorldOf(env, world);
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

jobjectArray JNICALL nativeRaidBar(JNIEnv* env, jclass, jstring world, jint id) {
    const auto found = envWorldOf(env, world);
    if (found == nullptr) {
        return nullptr;
    }
    const auto raids = jni::onServer<std::vector<plugins::PluginRaid>>({}, [&] {
        PluginWorldEnv* target = found->env();
        return target == nullptr ? std::vector<plugins::PluginRaid>{} : target->raids();
    });
    for (const plugins::PluginRaid& raid : raids) {
        if (raid.id != id) {
            continue;
        }
        return jni::stringArrayOf(
            env,
            {raid.barTitle, std::to_string(raid.barColor), std::to_string(raid.barStyle), std::to_string(raid.barFlags),
             std::to_string(raid.barProgress), raid.barVisible ? "1" : "0",
             joinWith(
                 ',', [&](std::size_t at) { return std::to_string(raid.barPlayers[at]); }, raid.barPlayers.size())});
    }
    return nullptr;
}

jboolean JNICALL nativeRaidBarControl(JNIEnv* env, jclass, jstring world, jint id, jstring action, jstring text,
                                      jint value) {
    const auto found = envWorldOf(env, world);
    const std::string what = toStd(env, action);
    const std::string content = text == nullptr ? std::string{} : toStd(env, text);
    return found != nullptr && jni::onServer<bool>(false,
                                                   [&] {
                                                       PluginWorldEnv* target = found->env();
                                                       return target != nullptr &&
                                                              target->raidBar(id, what, content, value);
                                                   })
               ? JNI_TRUE
               : JNI_FALSE;
}

jbyteArray JNICALL nativeRaidValues(JNIEnv* env, jclass, jstring world, jint id) {
    const auto found = envWorldOf(env, world);
    if (found == nullptr) {
        return nullptr;
    }
    return jni::byteArrayOf(env, jni::onServer<std::optional<std::vector<std::uint8_t>>>(std::nullopt, [&] {
                                PluginWorldEnv* target = found->env();
                                return target == nullptr ? std::nullopt : target->raidValues(id);
                            }));
}

jboolean JNICALL nativeSetRaidValues(JNIEnv* env, jclass, jstring world, jint id, jbyteArray nbt) {
    const auto found = envWorldOf(env, world);
    const std::vector<std::uint8_t> bytes = jni::bytesOf(env, nbt);
    return found != nullptr && jni::onServer<bool>(false,
                                                   [&] {
                                                       PluginWorldEnv* target = found->env();
                                                       return target != nullptr && target->setRaidValues(id, bytes);
                                                   })
               ? JNI_TRUE
               : JNI_FALSE;
}

jboolean JNICALL nativeSetRaid(JNIEnv* env, jclass, jstring world, jint id, jint omen, jint groups) {
    const auto found = envWorldOf(env, world);
    return found != nullptr && jni::onServer<bool>(false,
                                                   [&] {
                                                       PluginWorldEnv* target = found->env();
                                                       return target != nullptr && target->setRaid(id, omen, groups);
                                                   })
               ? JNI_TRUE
               : JNI_FALSE;
}

jstring JNICALL nativeFluidAt(JNIEnv* env, jclass, jstring world, jint x, jint y, jint z) {
    const auto found = envWorldOf(env, world);
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
    const auto found = envWorldOf(env, world);
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
    const auto found = envWorldOf(env, world);
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
    const auto found = envWorldOf(env, world);
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
    const auto found = envWorldOf(env, world);
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
    const auto found = envWorldOf(env, world);
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
    const auto found = envWorldOf(env, world);
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
    const auto found = envWorldOf(env, world);
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
    const auto found = envWorldOf(env, world);
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
    const auto found = envWorldOf(env, world);
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
    const auto found = envWorldOf(env, world);
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
    const auto found = envWorldOf(env, world);
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
    const auto found = envWorldOf(env, world);
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
    const auto found = envWorldOf(env, world);
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
    const auto found = envWorldOf(env, world);
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

void registerWorldEnvMoreNatives(JNIEnv* env, jclass natives) {
    const auto method = [](const char* name, const char* signature, void* function) {
        return JNINativeMethod{const_cast<char*>(name), const_cast<char*>(signature), function};
    };
    const std::array methods{
        method("raids", "(Ljava/lang/String;)[Ljava/lang/String;", reinterpret_cast<void*>(&nativeRaids)),
        method("setRaid", "(Ljava/lang/String;III)Z", reinterpret_cast<void*>(&nativeSetRaid)),
        method("raidBar", "(Ljava/lang/String;I)[Ljava/lang/String;", reinterpret_cast<void*>(&nativeRaidBar)),
        method("raidBarControl", "(Ljava/lang/String;ILjava/lang/String;Ljava/lang/String;I)Z",
               reinterpret_cast<void*>(&nativeRaidBarControl)),
        method("raidValues", "(Ljava/lang/String;I)[B", reinterpret_cast<void*>(&nativeRaidValues)),
        method("setRaidValues", "(Ljava/lang/String;I[B)Z", reinterpret_cast<void*>(&nativeSetRaidValues)),
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
