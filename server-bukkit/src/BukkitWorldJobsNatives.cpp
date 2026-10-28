#include "BukkitWorldNativesShared.hpp"

namespace basalt::server::bukkit {
namespace {
using namespace shared;

// Holds a Java object across threads and lets go of it wherever it is last used.
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

void JNICALL nativeRequestChunk(JNIEnv* env, jclass, jstring name, jint chunkX, jint chunkZ, jboolean generate,
                                jint priority, jobject future) {
    const auto world = worldOf(env, name);
    const auto held = std::make_shared<JavaRef>(env, future);
    const auto finish = [held](bool loaded) {
        JNIEnv* callEnv = BukkitBridge::liveEnv();
        if (callEnv == nullptr) {
            return;
        }
        JniLocalFrame frame{callEnv};
        if (!frame)
            return;
        jclass booleanClass = callEnv->FindClass("java/lang/Boolean");
        jmethodID valueOf = callEnv->GetStaticMethodID(booleanClass, "valueOf", "(Z)Ljava/lang/Boolean;");
        jobject value = callEnv->CallStaticObjectMethod(booleanClass, valueOf, loaded ? JNI_TRUE : JNI_FALSE);
        jclass futureClass = callEnv->FindClass("java/util/concurrent/CompletableFuture");
        jmethodID complete = callEnv->GetMethodID(futureClass, "complete", "(Ljava/lang/Object;)Z");
        callEnv->CallBooleanMethod(held->ref, complete, value);
        callEnv->ExceptionClear();
        callEnv->DeleteLocalRef(value);
        callEnv->DeleteLocalRef(booleanClass);
        callEnv->DeleteLocalRef(futureClass);
    };
    if (world == nullptr) {
        finish(false);
        return;
    }
    BukkitBridge& bridge = BukkitBridge::current();
    const auto start = [world, chunkX, chunkZ, generate, priority, finish] {
        world->requestChunk(chunkX, chunkZ, generate == JNI_TRUE, priority, finish);
    };
    if (bridge.plugins().onPluginThread()) {
        start();
    } else {
        static_cast<void>(bridge.services().tasks().after(0, start));
    }
}

jboolean JNICALL nativeHoldChunks(JNIEnv* env, jclass, jstring name, jstring token, jintArray chunks) {
    const auto world = worldOf(env, name);
    if (world == nullptr) {
        return JNI_FALSE;
    }
    const std::string holder = toStd(env, token);
    const std::vector<std::int32_t> pairs = ints(env, chunks);
    return guarded<bool>(false, [&] { return world->holdChunks(holder, pairs); }) ? JNI_TRUE : JNI_FALSE;
}

void JNICALL nativeReleaseChunks(JNIEnv* env, jclass, jstring name, jstring token) {
    if (const auto world = worldOf(env, name); world != nullptr) {
        const std::string holder = toStd(env, token);
        static_cast<void>(guarded<int>(0, [&] {
            world->releaseChunks(holder);
            return 0;
        }));
    }
}

jint JNICALL nativeUpdateNeighbors(JNIEnv* env, jclass, jstring name, jintArray positions, jint effects) {
    const auto world = worldOf(env, name);
    if (world == nullptr) {
        return 0;
    }
    const std::vector<std::int32_t> at = ints(env, positions);
    return guarded<jint>(0, [&] { return world->updateNeighbors(at, static_cast<std::uint32_t>(effects)); });
}

void JNICALL nativeLightJob(JNIEnv* env, jclass, jstring name, jintArray chunks, jobject future) {
    const auto world = worldOf(env, name);
    const auto held = std::make_shared<JavaRef>(env, future);
    const auto finish = [held](std::int32_t relit) {
        JNIEnv* callEnv = BukkitBridge::liveEnv();
        if (callEnv == nullptr) {
            return;
        }
        JniLocalFrame frame{callEnv};
        if (!frame)
            return;
        jclass integerClass = callEnv->FindClass("java/lang/Integer");
        jmethodID valueOf = callEnv->GetStaticMethodID(integerClass, "valueOf", "(I)Ljava/lang/Integer;");
        jobject value = callEnv->CallStaticObjectMethod(integerClass, valueOf, relit);
        jclass futureClass = callEnv->FindClass("java/util/concurrent/CompletableFuture");
        jmethodID complete = callEnv->GetMethodID(futureClass, "complete", "(Ljava/lang/Object;)Z");
        callEnv->CallBooleanMethod(held->ref, complete, value);
        callEnv->ExceptionClear();
        callEnv->DeleteLocalRef(value);
        callEnv->DeleteLocalRef(integerClass);
        callEnv->DeleteLocalRef(futureClass);
    };
    if (world == nullptr) {
        finish(0);
        return;
    }
    const std::vector<std::int32_t> pairs = ints(env, chunks);
    world->lightJob(pairs, finish);
}

jint JNICALL nativeLightAt(JNIEnv* env, jclass, jstring name, jint x, jint y, jint z, jint layer) {
    const auto world = worldOf(env, name);
    return world == nullptr ? -1 : world->lightAt(x, y, z, layer);
}

jbyteArray JNICALL nativeReadLight(JNIEnv* env, jclass, jstring name, jint chunkX, jint sectionY, jint chunkZ,
                                   jint layer, jboolean load) {
    const auto world = worldOf(env, name);
    if (world == nullptr) {
        return nullptr;
    }
    std::vector<std::uint8_t> read(2048);
    bool ok = world->readLight(chunkX, sectionY, chunkZ, layer, false, read);
    if (!ok && load == JNI_TRUE) {
        ok = guarded<bool>(false, [&] { return world->readLight(chunkX, sectionY, chunkZ, layer, true, read); });
    }
    if (!ok) {
        return nullptr;
    }
    jbyteArray out = env->NewByteArray(static_cast<jsize>(read.size()));
    env->SetByteArrayRegion(out, 0, static_cast<jsize>(read.size()), reinterpret_cast<const jbyte*>(read.data()));
    return out;
}

jboolean JNICALL nativeWriteLight(JNIEnv* env, jclass, jstring name, jint chunkX, jint sectionY, jint chunkZ,
                                  jint layer, jbyteArray nibbles) {
    const auto world = worldOf(env, name);
    if (world == nullptr) {
        return JNI_FALSE;
    }
    const std::vector<std::uint8_t> data = bytes(env, nibbles);
    return world->writeLight(chunkX, sectionY, chunkZ, layer, data) ? JNI_TRUE : JNI_FALSE;
}

void JNICALL nativeCheckLight(JNIEnv* env, jclass, jstring name, jintArray positions) {
    const auto world = worldOf(env, name);
    if (world == nullptr) {
        return;
    }
    world->checkLight(ints(env, positions));
}

jint JNICALL nativeUseItemOn(JNIEnv* env, jclass, jstring name, jint x, jint y, jint z, jint face, jstring item,
                             jfloat yaw, jfloat pitch) {
    const auto world = worldOf(env, name);
    if (world == nullptr) {
        return 0;
    }
    const std::string held = toStd(env, item);
    return guarded<jint>(0, [&] { return world->useItemOn(x, y, z, face, held, yaw, pitch); });
}

jboolean JNICALL nativeScheduleTick(JNIEnv* env, jclass, jstring name, jint x, jint y, jint z, jint delay,
                                    jboolean fluid) {
    const auto world = worldOf(env, name);
    if (world == nullptr) {
        return JNI_FALSE;
    }
    return guarded<bool>(false, [&] { return world->scheduleTick(x, y, z, delay, fluid != JNI_FALSE); }) ? JNI_TRUE
                                                                                                         : JNI_FALSE;
}

jint JNICALL nativeValidState(JNIEnv* env, jclass, jstring name, jint x, jint y, jint z, jint state) {
    const auto world = worldOf(env, name);
    if (world == nullptr) {
        return state;
    }
    return guarded<jint>(state, [&] { return world->validState(x, y, z, state); });
}

jint JNICALL nativeSettleBlocks(JNIEnv* env, jclass, jstring name, jintArray positions, jintArray previous,
                                jint effects) {
    const auto world = worldOf(env, name);
    if (world == nullptr) {
        return 0;
    }
    const std::vector<std::int32_t> at = ints(env, positions);
    const std::vector<std::int32_t> was = ints(env, previous);
    return guarded<jint>(0, [&] { return world->settleBlocks(at, was, static_cast<std::uint32_t>(effects)); });
}

jint JNICALL nativeNeighborChanged(JNIEnv* env, jclass, jstring name, jintArray positions, jintArray sources) {
    const auto world = worldOf(env, name);
    if (world == nullptr) {
        return 0;
    }
    const std::vector<std::int32_t> at = ints(env, positions);
    const std::vector<std::int32_t> told = ints(env, sources);
    return guarded<jint>(0, [&] { return world->neighborChanged(at, told); });
}

jint JNICALL nativeUpdateShapes(JNIEnv* env, jclass, jstring name, jintArray positions, jintArray states,
                                jboolean indirect) {
    const auto world = worldOf(env, name);
    if (world == nullptr) {
        return 0;
    }
    const std::vector<std::int32_t> at = ints(env, positions);
    const std::vector<std::int32_t> next = ints(env, states);
    return guarded<jint>(0, [&] { return world->updateShapes(at, next, indirect != JNI_FALSE); });
}

jint JNICALL nativeApplyChanges(JNIEnv* env, jclass, jstring name, jintArray positions, jintArray states,
                                jint effects) {
    const auto world = worldOf(env, name);
    if (world == nullptr) {
        return 0;
    }
    const std::vector<std::int32_t> at = ints(env, positions);
    const std::vector<std::int32_t> next = ints(env, states);
    return guarded<jint>(0, [&] { return world->applyChanges(at, next, static_cast<std::uint32_t>(effects)); });
}
} // namespace

void registerWorldNativesJobs(JNIEnv* env, jclass natives) {
    const std::array methods{
        nativeMethod("requestChunk", "(Ljava/lang/String;IIZILjava/util/concurrent/CompletableFuture;)V",
                     reinterpret_cast<void*>(&nativeRequestChunk)),
        nativeMethod("holdChunks", "(Ljava/lang/String;Ljava/lang/String;[I)Z",
                     reinterpret_cast<void*>(&nativeHoldChunks)),
        nativeMethod("releaseChunks", "(Ljava/lang/String;Ljava/lang/String;)V",
                     reinterpret_cast<void*>(&nativeReleaseChunks)),
        nativeMethod("updateNeighbors", "(Ljava/lang/String;[II)I", reinterpret_cast<void*>(&nativeUpdateNeighbors)),
        nativeMethod("applyChanges", "(Ljava/lang/String;[I[II)I", reinterpret_cast<void*>(&nativeApplyChanges)),
        nativeMethod("lightJob", "(Ljava/lang/String;[ILjava/util/concurrent/CompletableFuture;)V",
                     reinterpret_cast<void*>(&nativeLightJob)),
        nativeMethod("lightAt", "(Ljava/lang/String;IIII)I", reinterpret_cast<void*>(&nativeLightAt)),
        nativeMethod("readLight", "(Ljava/lang/String;IIIIZ)[B", reinterpret_cast<void*>(&nativeReadLight)),
        nativeMethod("writeLight", "(Ljava/lang/String;IIII[B)Z", reinterpret_cast<void*>(&nativeWriteLight)),
        nativeMethod("checkLight", "(Ljava/lang/String;[I)V", reinterpret_cast<void*>(&nativeCheckLight)),
        nativeMethod("useItemOn", "(Ljava/lang/String;IIIILjava/lang/String;FF)I",
                     reinterpret_cast<void*>(&nativeUseItemOn)),
        nativeMethod("scheduleTick", "(Ljava/lang/String;IIIIZ)Z", reinterpret_cast<void*>(&nativeScheduleTick)),
        nativeMethod("validState", "(Ljava/lang/String;IIII)I", reinterpret_cast<void*>(&nativeValidState)),
        nativeMethod("settleBlocks", "(Ljava/lang/String;[I[II)I", reinterpret_cast<void*>(&nativeSettleBlocks)),
        nativeMethod("neighborChanged", "(Ljava/lang/String;[I[I)I", reinterpret_cast<void*>(&nativeNeighborChanged)),
        nativeMethod("updateShapes", "(Ljava/lang/String;[I[IZ)I", reinterpret_cast<void*>(&nativeUpdateShapes)),
    };
    registerMethods(env, natives, methods.data(), static_cast<jint>(methods.size()));
}
} // namespace basalt::server::bukkit
