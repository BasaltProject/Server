#include "BukkitWorldNativesShared.hpp"

namespace basalt::server::bukkit {
namespace {
using namespace shared;

jint JNICALL nativeState(JNIEnv* env, jclass, jstring name, jint x, jint y, jint z, jboolean load) {
    const auto world = worldOf(env, name);
    if (world == nullptr) {
        return -1;
    }
    return guarded<jint>(-1, [&] { return world->stateAt(x, y, z, load == JNI_TRUE); });
}

jboolean JNICALL nativeSetState(JNIEnv* env, jclass, jstring name, jint x, jint y, jint z, jint state,
                                jboolean notify) {
    const auto world = worldOf(env, name);
    if (world == nullptr) {
        return JNI_FALSE;
    }
    return guarded<jboolean>(
        JNI_FALSE, [&] { return world->setStateAt(x, y, z, state, notify == JNI_TRUE) ? JNI_TRUE : JNI_FALSE; });
}

jint JNICALL nativeReadStates(JNIEnv* env, jclass, jstring name, jintArray positions, jintArray out, jboolean load) {
    const auto world = worldOf(env, name);
    if (world == nullptr) {
        return 0;
    }
    const jsize count = env->GetArrayLength(out);
    std::vector<std::int32_t> at(static_cast<std::size_t>(env->GetArrayLength(positions)));
    env->GetIntArrayRegion(positions, 0, static_cast<jsize>(at.size()), at.data());
    std::vector<std::int32_t> read(static_cast<std::size_t>(count), -1);
    const jint found = guarded<jint>(0, [&] { return world->readStates(at, read, load == JNI_TRUE); });
    env->SetIntArrayRegion(out, 0, count, read.data());
    return found;
}

jint JNICALL nativeWriteStates(JNIEnv* env, jclass, jstring name, jintArray positions, jintArray states,
                               jboolean notify) {
    const auto world = worldOf(env, name);
    if (world == nullptr) {
        return 0;
    }
    std::vector<std::int32_t> at(static_cast<std::size_t>(env->GetArrayLength(positions)));
    env->GetIntArrayRegion(positions, 0, static_cast<jsize>(at.size()), at.data());
    std::vector<std::int32_t> next(static_cast<std::size_t>(env->GetArrayLength(states)));
    env->GetIntArrayRegion(states, 0, static_cast<jsize>(next.size()), next.data());
    return guarded<jint>(0, [&] { return world->writeStates(at, next, notify == JNI_TRUE); });
}

jboolean JNICALL nativeReadSection(JNIEnv* env, jclass, jstring name, jint chunkX, jint sectionY, jint chunkZ,
                                   jboolean load, jintArray out) {
    const auto world = worldOf(env, name);
    if (world == nullptr) {
        return JNI_FALSE;
    }
    std::vector<std::int32_t> read(4096);
    // A loaded section is copied under the world's read lock, so no thread hop is needed.
    if (world->readSection(chunkX, sectionY, chunkZ, false, read)) {
        env->SetIntArrayRegion(out, 0, 4096, read.data());
        return JNI_TRUE;
    }
    const bool ok =
        guarded<bool>(false, [&] { return world->readSection(chunkX, sectionY, chunkZ, load == JNI_TRUE, read); });
    if (ok) {
        env->SetIntArrayRegion(out, 0, 4096, read.data());
    }
    return ok ? JNI_TRUE : JNI_FALSE;
}

jboolean JNICALL nativeWriteSection(JNIEnv* env, jclass, jstring name, jint chunkX, jint sectionY, jint chunkZ,
                                    jintArray states, jboolean notify) {
    const auto world = worldOf(env, name);
    if (world == nullptr || env->GetArrayLength(states) < 4096) {
        return JNI_FALSE;
    }
    std::vector<std::int32_t> next(4096);
    env->GetIntArrayRegion(states, 0, 4096, next.data());
    return guarded<jboolean>(JNI_FALSE, [&] {
        return world->writeSection(chunkX, sectionY, chunkZ, next, notify == JNI_TRUE) ? JNI_TRUE : JNI_FALSE;
    });
}

jint JNICALL nativeApplySections(JNIEnv* env, jclass, jstring name, jint chunkX, jint chunkZ, jintArray sections,
                                 jintArray states, jboolean resend, jintArray previous) {
    const auto world = worldOf(env, name);
    if (world == nullptr) {
        return -1;
    }
    std::vector<std::int32_t> ys(static_cast<std::size_t>(env->GetArrayLength(sections)));
    env->GetIntArrayRegion(sections, 0, static_cast<jsize>(ys.size()), ys.data());
    std::vector<std::int32_t> next(static_cast<std::size_t>(env->GetArrayLength(states)));
    env->GetIntArrayRegion(states, 0, static_cast<jsize>(next.size()), next.data());
    std::vector<std::int32_t> before(previous == nullptr ? 0 : static_cast<std::size_t>(env->GetArrayLength(previous)));
    const jint changed =
        guarded<jint>(-1, [&] { return world->applySections(chunkX, chunkZ, ys, next, resend == JNI_TRUE, before); });
    if (previous != nullptr && changed >= 0) {
        env->SetIntArrayRegion(previous, 0, static_cast<jsize>(before.size()), before.data());
    }
    return changed;
}

void JNICALL nativeResendChunks(JNIEnv* env, jclass, jstring name, jintArray chunks) {
    const auto world = worldOf(env, name);
    if (world == nullptr) {
        return;
    }
    std::vector<std::int32_t> pairs(static_cast<std::size_t>(env->GetArrayLength(chunks)));
    env->GetIntArrayRegion(chunks, 0, static_cast<jsize>(pairs.size()), pairs.data());
    static_cast<void>(guarded<int>(0, [&] {
        world->resendChunks(pairs);
        return 0;
    }));
}

jstring JNICALL nativeBiome(JNIEnv* env, jclass, jstring name, jint x, jint y, jint z) {
    const auto world = worldOf(env, name);
    return world == nullptr ? nullptr : javaOrNull(env, guarded<std::optional<std::string>>(std::nullopt, [&] {
                                                       return world->biomeAt(x, y, z);
                                                   }));
}

jboolean JNICALL nativeSetBiome(JNIEnv* env, jclass, jstring name, jint x, jint y, jint z, jstring biome) {
    const auto world = worldOf(env, name);
    const std::string biomeName = world == nullptr ? std::string{} : toStd(env, biome);
    return world != nullptr && guarded<bool>(false, [&] { return world->setBiomeAt(x, y, z, biomeName); }) ? JNI_TRUE
                                                                                                           : JNI_FALSE;
}

jint JNICALL nativeHighest(JNIEnv* env, jclass, jstring name, jint x, jint z, jstring kind, jboolean load) {
    const auto world = worldOf(env, name);
    if (world == nullptr) {
        return 0;
    }
    const std::string heightmap = toStd(env, kind);
    return guarded<jint>(0, [&] { return world->highest(x, z, heightmap, load == JNI_TRUE); });
}

jboolean JNICALL nativeChunkLoaded(JNIEnv* env, jclass, jstring name, jint chunkX, jint chunkZ) {
    const auto world = worldOf(env, name);
    return world != nullptr && world->chunkLoaded(chunkX, chunkZ) ? JNI_TRUE : JNI_FALSE;
}

jboolean JNICALL nativeLoadChunk(JNIEnv* env, jclass, jstring name, jint chunkX, jint chunkZ, jboolean generate) {
    const auto world = worldOf(env, name);
    return world != nullptr &&
                   guarded<bool>(false, [&] { return world->loadChunk(chunkX, chunkZ, generate == JNI_TRUE); })
               ? JNI_TRUE
               : JNI_FALSE;
}

jboolean JNICALL nativeUnloadChunk(JNIEnv* env, jclass, jstring name, jint chunkX, jint chunkZ) {
    const auto world = worldOf(env, name);
    return world != nullptr && guarded<bool>(false, [&] { return world->unloadChunk(chunkX, chunkZ); }) ? JNI_TRUE
                                                                                                        : JNI_FALSE;
}

jintArray JNICALL nativeLoadedChunks(JNIEnv* env, jclass, jstring name) {
    const auto world = worldOf(env, name);
    return intArray(env, world == nullptr ? std::vector<std::int32_t>{} : world->loadedChunks());
}
} // namespace

void registerWorldNativesBlocks(JNIEnv* env, jclass natives) {
    const std::array methods{
        nativeMethod("state", "(Ljava/lang/String;IIIZ)I", reinterpret_cast<void*>(&nativeState)),
        nativeMethod("setState", "(Ljava/lang/String;IIIIZ)Z", reinterpret_cast<void*>(&nativeSetState)),
        nativeMethod("readStates", "(Ljava/lang/String;[I[IZ)I", reinterpret_cast<void*>(&nativeReadStates)),
        nativeMethod("writeStates", "(Ljava/lang/String;[I[IZ)I", reinterpret_cast<void*>(&nativeWriteStates)),
        nativeMethod("readSection", "(Ljava/lang/String;IIIZ[I)Z", reinterpret_cast<void*>(&nativeReadSection)),
        nativeMethod("writeSection", "(Ljava/lang/String;III[IZ)Z", reinterpret_cast<void*>(&nativeWriteSection)),
        nativeMethod("applySections", "(Ljava/lang/String;II[I[IZ[I)I", reinterpret_cast<void*>(&nativeApplySections)),
        nativeMethod("resendChunks", "(Ljava/lang/String;[I)V", reinterpret_cast<void*>(&nativeResendChunks)),
        nativeMethod("biome", "(Ljava/lang/String;III)Ljava/lang/String;", reinterpret_cast<void*>(&nativeBiome)),
        nativeMethod("setBiome", "(Ljava/lang/String;IIILjava/lang/String;)Z",
                     reinterpret_cast<void*>(&nativeSetBiome)),
        nativeMethod("highest", "(Ljava/lang/String;IILjava/lang/String;Z)I", reinterpret_cast<void*>(&nativeHighest)),
        nativeMethod("chunkLoaded", "(Ljava/lang/String;II)Z", reinterpret_cast<void*>(&nativeChunkLoaded)),
        nativeMethod("loadChunk", "(Ljava/lang/String;IIZ)Z", reinterpret_cast<void*>(&nativeLoadChunk)),
        nativeMethod("unloadChunk", "(Ljava/lang/String;II)Z", reinterpret_cast<void*>(&nativeUnloadChunk)),
        nativeMethod("loadedChunks", "(Ljava/lang/String;)[I", reinterpret_cast<void*>(&nativeLoadedChunks)),
    };
    registerMethods(env, natives, methods.data(), static_cast<jint>(methods.size()));
}
} // namespace basalt::server::bukkit
