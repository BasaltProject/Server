#include "BukkitWorldNativesShared.hpp"

namespace basalt::server::bukkit {
namespace {
using namespace shared;

jint JNICALL nativeStateId(JNIEnv* env, jclass, jstring text) { return worlds().stateId(toStd(env, text)); }

jstring JNICALL nativeStateText(JNIEnv* env, jclass, jint id) { return toJava(env, worlds().stateText(id)); }

jobjectArray JNICALL nativeBlockTable(JNIEnv* env, jclass) { return stringArray(env, worlds().blockTable()); }

jobjectArray JNICALL nativeBlockProperties(JNIEnv* env, jclass, jstring block) {
    return stringArray(env, worlds().blockProperties(toStd(env, block)));
}

jint JNICALL nativeStateFlags(JNIEnv*, jclass, jint id) { return worlds().stateFlags(id); }

jdoubleArray JNICALL nativeBlockHardness(JNIEnv* env, jclass, jstring block) {
    const std::vector<double> facts = worlds().blockHardness(toStd(env, block));
    return facts.empty() ? nullptr : doubleArray(env, facts.data(), facts.size());
}

jdoubleArray JNICALL nativeBlockInfo(JNIEnv* env, jclass, jint id) {
    const std::vector<double> facts = worlds().blockInfo(id);
    return facts.empty() ? nullptr : doubleArray(env, facts.data(), facts.size());
}

jobjectArray JNICALL nativeBlockSounds(JNIEnv* env, jclass, jint id) {
    const std::vector<std::string> sounds = worlds().blockSounds(id);
    return sounds.empty() ? nullptr : stringArray(env, sounds);
}

jobjectArray JNICALL nativeBiomeIds(JNIEnv* env, jclass) { return stringArray(env, worlds().biomeIds()); }

jobjectArray JNICALL nativeItemIds(JNIEnv* env, jclass) { return stringArray(env, worlds().itemIds()); }

jobjectArray JNICALL nativeTags(JNIEnv* env, jclass, jstring registry) {
    return stringArray(env, worlds().tags(toStd(env, registry)));
}

jobjectArray JNICALL nativeRegistryIds(JNIEnv* env, jclass, jstring registry) {
    return stringArray(env, worlds().registryIds(toStd(env, registry)));
}

jobjectArray JNICALL nativeEntityTypes(JNIEnv* env, jclass) { return stringArray(env, worlds().entityTypeIds()); }

jobjectArray JNICALL nativeWorldStrings(JNIEnv* env, jclass, jstring name) {
    const auto world = worldOf(env, name);
    if (world == nullptr) {
        return nullptr;
    }
    return stringArray(env, {world->dimension(), world->folder(), world->difficulty()});
}

jlongArray JNICALL nativeWorldNumbers(JNIEnv* env, jclass, jstring name) {
    const auto world = worldOf(env, name);
    if (world == nullptr) {
        return nullptr;
    }
    const std::array<std::int32_t, 3> spawn = world->spawn();
    const std::array<jlong, 11> values{world->seed(),
                                       world->minHeight(),
                                       world->height(),
                                       spawn[0],
                                       spawn[1],
                                       spawn[2],
                                       world->dayTime(),
                                       world->raining() ? 1 : 0,
                                       world->thundering() ? 1 : 0,
                                       world->time(),
                                       0};
    jlongArray array = env->NewLongArray(static_cast<jsize>(values.size()));
    env->SetLongArrayRegion(array, 0, static_cast<jsize>(values.size()), values.data());
    return array;
}

void JNICALL nativeSetTime(JNIEnv* env, jclass, jstring name, jlong ticks) {
    if (const auto world = worldOf(env, name); world != nullptr) {
        world->setDayTime(ticks);
    }
}

void JNICALL nativeSetWeather(JNIEnv* env, jclass, jstring name, jboolean raining, jboolean thundering) {
    if (const auto world = worldOf(env, name); world != nullptr) {
        world->setWeather(raining == JNI_TRUE, thundering == JNI_TRUE);
    }
}

void JNICALL nativeSetSpawn(JNIEnv* env, jclass, jstring name, jint x, jint y, jint z) {
    if (const auto world = worldOf(env, name); world != nullptr) {
        world->setSpawn(x, y, z);
    }
}

jboolean JNICALL nativeSetDifficulty(JNIEnv* env, jclass, jstring name, jstring difficulty) {
    const auto world = worldOf(env, name);
    return world != nullptr && world->setDifficulty(toStd(env, difficulty)) ? JNI_TRUE : JNI_FALSE;
}

jstring JNICALL nativeGameRule(JNIEnv* env, jclass, jstring name, jstring rule) {
    const auto world = worldOf(env, name);
    return world == nullptr ? nullptr : javaOrNull(env, world->gameRule(toStd(env, rule)));
}

jboolean JNICALL nativeSetGameRule(JNIEnv* env, jclass, jstring name, jstring rule, jstring value) {
    const auto world = worldOf(env, name);
    return world != nullptr && world->setGameRule(toStd(env, rule), toStd(env, value)) ? JNI_TRUE : JNI_FALSE;
}

jobjectArray JNICALL nativeGameRules(JNIEnv* env, jclass, jstring name) {
    const auto world = worldOf(env, name);
    return stringArray(env, world == nullptr ? std::vector<std::string>{} : world->gameRules());
}

void JNICALL nativeSaveWorld(JNIEnv* env, jclass, jstring name) {
    if (const auto world = worldOf(env, name); world != nullptr) {
        static_cast<void>(guarded<int>(0, [&] {
            world->save();
            return 0;
        }));
    }
}
} // namespace

void registerWorldNativesRegistry(JNIEnv* env, jclass natives) {
    const std::array methods{
        nativeMethod("stateId", "(Ljava/lang/String;)I", reinterpret_cast<void*>(&nativeStateId)),
        nativeMethod("stateText", "(I)Ljava/lang/String;", reinterpret_cast<void*>(&nativeStateText)),
        nativeMethod("blockTable", "()[Ljava/lang/String;", reinterpret_cast<void*>(&nativeBlockTable)),
        nativeMethod("blockProperties", "(Ljava/lang/String;)[Ljava/lang/String;",
                     reinterpret_cast<void*>(&nativeBlockProperties)),
        nativeMethod("stateFlags", "(I)I", reinterpret_cast<void*>(&nativeStateFlags)),
        nativeMethod("blockHardness", "(Ljava/lang/String;)[D", reinterpret_cast<void*>(&nativeBlockHardness)),
        nativeMethod("blockInfo", "(I)[D", reinterpret_cast<void*>(&nativeBlockInfo)),
        nativeMethod("blockSounds", "(I)[Ljava/lang/String;", reinterpret_cast<void*>(&nativeBlockSounds)),
        nativeMethod("biomeIds", "()[Ljava/lang/String;", reinterpret_cast<void*>(&nativeBiomeIds)),
        nativeMethod("itemIds", "()[Ljava/lang/String;", reinterpret_cast<void*>(&nativeItemIds)),
        nativeMethod("registryIds", "(Ljava/lang/String;)[Ljava/lang/String;",
                     reinterpret_cast<void*>(&nativeRegistryIds)),
        nativeMethod("tags", "(Ljava/lang/String;)[Ljava/lang/String;", reinterpret_cast<void*>(&nativeTags)),
        nativeMethod("entityTypes", "()[Ljava/lang/String;", reinterpret_cast<void*>(&nativeEntityTypes)),
        nativeMethod("worldStrings", "(Ljava/lang/String;)[Ljava/lang/String;",
                     reinterpret_cast<void*>(&nativeWorldStrings)),
        nativeMethod("worldNumbers", "(Ljava/lang/String;)[J", reinterpret_cast<void*>(&nativeWorldNumbers)),
        nativeMethod("setTime", "(Ljava/lang/String;J)V", reinterpret_cast<void*>(&nativeSetTime)),
        nativeMethod("setWeather", "(Ljava/lang/String;ZZ)V", reinterpret_cast<void*>(&nativeSetWeather)),
        nativeMethod("setSpawn", "(Ljava/lang/String;III)V", reinterpret_cast<void*>(&nativeSetSpawn)),
        nativeMethod("setDifficulty", "(Ljava/lang/String;Ljava/lang/String;)Z",
                     reinterpret_cast<void*>(&nativeSetDifficulty)),
        nativeMethod("gameRule", "(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;",
                     reinterpret_cast<void*>(&nativeGameRule)),
        nativeMethod("setGameRule", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Z",
                     reinterpret_cast<void*>(&nativeSetGameRule)),
        nativeMethod("gameRules", "(Ljava/lang/String;)[Ljava/lang/String;", reinterpret_cast<void*>(&nativeGameRules)),
        nativeMethod("saveWorld", "(Ljava/lang/String;)V", reinterpret_cast<void*>(&nativeSaveWorld)),
    };
    registerMethods(env, natives, methods.data(), static_cast<jint>(methods.size()));
}
} // namespace basalt::server::bukkit
