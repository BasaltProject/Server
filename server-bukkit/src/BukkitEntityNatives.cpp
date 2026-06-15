#include "BridgeInternal.hpp"

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

struct Boxes {
    jclass booleanClass = nullptr;
    jclass longClass = nullptr;
    jclass integerClass = nullptr;
    jclass doubleClass = nullptr;
    jclass stringClass = nullptr;
    jclass stringArrayClass = nullptr;
    jclass bytesClass = nullptr;
    jmethodID booleanOf = nullptr;
    jmethodID longOf = nullptr;
    jmethodID doubleOf = nullptr;
    jmethodID booleanValue = nullptr;
    jmethodID longValue = nullptr;
    jmethodID integerValue = nullptr;
    jmethodID doubleValue = nullptr;
};

[[nodiscard]] Boxes& boxes(JNIEnv* env) {
    static Boxes cached;
    if (cached.booleanClass != nullptr) {
        return cached;
    }
    const auto global = [env](const char* name) {
        jclass local = env->FindClass(name);
        auto* made = static_cast<jclass>(env->NewGlobalRef(local));
        env->DeleteLocalRef(local);
        return made;
    };
    cached.booleanClass = global("java/lang/Boolean");
    cached.longClass = global("java/lang/Long");
    cached.integerClass = global("java/lang/Integer");
    cached.doubleClass = global("java/lang/Double");
    cached.stringClass = global("java/lang/String");
    cached.stringArrayClass = global("[Ljava/lang/String;");
    cached.bytesClass = global("[B");
    cached.booleanOf = env->GetStaticMethodID(cached.booleanClass, "valueOf", "(Z)Ljava/lang/Boolean;");
    cached.longOf = env->GetStaticMethodID(cached.longClass, "valueOf", "(J)Ljava/lang/Long;");
    cached.doubleOf = env->GetStaticMethodID(cached.doubleClass, "valueOf", "(D)Ljava/lang/Double;");
    cached.booleanValue = env->GetMethodID(cached.booleanClass, "booleanValue", "()Z");
    cached.longValue = env->GetMethodID(cached.longClass, "longValue", "()J");
    cached.integerValue = env->GetMethodID(cached.integerClass, "intValue", "()I");
    cached.doubleValue = env->GetMethodID(cached.doubleClass, "doubleValue", "()D");
    return cached;
}

[[nodiscard]] std::shared_ptr<PluginEntityRef> entityOf(JNIEnv* env, jstring world, jint id) {
    const auto found = BukkitBridge::current().services().worlds().byName(toStd(env, world));
    return found == nullptr ? nullptr : found->entityById(id);
}

template <typename Result, typename Body>
[[nodiscard]] Result guarded(Result fallback, Body&& body) {
    const auto run = [&fallback, &body]() -> Result {
        try {
            return body();
        } catch (const std::exception& error) {
            BukkitBridge::current().log().warn([&error] { return std::string{"entity call failed: "} + error.what(); });
            return fallback;
        }
    };
    BukkitBridge& bridge = BukkitBridge::current();
    if (bridge.plugins().onPluginThread()) {
        return run();
    }
    std::promise<Result> done;
    std::future<Result> result = done.get_future();
    static_cast<void>(bridge.services().tasks().after(0, [&done, &run] { done.set_value(run()); }));
    while (result.wait_for(std::chrono::milliseconds{50}) != std::future_status::ready) {
        if (bridge.stopping()) {
            return fallback;
        }
    }
    return result.get();
}

[[nodiscard]] PluginEntityValue fromJava(JNIEnv* env, jobject value) {
    if (value == nullptr) {
        return std::monostate{};
    }
    Boxes& box = boxes(env);
    if (env->IsInstanceOf(value, box.booleanClass) == JNI_TRUE) {
        return env->CallBooleanMethod(value, box.booleanValue) == JNI_TRUE;
    }
    if (env->IsInstanceOf(value, box.longClass) == JNI_TRUE) {
        return static_cast<std::int64_t>(env->CallLongMethod(value, box.longValue));
    }
    if (env->IsInstanceOf(value, box.integerClass) == JNI_TRUE) {
        return static_cast<std::int64_t>(env->CallIntMethod(value, box.integerValue));
    }
    if (env->IsInstanceOf(value, box.doubleClass) == JNI_TRUE) {
        return env->CallDoubleMethod(value, box.doubleValue);
    }
    if (env->IsInstanceOf(value, box.stringClass) == JNI_TRUE) {
        return toStd(env, static_cast<jstring>(value));
    }
    if (env->IsInstanceOf(value, box.stringArrayClass) == JNI_TRUE) {
        auto* array = static_cast<jobjectArray>(value);
        std::vector<std::string> list;
        const jsize length = env->GetArrayLength(array);
        for (jsize at = 0; at < length; ++at) {
            auto* item = static_cast<jstring>(env->GetObjectArrayElement(array, at));
            list.push_back(toStd(env, item));
            env->DeleteLocalRef(item);
        }
        return list;
    }
    if (env->IsInstanceOf(value, box.bytesClass) == JNI_TRUE) {
        auto* array = static_cast<jbyteArray>(value);
        const jsize length = env->GetArrayLength(array);
        std::string bytes(static_cast<std::size_t>(length), '\0');
        env->GetByteArrayRegion(array, 0, length, reinterpret_cast<jbyte*>(bytes.data()));
        return bytes;
    }
    return std::monostate{};
}

