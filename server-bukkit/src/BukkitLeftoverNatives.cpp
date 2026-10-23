#include "JniSupport.hpp"

namespace basalt::server::bukkit {
namespace {
using Blob = std::optional<std::vector<std::uint8_t>>;

[[nodiscard]] jboolean flag(bool value) { return value ? JNI_TRUE : JNI_FALSE; }

[[nodiscard]] plugins::PluginStructures& structures() { return BukkitBridge::current().services().structures(); }

jobjectArray JNICALL nativeStructureIds(JNIEnv* env, jclass) {
    return jni::stringArrayOf(env, jni::onServer<std::vector<std::string>>({}, [] { return structures().ids(); }));
}

jbyteArray JNICALL nativeStructureRead(JNIEnv* env, jclass, jstring id) {
    const std::string name = toStd(env, id);
    return jni::byteArrayOf(env, jni::onServer<Blob>(std::nullopt, [&] { return structures().read(name); }));
}

jboolean JNICALL nativeStructureStore(JNIEnv* env, jclass, jstring id, jbyteArray nbt, jboolean toDisk) {
    const std::string name = toStd(env, id);
    const std::vector<std::uint8_t> data = jni::bytesOf(env, nbt);
    return flag(jni::onServer<bool>(false, [&] { return structures().store(name, data, toDisk == JNI_TRUE); }));
}

jboolean JNICALL nativeStructureRemove(JNIEnv* env, jclass, jstring id, jboolean fromDisk) {
    const std::string name = toStd(env, id);
    return flag(jni::onServer<bool>(false, [&] { return structures().remove(name, fromDisk == JNI_TRUE); }));
}

jstring JNICALL nativeStructureFile(JNIEnv* env, jclass, jstring id) {
    const std::string name = toStd(env, id);
    return toJava(env, jni::onServer<std::string>({}, [&] { return structures().file(name); }));
}

jbyteArray JNICALL nativeStructureCapture(JNIEnv* env, jclass, jstring world, jint x, jint y, jint z, jint sizeX,
                                          jint sizeY, jint sizeZ, jboolean entities) {
    const std::string name = toStd(env, world);
    return jni::byteArrayOf(env, jni::onServer<Blob>(std::nullopt, [&] {
                                return structures().capture(name, x, y, z, sizeX, sizeY, sizeZ, entities == JNI_TRUE);
                            }));
}

jboolean JNICALL nativeStructurePlace(JNIEnv* env, jclass, jbyteArray nbt, jstring world, jint x, jint y, jint z,
                                      jint rotation, jint mirror, jfloat integrity, jlong seed, jboolean entities,
                                      jint palette) {
    const std::vector<std::uint8_t> data = jni::bytesOf(env, nbt);
    const plugins::PluginStructurePlacement placement{.world = toStd(env, world),
                                                      .x = x,
                                                      .y = y,
                                                      .z = z,
                                                      .rotation = rotation,
                                                      .mirror = mirror,
                                                      .integrity = integrity,
                                                      .seed = seed,
                                                      .entities = entities == JNI_TRUE,
                                                      .palette = palette};
    return flag(jni::onServer<bool>(false, [&] { return structures().place(data, placement); }));
}

jbyteArray JNICALL nativeExplorerMap(JNIEnv* env, jclass, jstring world, jdouble x, jdouble y, jdouble z,
                                     jobjectArray destination, jstring decoration, jint zoom, jint radius,
                                     jboolean skipExisting) {
    const std::string name = toStd(env, world);
    plugins::PluginExploration request;
    request.destination = jni::stringsOf(env, destination);
    request.decoration = toStd(env, decoration);
    request.zoom = zoom;
    request.radius = radius;
    request.skipExisting = skipExisting == JNI_TRUE;
    return jni::byteArrayOf(env, jni::onServer<Blob>(std::nullopt, [&] {
                                return BukkitBridge::current().services().maps().explorerMap(name, x, y, z, request);
                            }));
}
} // namespace

void registerLeftoverNatives(JNIEnv* env, jclass natives) {
    const auto method = [](const char* name, const char* signature, void* function) {
        return JNINativeMethod{const_cast<char*>(name), const_cast<char*>(signature), function};
    };
    const std::array methods{
        method("structureIds", "()[Ljava/lang/String;", reinterpret_cast<void*>(&nativeStructureIds)),
        method("structureRead", "(Ljava/lang/String;)[B", reinterpret_cast<void*>(&nativeStructureRead)),
        method("structureStore", "(Ljava/lang/String;[BZ)Z", reinterpret_cast<void*>(&nativeStructureStore)),
        method("structureRemove", "(Ljava/lang/String;Z)Z", reinterpret_cast<void*>(&nativeStructureRemove)),
        method("structureFile", "(Ljava/lang/String;)Ljava/lang/String;",
               reinterpret_cast<void*>(&nativeStructureFile)),
        method("structureCapture", "(Ljava/lang/String;IIIIIIZ)[B", reinterpret_cast<void*>(&nativeStructureCapture)),
        method("structurePlace", "([BLjava/lang/String;IIIIIFJZI)Z", reinterpret_cast<void*>(&nativeStructurePlace)),
        method("explorerMap", "(Ljava/lang/String;DDD[Ljava/lang/String;Ljava/lang/String;IIZ)[B",
               reinterpret_cast<void*>(&nativeExplorerMap)),
    };
    if (env->RegisterNatives(natives, methods.data(), static_cast<jint>(methods.size())) != 0) {
        BukkitBridge::current().log().error(
            [] { return std::string{"LeftoverNatives has a native the runtime does not declare"}; });
        env->ExceptionDescribe();
        env->ExceptionClear();
    }
}
} // namespace basalt::server::bukkit
