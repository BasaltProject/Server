#include "BukkitWorldNativesShared.hpp"

namespace basalt::server::bukkit {
namespace {
using namespace shared;

// Worlds handed out by number so the hot version check needs no name lookup; they live as long as the process.
constexpr std::size_t kMaxHandles = 256;
std::array<std::shared_ptr<PluginWorldRef>, kMaxHandles> gHandleOwners;
std::array<std::atomic<PluginWorldRef*>, kMaxHandles> gHandles{};
std::atomic<std::int32_t> gHandleCount{0};
std::mutex gHandleMutex;

jint JNICALL nativeWorldHandle(JNIEnv* env, jclass, jstring name) {
    const std::string wanted = toStd(env, name);
    const std::lock_guard<std::mutex> lock{gHandleMutex};
    const std::int32_t count = gHandleCount.load();
    for (std::int32_t at = 0; at < count; ++at) {
        if (gHandles[static_cast<std::size_t>(at)].load()->name() == wanted) {
            return at;
        }
    }
    const auto world = worlds().byName(wanted);
    if (world == nullptr || count >= static_cast<std::int32_t>(kMaxHandles)) {
        return -1;
    }
    gHandleOwners[static_cast<std::size_t>(count)] = world;
    gHandles[static_cast<std::size_t>(count)].store(world.get());
    gHandleCount.store(count + 1);
    return count;
}

jlong JNICALL nativeBlockVersion(JNIEnv*, jclass, jint handle) {
    if (handle < 0 || handle >= gHandleCount.load()) {
        return -1;
    }
    return static_cast<jlong>(gHandles[static_cast<std::size_t>(handle)].load()->blockVersion());
}

jint JNICALL nativeApplyChunks(JNIEnv* env, jclass, jstring name, jintArray chunks, jintArray sections,
                               jintArray states, jboolean resend, jintArray previous) {
    const auto world = worldOf(env, name);
    if (world == nullptr) {
        return -1;
    }
    const std::vector<std::int32_t> chunkList = ints(env, chunks);
    const std::vector<std::int32_t> sectionList = ints(env, sections);
    const std::vector<std::int32_t> next = ints(env, states);
    std::vector<std::int32_t> before(previous == nullptr ? 0 : static_cast<std::size_t>(env->GetArrayLength(previous)));
    const jint changed =
        guarded<jint>(-1, [&] { return world->applyChunks(chunkList, sectionList, next, resend == JNI_TRUE, before); });
    if (previous != nullptr && !before.empty()) {
        env->SetIntArrayRegion(previous, 0, static_cast<jsize>(before.size()), before.data());
    }
    return changed;
}

jobjectArray JNICALL nativeBlockEntityTypes(JNIEnv* env, jclass) {
    return stringArray(env, worlds().blockEntityTypes());
}

jint JNICALL nativeDataVersion(JNIEnv*, jclass) { return worlds().dataVersion(); }

jbyteArray JNICALL nativeFixNbt(JNIEnv* env, jclass, jstring kind, jbyteArray nbt, jint fromVersion) {
    const std::vector<std::uint8_t> data = bytes(env, nbt);
    return byteArray(env, worlds().fixNbt(toStd(env, kind), data, fromVersion));
}

jstring JNICALL nativeFixName(JNIEnv* env, jclass, jstring kind, jstring text, jint fromVersion) {
    return javaOrNull(env, worlds().fixName(toStd(env, kind), toStd(env, text), fromVersion));
}

jbyteArray JNICALL nativeBlockEntityNbt(JNIEnv* env, jclass, jstring name, jint x, jint y, jint z) {
    const auto world = worldOf(env, name);
    if (world == nullptr) {
        return nullptr;
    }
    return byteArray(env, guarded<std::optional<std::vector<std::uint8_t>>>(
                              std::nullopt, [&] { return world->blockEntityNbt(x, y, z); }));
}

jboolean JNICALL nativeSetBlockEntityNbt(JNIEnv* env, jclass, jstring name, jint x, jint y, jint z, jbyteArray nbt) {
    const auto world = worldOf(env, name);
    const std::vector<std::uint8_t> data = bytes(env, nbt);
    return world != nullptr && guarded<bool>(false, [&] { return world->setBlockEntityNbt(x, y, z, data); })
               ? JNI_TRUE
               : JNI_FALSE;
}

jintArray JNICALL nativeBlockEntityPositions(JNIEnv* env, jclass, jstring name, jint chunkX, jint chunkZ) {
    const auto world = worldOf(env, name);
    return intArray(env, world == nullptr ? std::vector<std::int32_t>{} : guarded<std::vector<std::int32_t>>({}, [&] {
                        return world->blockEntityPositions(chunkX, chunkZ);
                    }));
}

jbyteArray JNICALL nativeEntityNbt(JNIEnv* env, jclass, jstring name, jint id) {
    const auto world = worldOf(env, name);
    if (world == nullptr) {
        return nullptr;
    }
    return byteArray(
        env, guarded<std::optional<std::vector<std::uint8_t>>>(std::nullopt, [&] { return world->entityNbt(id); }));
}

jint JNICALL nativeSpawnEntityNbt(JNIEnv* env, jclass, jstring name, jbyteArray nbt, jstring cause) {
    const auto world = worldOf(env, name);
    const std::vector<std::uint8_t> data = bytes(env, nbt);
    const std::string why = toStd(env, cause);
    return world == nullptr ? -1 : guarded<jint>(-1, [&] { return world->spawnEntityNbt(data, why); });
}

jboolean JNICALL nativeCreateBlockEntity(JNIEnv* env, jclass, jstring name, jint x, jint y, jint z, jbyteArray nbt) {
    const auto world = worldOf(env, name);
    const std::vector<std::uint8_t> data = bytes(env, nbt);
    return world != nullptr && guarded<bool>(false, [&] { return world->createBlockEntity(x, y, z, data); })
               ? JNI_TRUE
               : JNI_FALSE;
}

jboolean JNICALL nativeRemoveBlockEntity(JNIEnv* env, jclass, jstring name, jint x, jint y, jint z) {
    const auto world = worldOf(env, name);
    return world != nullptr && guarded<bool>(false, [&] { return world->removeBlockEntity(x, y, z); }) ? JNI_TRUE
                                                                                                       : JNI_FALSE;
}

jint JNICALL nativeEntityByUuid(JNIEnv* env, jclass, jstring name, jstring uuid) {
    const auto world = worldOf(env, name);
    const std::string wanted = toStd(env, uuid);
    return world == nullptr ? -1 : guarded<jint>(-1, [&] {
        const std::shared_ptr<PluginEntityRef> found = world->entityByUuid(wanted);
        return found == nullptr ? -1 : found->id();
    });
}

jboolean JNICALL nativeRemoveEntityByUuid(JNIEnv* env, jclass, jstring name, jstring uuid) {
    const auto world = worldOf(env, name);
    const std::string wanted = toStd(env, uuid);
    return world != nullptr && guarded<bool>(false, [&] { return world->removeEntityByUuid(wanted); }) ? JNI_TRUE
                                                                                                       : JNI_FALSE;
}

jboolean JNICALL nativeReadBiomes(JNIEnv* env, jclass, jstring name, jint chunkX, jint chunkZ, jintArray out) {
    const auto world = worldOf(env, name);
    if (world == nullptr) {
        return JNI_FALSE;
    }
    std::vector<std::int32_t> read(static_cast<std::size_t>(env->GetArrayLength(out)));
    const bool ok = guarded<bool>(false, [&] { return world->readBiomes(chunkX, chunkZ, read); });
    if (ok) {
        env->SetIntArrayRegion(out, 0, static_cast<jsize>(read.size()), read.data());
    }
    return ok ? JNI_TRUE : JNI_FALSE;
}

jboolean JNICALL nativeWriteBiomes(JNIEnv* env, jclass, jstring name, jint chunkX, jint chunkZ, jintArray ids) {
    const auto world = worldOf(env, name);
    const std::vector<std::int32_t> next = ints(env, ids);
    return world != nullptr && guarded<bool>(false, [&] { return world->writeBiomes(chunkX, chunkZ, next); })
               ? JNI_TRUE
               : JNI_FALSE;
}

void JNICALL nativeRelightChunks(JNIEnv* env, jclass, jstring name, jintArray chunks) {
    const auto world = worldOf(env, name);
    if (world == nullptr) {
        return;
    }
    const std::vector<std::int32_t> pairs = ints(env, chunks);
    guarded<int>(0, [&] {
        world->relightChunks(pairs);
        return 0;
    });
}

jboolean JNICALL nativeReadSections(JNIEnv* env, jclass, jstring name, jint chunkX, jint chunkZ, jboolean load,
                                    jintArray out) {
    const auto world = worldOf(env, name);
    if (world == nullptr) {
        return JNI_FALSE;
    }
    std::vector<std::int32_t> read(static_cast<std::size_t>(env->GetArrayLength(out)));
    // A loaded chunk is copied under the world's read lock, so no thread hop is needed.
    bool ok = world->readSections(chunkX, chunkZ, false, read);
    if (!ok && load == JNI_TRUE) {
        ok = guarded<bool>(false, [&] { return world->readSections(chunkX, chunkZ, true, read); });
    }
    if (ok) {
        env->SetIntArrayRegion(out, 0, static_cast<jsize>(read.size()), read.data());
    }
    return ok ? JNI_TRUE : JNI_FALSE;
}

jint JNICALL nativeChunkStatus(JNIEnv* env, jclass, jstring name, jint chunkX, jint chunkZ) {
    const auto world = worldOf(env, name);
    return world == nullptr ? 0 : world->chunkStatus(chunkX, chunkZ);
}
} // namespace

void registerWorldNativesChunks(JNIEnv* env, jclass natives) {
    const std::array methods{
        nativeMethod("worldHandle", "(Ljava/lang/String;)I", reinterpret_cast<void*>(&nativeWorldHandle)),
        nativeMethod("blockVersion", "(I)J", reinterpret_cast<void*>(&nativeBlockVersion)),
        nativeMethod("applyChunks", "(Ljava/lang/String;[I[I[IZ[I)I", reinterpret_cast<void*>(&nativeApplyChunks)),
        nativeMethod("blockEntityNbt", "(Ljava/lang/String;III)[B", reinterpret_cast<void*>(&nativeBlockEntityNbt)),
        nativeMethod("setBlockEntityNbt", "(Ljava/lang/String;III[B)Z",
                     reinterpret_cast<void*>(&nativeSetBlockEntityNbt)),
        nativeMethod("blockEntityPositions", "(Ljava/lang/String;II)[I",
                     reinterpret_cast<void*>(&nativeBlockEntityPositions)),
        nativeMethod("entityNbt", "(Ljava/lang/String;I)[B", reinterpret_cast<void*>(&nativeEntityNbt)),
        nativeMethod("spawnEntityNbt", "(Ljava/lang/String;[BLjava/lang/String;)I",
                     reinterpret_cast<void*>(&nativeSpawnEntityNbt)),
        nativeMethod("blockEntityTypes", "()[Ljava/lang/String;", reinterpret_cast<void*>(&nativeBlockEntityTypes)),
        nativeMethod("dataVersion", "()I", reinterpret_cast<void*>(&nativeDataVersion)),
        nativeMethod("fixNbt", "(Ljava/lang/String;[BI)[B", reinterpret_cast<void*>(&nativeFixNbt)),
        nativeMethod("fixName", "(Ljava/lang/String;Ljava/lang/String;I)Ljava/lang/String;",
                     reinterpret_cast<void*>(&nativeFixName)),
        nativeMethod("createBlockEntity", "(Ljava/lang/String;III[B)Z",
                     reinterpret_cast<void*>(&nativeCreateBlockEntity)),
        nativeMethod("removeBlockEntity", "(Ljava/lang/String;III)Z",
                     reinterpret_cast<void*>(&nativeRemoveBlockEntity)),
        nativeMethod("entityByUuid", "(Ljava/lang/String;Ljava/lang/String;)I",
                     reinterpret_cast<void*>(&nativeEntityByUuid)),
        nativeMethod("removeEntityByUuid", "(Ljava/lang/String;Ljava/lang/String;)Z",
                     reinterpret_cast<void*>(&nativeRemoveEntityByUuid)),
        nativeMethod("readBiomes", "(Ljava/lang/String;II[I)Z", reinterpret_cast<void*>(&nativeReadBiomes)),
        nativeMethod("writeBiomes", "(Ljava/lang/String;II[I)Z", reinterpret_cast<void*>(&nativeWriteBiomes)),
        nativeMethod("relightChunks", "(Ljava/lang/String;[I)V", reinterpret_cast<void*>(&nativeRelightChunks)),
        nativeMethod("readSections", "(Ljava/lang/String;IIZ[I)Z", reinterpret_cast<void*>(&nativeReadSections)),
        nativeMethod("chunkStatus", "(Ljava/lang/String;II)I", reinterpret_cast<void*>(&nativeChunkStatus)),
    };
    registerMethods(env, natives, methods.data(), static_cast<jint>(methods.size()));
}
} // namespace basalt::server::bukkit