[[nodiscard]] jobject boxed(JNIEnv* env, const std::optional<PluginEntityValue>& value, bool binary) {
    if (!value.has_value() || std::holds_alternative<std::monostate>(*value)) {
        return nullptr;
    }
    Boxes& box = boxes(env);
    if (const auto* flag = std::get_if<bool>(&*value); flag != nullptr) {
        return env->CallStaticObjectMethod(box.booleanClass, box.booleanOf, *flag ? JNI_TRUE : JNI_FALSE);
    }
    if (const auto* whole = std::get_if<std::int64_t>(&*value); whole != nullptr) {
        return env->CallStaticObjectMethod(box.longClass, box.longOf, static_cast<jlong>(*whole));
    }
    if (const auto* real = std::get_if<double>(&*value); real != nullptr) {
        return env->CallStaticObjectMethod(box.doubleClass, box.doubleOf, *real);
    }
    if (const auto* text = std::get_if<std::string>(&*value); text != nullptr) {
        if (binary) {
            jbyteArray array = env->NewByteArray(static_cast<jsize>(text->size()));
            env->SetByteArrayRegion(array, 0, static_cast<jsize>(text->size()),
                                    reinterpret_cast<const jbyte*>(text->data()));
            return array;
        }
        return toJava(env, std::string_view{*text});
    }
    const auto& list = std::get<std::vector<std::string>>(*value);
    jobjectArray array = env->NewObjectArray(static_cast<jsize>(list.size()), box.stringClass, nullptr);
    for (std::size_t at = 0; at < list.size(); ++at) {
        jstring item = toJava(env, std::string_view{list[at]});
        env->SetObjectArrayElement(array, static_cast<jsize>(at), item);
        env->DeleteLocalRef(item);
    }
    return array;
}

jobject JNICALL nativeGet(JNIEnv* env, jclass, jstring world, jint id, jstring name) {
    const auto entity = entityOf(env, world, id);
    if (entity == nullptr) {
        return nullptr;
    }
    const std::string property = toStd(env, name);
    return boxed(env,
                 guarded<std::optional<PluginEntityValue>>(std::nullopt, [&] { return entity->property(property); }),
                 false);
}

jboolean JNICALL nativeSet(JNIEnv* env, jclass, jstring world, jint id, jstring name, jobject value) {
    const auto entity = entityOf(env, world, id);
    if (entity == nullptr) {
        return JNI_FALSE;
    }
    const std::string property = toStd(env, name);
    const PluginEntityValue converted = fromJava(env, value);
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
        converted.push_back(fromJava(env, item));
        env->DeleteLocalRef(item);
    }
    const auto result =
        guarded<std::optional<PluginEntityValue>>(std::nullopt, [&] { return entity->invoke(name, converted); });
    return boxed(env, result, name == "nbt" || name == "equipment");
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

} // namespace

void registerEntityNatives(JNIEnv* env, jclass natives) {
    const auto method = [](const char* name, const char* signature, void* function) {
        return JNINativeMethod{const_cast<char*>(name), const_cast<char*>(signature), function};
    };
    const std::array<JNINativeMethod, 5> methods{
        method("get", "(Ljava/lang/String;ILjava/lang/String;)Ljava/lang/Object;", reinterpret_cast<void*>(&nativeGet)),
        method("set", "(Ljava/lang/String;ILjava/lang/String;Ljava/lang/Object;)Z",
               reinterpret_cast<void*>(&nativeSet)),
        method("call", "(Ljava/lang/String;ILjava/lang/String;[Ljava/lang/Object;)Ljava/lang/Object;",
               reinterpret_cast<void*>(&nativeCall)),
        method("spawnAs", "(Ljava/lang/String;Ljava/lang/String;DDDFLjava/lang/String;)I",
               reinterpret_cast<void*>(&nativeSpawnAs)),
        method("dropItem", "(Ljava/lang/String;[BDDDDDD)I", reinterpret_cast<void*>(&nativeDropItem)),
    };
    env->RegisterNatives(natives, methods.data(), static_cast<jint>(methods.size()));
}

} // namespace basalt::server::bukkit
