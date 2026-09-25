#pragma once

#include "BridgeInternal.hpp"

#include <basalt/server/plugins/PluginEntityOps.hpp>

#include <string>
#include <variant>
#include <vector>

namespace basalt::server::bukkit::jni {
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

[[nodiscard]] inline Boxes makeBoxes(JNIEnv* env) {
    Boxes cached;
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

[[nodiscard]] inline Boxes& boxes(JNIEnv* env) {
    static Boxes cached = makeBoxes(env);
    return cached;
}

[[nodiscard]] inline PluginEntityValue fromJava(JNIEnv* env, jobject value) {
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

[[nodiscard]] inline jobject boxed(JNIEnv* env, const std::optional<PluginEntityValue>& value, bool binary) {
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

} // namespace basalt::server::bukkit::jni
