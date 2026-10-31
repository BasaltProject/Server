#pragma once

#include "BridgeInternal.hpp"
#include "NativeTable.hpp"
#include "ServerThreadCall.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <exception>
#include <future>
#include <memory>
#include <mutex>

namespace basalt::server::bukkit {
void registerWorldNativesRegistry(JNIEnv* env, jclass natives);
void registerWorldNativesBlocks(JNIEnv* env, jclass natives);
void registerWorldNativesEntities(JNIEnv* env, jclass natives);
void registerWorldNativesChunks(JNIEnv* env, jclass natives);
void registerWorldNativesJobs(JNIEnv* env, jclass natives);

namespace shared {
using plugins::PluginEntityRef;
using plugins::PluginItem;
using plugins::PluginPlayerRef;
using plugins::PluginWorldRef;

[[nodiscard]] inline plugins::PluginWorlds& worlds() { return BukkitBridge::current().services().worlds(); }

[[nodiscard]] inline std::shared_ptr<PluginWorldRef> worldOf(JNIEnv* env, jstring name) {
    return worlds().byName(toStd(env, name));
}

[[nodiscard]] inline std::shared_ptr<PluginPlayerRef> playerOf(JNIEnv* env, jstring uuid) {
    return BukkitBridge::current().services().players().byUuid(toStd(env, uuid));
}

[[nodiscard]] inline jstring javaOrNull(JNIEnv* env, const std::optional<std::string>& text) {
    return text.has_value() ? toJava(env, *text) : nullptr;
}

[[nodiscard]] inline jobjectArray stringArray(JNIEnv* env, const std::vector<std::string>& values) {
    jclass string = env->FindClass("java/lang/String");
    jobjectArray array = env->NewObjectArray(static_cast<jsize>(values.size()), string, nullptr);
    for (std::size_t index = 0; index < values.size(); ++index) {
        jstring text = toJava(env, values[index]);
        env->SetObjectArrayElement(array, static_cast<jsize>(index), text);
        env->DeleteLocalRef(text);
    }
    return array;
}

[[nodiscard]] inline jintArray intArray(JNIEnv* env, const std::vector<std::int32_t>& values) {
    jintArray array = env->NewIntArray(static_cast<jsize>(values.size()));
    env->SetIntArrayRegion(array, 0, static_cast<jsize>(values.size()), values.data());
    return array;
}

[[nodiscard]] inline jdoubleArray doubleArray(JNIEnv* env, const double* values, std::size_t count) {
    jdoubleArray array = env->NewDoubleArray(static_cast<jsize>(count));
    env->SetDoubleArrayRegion(array, 0, static_cast<jsize>(count), values);
    return array;
}

// No C++ exception may reach Java; a plugin worker thread has the call run on the tick thread and waits for it.
template <typename Result, typename Body>
[[nodiscard]] Result guarded(Result fallback, Body&& body) {
    const auto run = [&fallback, &body]() -> Result {
        try {
            return static_cast<Result>(body());
        } catch (const std::exception& error) {
            BukkitBridge::current().log().warn([&error] { return std::string{"world call failed: "} + error.what(); });
            return fallback;
        }
    };
    return callOnPluginThread(BukkitBridge::current(), fallback, run);
}

[[nodiscard]] inline std::vector<std::int32_t> ints(JNIEnv* env, jintArray array) {
    std::vector<std::int32_t> values(array == nullptr ? 0 : static_cast<std::size_t>(env->GetArrayLength(array)));
    if (!values.empty()) {
        env->GetIntArrayRegion(array, 0, static_cast<jsize>(values.size()), values.data());
    }
    return values;
}

[[nodiscard]] inline std::vector<std::uint8_t> bytes(JNIEnv* env, jbyteArray array) {
    std::vector<std::uint8_t> values(array == nullptr ? 0 : static_cast<std::size_t>(env->GetArrayLength(array)));
    if (!values.empty()) {
        env->GetByteArrayRegion(array, 0, static_cast<jsize>(values.size()), reinterpret_cast<jbyte*>(values.data()));
    }
    return values;
}

[[nodiscard]] inline jbyteArray byteArray(JNIEnv* env, const std::optional<std::vector<std::uint8_t>>& values) {
    if (!values.has_value()) {
        return nullptr;
    }
    jbyteArray array = env->NewByteArray(static_cast<jsize>(values->size()));
    env->SetByteArrayRegion(array, 0, static_cast<jsize>(values->size()),
                            reinterpret_cast<const jbyte*>(values->data()));
    return array;
}

} // namespace shared
} // namespace basalt::server::bukkit
