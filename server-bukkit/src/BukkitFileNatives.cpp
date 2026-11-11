#include "JniSupport.hpp"
#include "NativeTable.hpp"

#include <basalt/server/plugins/PluginFiles.hpp>

#include <iterator>

namespace basalt::server::bukkit {
namespace {
namespace files = plugins::fileService;

[[nodiscard]] jboolean flag(bool value) { return value ? JNI_TRUE : JNI_FALSE; }

jlongArray JNICALL nativeStat(JNIEnv* env, jclass, jstring path) {
    const std::optional<files::Stat> found = files::stat(toStd(env, path));
    if (!found.has_value()) {
        return nullptr;
    }
    const jlong values[] = {static_cast<jlong>(found->kind), found->size, found->modifiedMillis};
    jlongArray array = env->NewLongArray(3);
    env->SetLongArrayRegion(array, 0, 3, values);
    return array;
}

jobjectArray JNICALL nativeWalk(JNIEnv* env, jclass, jstring directory) {
    return jni::stringArrayOf(env, files::walk(toStd(env, directory)));
}

jobjectArray JNICALL nativeList(JNIEnv* env, jclass, jstring directory) {
    const std::optional<std::vector<std::string>> names = files::list(toStd(env, directory));
    return names.has_value() ? jni::stringArrayOf(env, *names) : nullptr;
}

jbyteArray JNICALL nativeRead(JNIEnv* env, jclass, jstring path) {
    return jni::byteArrayOf(env, files::read(toStd(env, path)));
}

jboolean JNICALL nativeWrite(JNIEnv* env, jclass, jstring path, jbyteArray bytes) {
    return flag(files::write(toStd(env, path), jni::bytesOf(env, bytes)));
}

jboolean JNICALL nativeMakeDirectories(JNIEnv* env, jclass, jstring path) {
    return flag(files::makeDirectories(toStd(env, path)));
}

jboolean JNICALL nativeCopy(JNIEnv* env, jclass, jstring from, jstring to) {
    return flag(files::copyFile(toStd(env, from), toStd(env, to)));
}

jboolean JNICALL nativeLink(JNIEnv* env, jclass, jstring from, jstring to) {
    return flag(files::hardLink(toStd(env, from), toStd(env, to)));
}

jboolean JNICALL nativeMove(JNIEnv* env, jclass, jstring from, jstring to, jboolean atomic) {
    return flag(files::move(toStd(env, from), toStd(env, to), atomic == JNI_TRUE));
}

jboolean JNICALL nativeRemove(JNIEnv* env, jclass, jstring path) { return flag(files::remove(toStd(env, path))); }

jboolean JNICALL nativeRemoveTree(JNIEnv* env, jclass, jstring path) {
    return flag(files::removeTree(toStd(env, path)));
}

jint JNICALL nativeCapabilities(JNIEnv* env, jclass, jstring directory) {
    const files::Capabilities found = files::capabilities(toStd(env, directory));
    return (found.atomicMove ? 1 : 0) | (found.hardLinks ? 2 : 0);
}
} // namespace

void registerFileNatives(JNIEnv* env, jclass natives) {
    const JNINativeMethod methods[] = {
        nativeMethod("stat", "(Ljava/lang/String;)[J", reinterpret_cast<void*>(&nativeStat)),
        nativeMethod("walk", "(Ljava/lang/String;)[Ljava/lang/String;", reinterpret_cast<void*>(&nativeWalk)),
        nativeMethod("list", "(Ljava/lang/String;)[Ljava/lang/String;", reinterpret_cast<void*>(&nativeList)),
        nativeMethod("read", "(Ljava/lang/String;)[B", reinterpret_cast<void*>(&nativeRead)),
        nativeMethod("write", "(Ljava/lang/String;[B)Z", reinterpret_cast<void*>(&nativeWrite)),
        nativeMethod("makeDirectories", "(Ljava/lang/String;)Z", reinterpret_cast<void*>(&nativeMakeDirectories)),
        nativeMethod("copy", "(Ljava/lang/String;Ljava/lang/String;)Z", reinterpret_cast<void*>(&nativeCopy)),
        nativeMethod("link", "(Ljava/lang/String;Ljava/lang/String;)Z", reinterpret_cast<void*>(&nativeLink)),
        nativeMethod("move", "(Ljava/lang/String;Ljava/lang/String;Z)Z", reinterpret_cast<void*>(&nativeMove)),
        nativeMethod("remove", "(Ljava/lang/String;)Z", reinterpret_cast<void*>(&nativeRemove)),
        nativeMethod("removeTree", "(Ljava/lang/String;)Z", reinterpret_cast<void*>(&nativeRemoveTree)),
        nativeMethod("capabilities", "(Ljava/lang/String;)I", reinterpret_cast<void*>(&nativeCapabilities)),
    };
    registerMethods(env, natives, methods, static_cast<jint>(std::size(methods)));
}
} // namespace basalt::server::bukkit
