#include "BridgeValues.hpp"
#include "JniSupport.hpp"

#include <basalt/wire/nbt/NbtIO.hpp>
#include <basalt/wire/nbt/Snbt.hpp>

#include <array>
#include <chrono>
#include <exception>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <variant>

namespace basalt::server::bukkit {
namespace {

using plugins::PluginEntityRef;
using plugins::PluginEntityValue;

[[nodiscard]] std::shared_ptr<PluginEntityRef> entityOf(JNIEnv* env, jstring world, jint id) {
    const auto found = BukkitBridge::current().services().worlds().byName(toStd(env, world));
    return found == nullptr ? nullptr : found->entityById(id);
}

template <typename Result, typename Body>
[[nodiscard]] Result guarded(Result fallback, Body&& body) {
    return jni::onServer<Result>(std::move(fallback), std::forward<Body>(body));
}

jobject JNICALL nativeGet(JNIEnv* env, jclass, jstring world, jint id, jstring name) {
    const auto entity = entityOf(env, world, id);
    if (entity == nullptr) {
        return nullptr;
    }
    const std::string property = toStd(env, name);
    return jni::boxed(
        env, guarded<std::optional<PluginEntityValue>>(std::nullopt, [&] { return entity->property(property); }),
        false);
}

jboolean JNICALL nativeSet(JNIEnv* env, jclass, jstring world, jint id, jstring name, jobject value) {
    const auto entity = entityOf(env, world, id);
    if (entity == nullptr) {
        return JNI_FALSE;
    }
    const std::string property = toStd(env, name);
    const PluginEntityValue converted = jni::fromJava(env, value);
    return guarded<bool>(false, [&] { return entity->setProperty(property, converted); }) ? JNI_TRUE : JNI_FALSE;
}

jobject JNICALL nativeCall(JNIEnv* env, jclass, jstring world, jint id, jstring operation, jobjectArray arguments) {
    const auto entity = entityOf(env, world, id);
    if (entity == nullptr) {
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
    const auto result =
        guarded<std::optional<PluginEntityValue>>(std::nullopt, [&] { return entity->invoke(name, converted); });
    return jni::boxed(env, result, name == "nbt" || name == "equipment" || name == "held_get");
}

jint JNICALL nativeSpawnAs(JNIEnv* env, jclass, jstring world, jstring type, jdouble x, jdouble y, jdouble z,
                           jfloat yaw, jstring reason) {
    const auto found = BukkitBridge::current().services().worlds().byName(toStd(env, world));
    if (found == nullptr) {
        return -1;
    }
    const std::string kind = toStd(env, type);
    const std::string why = toStd(env, reason);
    const auto made =
        guarded<std::shared_ptr<PluginEntityRef>>(nullptr, [&] { return found->spawnAs(kind, x, y, z, yaw, why); });
    return made == nullptr ? -1 : made->id();
}

jint JNICALL nativeDropItem(JNIEnv* env, jclass, jstring world, jbyteArray stack, jdouble x, jdouble y, jdouble z,
                            jdouble vx, jdouble vy, jdouble vz) {
    const auto found = BukkitBridge::current().services().worlds().byName(toStd(env, world));
    if (found == nullptr || stack == nullptr) {
        return -1;
    }
    const jsize length = env->GetArrayLength(stack);
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
    env->GetByteArrayRegion(stack, 0, length, reinterpret_cast<jbyte*>(bytes.data()));
    return guarded<jint>(-1, [&] { return found->dropItem(bytes, x, y, z, vx, vy, vz); });
}

jboolean JNICALL nativeExplode(JNIEnv* env, jclass, jstring world, jdouble x, jdouble y, jdouble z, jfloat power,
                               jboolean breakBlocks, jint source) {
    const auto found = BukkitBridge::current().services().worlds().byName(toStd(env, world));
    return found != nullptr &&
                   guarded<bool>(false, [&] { return found->explode(x, y, z, power, breakBlocks == JNI_TRUE, source); })
               ? JNI_TRUE
               : JNI_FALSE;
}

jbyteArray JNICALL nativeSnbtToNbt(JNIEnv* env, jclass, jstring text) {
    const auto parsed = wire::nbt::snbt::parse(toStd(env, text));
    if (!parsed.has_value() || parsed->get<wire::nbt::NbtCompound>() == nullptr) {
        return nullptr;
    }
    const std::vector<std::uint8_t> bytes = wire::nbt::nbtIO::write(*parsed);
    jbyteArray array = env->NewByteArray(static_cast<jsize>(bytes.size()));
    env->SetByteArrayRegion(array, 0, static_cast<jsize>(bytes.size()), reinterpret_cast<const jbyte*>(bytes.data()));
    return array;
}

jstring JNICALL nativeNbtToSnbt(JNIEnv* env, jclass, jbyteArray nbt) {
    const jsize length = env->GetArrayLength(nbt);
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
    env->GetByteArrayRegion(nbt, 0, length, reinterpret_cast<jbyte*>(bytes.data()));
    const auto parsed = wire::nbt::nbtIO::read(bytes);
    return parsed.has_value() ? toJava(env, wire::nbt::snbt::stringify(parsed->tag)) : nullptr;
}

} // namespace

void registerEntityNatives(JNIEnv* env, jclass natives) {
    const auto method = [](const char* name, const char* signature, void* function) {
        return JNINativeMethod{const_cast<char*>(name), const_cast<char*>(signature), function};
    };
    const std::array<JNINativeMethod, 8> methods{
        method("get", "(Ljava/lang/String;ILjava/lang/String;)Ljava/lang/Object;", reinterpret_cast<void*>(&nativeGet)),
        method("set", "(Ljava/lang/String;ILjava/lang/String;Ljava/lang/Object;)Z",
               reinterpret_cast<void*>(&nativeSet)),
        method("call", "(Ljava/lang/String;ILjava/lang/String;[Ljava/lang/Object;)Ljava/lang/Object;",
               reinterpret_cast<void*>(&nativeCall)),
        method("spawnAs", "(Ljava/lang/String;Ljava/lang/String;DDDFLjava/lang/String;)I",
               reinterpret_cast<void*>(&nativeSpawnAs)),
        method("dropItem", "(Ljava/lang/String;[BDDDDDD)I", reinterpret_cast<void*>(&nativeDropItem)),
        method("explode", "(Ljava/lang/String;DDDFZI)Z", reinterpret_cast<void*>(&nativeExplode)),
        method("snbtToNbt", "(Ljava/lang/String;)[B", reinterpret_cast<void*>(&nativeSnbtToNbt)),
        method("nbtToSnbt", "([B)Ljava/lang/String;", reinterpret_cast<void*>(&nativeNbtToSnbt)),
    };
    env->RegisterNatives(natives, methods.data(), static_cast<jint>(methods.size()));
}

} // namespace basalt::server::bukkit
