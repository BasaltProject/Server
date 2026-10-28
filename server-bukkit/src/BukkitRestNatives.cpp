#include "JniSupport.hpp"

#include <basalt/server/plugins/PluginWorldEnv.hpp>

#include <cmath>
#include <limits>

namespace basalt::server::bukkit {
namespace {
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

[[nodiscard]] jboolean flag(bool value) { return value ? JNI_TRUE : JNI_FALSE; }

[[nodiscard]] plugins::PluginServerControl& control() { return BukkitBridge::current().services().serverControl(); }

template <typename Result, typename Body>
[[nodiscard]] Result withEnv(const std::string& world, Result fallback, Body&& body) {
    return jni::onServer<Result>(fallback, [&]() -> Result {
        const auto found = BukkitBridge::current().services().worlds().byName(world);
        plugins::PluginWorldEnv* env = found == nullptr ? nullptr : found->env();
        return env == nullptr ? fallback : body(*env);
    });
}

jlongArray JNICALL nativeTickTimes(JNIEnv* env, jclass) {
    const auto times = jni::onServer<std::vector<std::int64_t>>({}, [] { return control().tickTimes(); });
    jlongArray array = env->NewLongArray(static_cast<jsize>(times.size()));
    std::vector<jlong> copy(times.begin(), times.end());
    env->SetLongArrayRegion(array, 0, static_cast<jsize>(copy.size()), copy.data());
    return array;
}

jboolean JNICALL nativePaused(JNIEnv*, jclass) {
    return flag(jni::onServer<bool>(false, [] { return control().paused(); }));
}

jdouble JNICALL nativeWorldTunable(JNIEnv* env, jclass, jstring world, jstring key) {
    const std::string name = toStd(env, world);
    const std::string what = toStd(env, key);
    return withEnv<double>(name, std::numeric_limits<double>::quiet_NaN(), [&](plugins::PluginWorldEnv& target) {
        return target.tunable(what).value_or(std::numeric_limits<double>::quiet_NaN());
    });
}

jboolean JNICALL nativeSetWorldTunable(JNIEnv* env, jclass, jstring world, jstring key, jdouble value) {
    const std::string name = toStd(env, world);
    const std::string what = toStd(env, key);
    return flag(
        withEnv<bool>(name, false, [&](plugins::PluginWorldEnv& target) { return target.setTunable(what, value); }));
}

jboolean JNICALL nativeRandomTick(JNIEnv* env, jclass, jstring world, jint x, jint y, jint z) {
    const std::string name = toStd(env, world);
    return flag(
        withEnv<bool>(name, false, [&](plugins::PluginWorldEnv& target) { return target.randomTick(x, y, z); }));
}

jint JNICALL nativeTickableBlockEntities(JNIEnv* env, jclass, jstring world) {
    const std::string name = toStd(env, world);
    return withEnv<jint>(name, 0, [](plugins::PluginWorldEnv& target) { return target.tickableBlockEntityCount(); });
}

jintArray JNICALL nativePlaceTree(JNIEnv* env, jclass, jstring world, jstring tree, jint x, jint y, jint z,
                                  jboolean apply) {
    const std::string name = toStd(env, world);
    const std::string kind = toStd(env, tree);
    const auto placed = withEnv<std::optional<std::vector<plugins::PluginPlacedBlock>>>(
        name, std::nullopt,
        [&](plugins::PluginWorldEnv& target) { return target.placeTree(kind, x, y, z, apply == JNI_TRUE); });
    if (!placed.has_value()) {
        return nullptr;
    }
    std::vector<jint> flat;
    flat.reserve(placed->size() * 5);
    for (const plugins::PluginPlacedBlock& block : *placed) {
        flat.insert(flat.end(), {block.x, block.y, block.z, block.before, block.after});
    }
    jintArray array = env->NewIntArray(static_cast<jsize>(flat.size()));
    env->SetIntArrayRegion(array, 0, static_cast<jsize>(flat.size()), flat.data());
    return array;
}

jstring JNICALL nativeBlockInstrument(JNIEnv* env, jclass, jint state) {
    const auto name = jni::onServer<std::string>(
        {}, [&] { return BukkitBridge::current().services().worlds().blockInstrument(state); });
    return name.empty() ? nullptr : toJava(env, name);
}

jboolean JNICALL nativePlaceEndGateway(JNIEnv* env, jclass, jstring world, jint x, jint y, jint z) {
    const std::string name = toStd(env, world);
    return flag(jni::onServer<bool>(false, [&] {
        const auto found = BukkitBridge::current().services().worlds().byName(name);
        return found != nullptr && found->placeEndGateway(x, y, z);
    }));
}

jint JNICALL nativeNextEntityId(JNIEnv*, jclass) {
    return jni::onServer<jint>(-1, [] { return BukkitBridge::current().services().worlds().nextEntityId(); });
}

jboolean JNICALL nativeReloadData(JNIEnv*, jclass) {
    return flag(jni::onServer<bool>(false, [] { return BukkitBridge::current().services().datapacks().reload(); }));
}

jlong JNICALL nativeOfflineStatistic(JNIEnv* env, jclass, jstring uuid, jstring key) {
    const std::string who = toStd(env, uuid);
    const std::string what = toStd(env, key);
    return jni::onServer<jlong>(0, [&] { return BukkitBridge::current().services().access().statistic(who, what); });
}

jboolean JNICALL nativeSetOfflineStatistic(JNIEnv* env, jclass, jstring uuid, jstring key, jlong value) {
    const std::string who = toStd(env, uuid);
    const std::string what = toStd(env, key);
    return flag(jni::onServer<bool>(
        false, [&] { return BukkitBridge::current().services().access().setStatistic(who, what, value); }));
}

jobjectArray JNICALL nativeLastDeath(JNIEnv* env, jclass, jstring uuid) {
    const std::string who = toStd(env, uuid);
    const auto spot = jni::onServer<std::optional<std::array<std::string, 4>>>(
        std::nullopt, [&] { return BukkitBridge::current().services().access().lastDeath(who); });
    return spot.has_value() ? jni::stringArrayOf(env, {(*spot)[0], (*spot)[1], (*spot)[2], (*spot)[3]}) : nullptr;
}

jstring JNICALL nativePoiOfState(JNIEnv* env, jclass, jint state) {
    const auto name =
        jni::onServer<std::string>({}, [&] { return BukkitBridge::current().services().worlds().poiTypeOf(state); });
    return name.empty() ? nullptr : toJava(env, name);
}

jint JNICALL nativePoiTickets(JNIEnv* env, jclass, jstring type) {
    const std::string name = toStd(env, type);
    return jni::onServer<jint>(0, [&] { return BukkitBridge::current().services().worlds().poiTickets(name); });
}

jboolean JNICALL nativeUseGeneratorWith(JNIEnv* env, jclass, jstring world, jint flags) {
    const std::string name = toStd(env, world);
    const auto found = BukkitBridge::current().services().worlds().byName(name);
    jclass local = env->FindClass("org/basaltmc/bukkit/worldgen/CustomGenerators");
    if (found == nullptr || local == nullptr) {
        env->ExceptionClear();
        return JNI_FALSE;
    }
    const auto holder = std::make_shared<JavaRef>(env, local);
    const jmethodID fill = env->GetStaticMethodID(local, "fillOver", "(Ljava/lang/String;II[I)[I");
    if (fill == nullptr) {
        env->ExceptionClear();
        return JNI_FALSE;
    }
    const plugins::PluginWorldEnv::ChunkFillTraits traits{.vanillaTerrain = (flags & 1) != 0,
                                                          .decorations = (flags & 2) != 0,
                                                          .structures = (flags & 4) != 0,
                                                          .mobs = (flags & 8) != 0,
                                                          .noise = (flags & 16) != 0,
                                                          .surface = (flags & 32) != 0,
                                                          .caves = (flags & 64) != 0};
    return flag(jni::onServer<bool>(false, [&] {
        plugins::PluginWorldEnv* target = found->env();
        return target != nullptr &&
               target->useChunkFill(
                   [holder, fill, name](std::int32_t chunkX, std::int32_t chunkZ, std::vector<std::int32_t>& blocks,
                                        std::vector<std::int32_t>& biomes) {
                       JNIEnv* callEnv = BukkitBridge::liveEnv();
                       if (callEnv == nullptr) {
                           return;
                       }
                       jintArray base = nullptr;
                       if (!blocks.empty()) {
                           std::vector<jint> given;
                           given.reserve(2 + blocks.size() + biomes.size());
                           given.push_back(static_cast<jint>(blocks.size()));
                           given.push_back(static_cast<jint>(biomes.size()));
                           given.insert(given.end(), blocks.begin(), blocks.end());
                           given.insert(given.end(), biomes.begin(), biomes.end());
                           base = callEnv->NewIntArray(static_cast<jsize>(given.size()));
                           callEnv->SetIntArrayRegion(base, 0, static_cast<jsize>(given.size()), given.data());
                       }
                       jstring who = toJava(callEnv, name);
                       auto* packed = static_cast<jintArray>(callEnv->CallStaticObjectMethod(
                           static_cast<jclass>(holder->ref), fill, who, chunkX, chunkZ, base));
                       callEnv->DeleteLocalRef(who);
                       if (base != nullptr) {
                           callEnv->DeleteLocalRef(base);
                       }
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
                       if (all[0] < 0 || all[1] < 0) {
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
                   },
                   traits);
    }));
}

jbyteArray JNICALL nativeServerIcon(JNIEnv* env, jclass) {
    using Blob = std::optional<std::vector<std::uint8_t>>;
    return jni::byteArrayOf(env, jni::onServer<Blob>(std::nullopt, [] { return Blob{control().serverIcon()}; }));
}

jboolean JNICALL nativeReloadCommands(JNIEnv*, jclass) {
    return flag(jni::onServer<bool>(false, [] { return control().reloadCommands(); }));
}
} // namespace

void registerRestNatives(JNIEnv* env, jclass natives) {
    const auto method = [](const char* name, const char* signature, void* function) {
        return JNINativeMethod{const_cast<char*>(name), const_cast<char*>(signature), function};
    };
    const std::array methods{
        method("tickTimes", "()[J", reinterpret_cast<void*>(&nativeTickTimes)),
        method("paused", "()Z", reinterpret_cast<void*>(&nativePaused)),
        method("worldTunable", "(Ljava/lang/String;Ljava/lang/String;)D", reinterpret_cast<void*>(&nativeWorldTunable)),
        method("setWorldTunable", "(Ljava/lang/String;Ljava/lang/String;D)Z",
               reinterpret_cast<void*>(&nativeSetWorldTunable)),
        method("randomTick", "(Ljava/lang/String;III)Z", reinterpret_cast<void*>(&nativeRandomTick)),
        method("tickableBlockEntities", "(Ljava/lang/String;)I", reinterpret_cast<void*>(&nativeTickableBlockEntities)),
        method("placeTree", "(Ljava/lang/String;Ljava/lang/String;IIIZ)[I", reinterpret_cast<void*>(&nativePlaceTree)),
        method("blockInstrument", "(I)Ljava/lang/String;", reinterpret_cast<void*>(&nativeBlockInstrument)),
        method("placeEndGateway", "(Ljava/lang/String;III)Z", reinterpret_cast<void*>(&nativePlaceEndGateway)),
        method("nextEntityId", "()I", reinterpret_cast<void*>(&nativeNextEntityId)),
        method("reloadData", "()Z", reinterpret_cast<void*>(&nativeReloadData)),
        method("offlineStatistic", "(Ljava/lang/String;Ljava/lang/String;)J",
               reinterpret_cast<void*>(&nativeOfflineStatistic)),
        method("setOfflineStatistic", "(Ljava/lang/String;Ljava/lang/String;J)Z",
               reinterpret_cast<void*>(&nativeSetOfflineStatistic)),
        method("lastDeath", "(Ljava/lang/String;)[Ljava/lang/String;", reinterpret_cast<void*>(&nativeLastDeath)),
        method("poiOfState", "(I)Ljava/lang/String;", reinterpret_cast<void*>(&nativePoiOfState)),
        method("poiTickets", "(Ljava/lang/String;)I", reinterpret_cast<void*>(&nativePoiTickets)),
        method("useGeneratorWith", "(Ljava/lang/String;I)Z", reinterpret_cast<void*>(&nativeUseGeneratorWith)),
        method("serverIcon", "()[B", reinterpret_cast<void*>(&nativeServerIcon)),
        method("reloadCommands", "()Z", reinterpret_cast<void*>(&nativeReloadCommands)),
    };
    if (env->RegisterNatives(natives, methods.data(), static_cast<jint>(methods.size())) != 0) {
        BukkitBridge::current().log().error(
            [] { return std::string{"RestNatives has a native the runtime does not declare"}; });
        env->ExceptionDescribe();
        env->ExceptionClear();
    }
}
} // namespace basalt::server::bukkit
