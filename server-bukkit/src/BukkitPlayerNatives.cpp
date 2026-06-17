#include "BridgeValues.hpp"
#include "JniSupport.hpp"

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace basalt::server::bukkit {
namespace {

using plugins::PluginEntityValue;
using plugins::PluginPlayerRef;

[[nodiscard]] std::shared_ptr<PluginPlayerRef> playerOf(JNIEnv* env, jstring uuid) {
    return BukkitBridge::current().services().players().byUuid(toStd(env, uuid));
}

jobject JNICALL nativeGet(JNIEnv* env, jclass, jstring uuid, jstring name) {
    const auto player = playerOf(env, uuid);
    if (player == nullptr) {
        return nullptr;
    }
    const std::string property = toStd(env, name);
    return jni::boxed(
        env, jni::onServer<std::optional<PluginEntityValue>>(std::nullopt, [&] { return player->property(property); }),
        false);
}

jboolean JNICALL nativeSet(JNIEnv* env, jclass, jstring uuid, jstring name, jobject value) {
    const auto player = playerOf(env, uuid);
    if (player == nullptr) {
        return JNI_FALSE;
    }
    const std::string property = toStd(env, name);
    const PluginEntityValue converted = jni::fromJava(env, value);
    return jni::onServer<bool>(false, [&] { return player->setProperty(property, converted); }) ? JNI_TRUE : JNI_FALSE;
}

jobject JNICALL nativeCall(JNIEnv* env, jclass, jstring uuid, jstring operation, jobjectArray arguments) {
    const auto player = playerOf(env, uuid);
    if (player == nullptr) {
        return nullptr;
    }
    const std::string name = toStd(env, operation);
    std::vector<PluginEntityValue> converted;
    const jsize count = arguments == nullptr ? 0 : env->GetArrayLength(arguments);
    for (jsize at = 0; at < count; ++at) {
        jobject item = env->GetObjectArrayElement(arguments, at);
        converted.push_back(jni::fromJava(env, item));
        env->DeleteLocalRef(item);
    }
    return jni::boxed(
        env,
        jni::onServer<std::optional<PluginEntityValue>>(std::nullopt, [&] { return player->invoke(name, converted); }),
        name == "cookie_answer" || name == "ender_slot");
}

} // namespace

void registerPlayerNatives(JNIEnv* env, jclass natives) {
    const auto method = [](const char* name, const char* signature, void* function) {
        return JNINativeMethod{const_cast<char*>(name), const_cast<char*>(signature), function};
    };
    const std::array<JNINativeMethod, 3> methods{
        method("get", "(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/Object;", reinterpret_cast<void*>(&nativeGet)),
        method("set", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/Object;)Z", reinterpret_cast<void*>(&nativeSet)),
        method("call", "(Ljava/lang/String;Ljava/lang/String;[Ljava/lang/Object;)Ljava/lang/Object;",
               reinterpret_cast<void*>(&nativeCall)),
    };
    env->RegisterNatives(natives, methods.data(), static_cast<jint>(methods.size()));
}

} // namespace basalt::server::bukkit
