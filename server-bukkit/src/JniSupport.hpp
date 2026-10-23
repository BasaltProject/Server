#pragma once

#include "BridgeInternal.hpp"

#include <chrono>
#include <exception>
#include <future>
#include <optional>
#include <vector>

namespace basalt::server::bukkit::jni {
[[nodiscard]] inline std::vector<std::uint8_t> bytesOf(JNIEnv* env, jbyteArray array) {
    std::vector<std::uint8_t> values(array == nullptr ? 0 : static_cast<std::size_t>(env->GetArrayLength(array)));
    if (!values.empty()) {
        env->GetByteArrayRegion(array, 0, static_cast<jsize>(values.size()), reinterpret_cast<jbyte*>(values.data()));
    }
    return values;
}

[[nodiscard]] inline jbyteArray byteArrayOf(JNIEnv* env, const std::optional<std::vector<std::uint8_t>>& values) {
    if (!values.has_value()) {
        return nullptr;
    }
    jbyteArray array = env->NewByteArray(static_cast<jsize>(values->size()));
    env->SetByteArrayRegion(array, 0, static_cast<jsize>(values->size()),
                            reinterpret_cast<const jbyte*>(values->data()));
    return array;
}

[[nodiscard]] inline jobjectArray stringArrayOf(JNIEnv* env, const std::vector<std::string>& values) {
    jclass string = env->FindClass("java/lang/String");
    jobjectArray array = env->NewObjectArray(static_cast<jsize>(values.size()), string, nullptr);
    for (std::size_t index = 0; index < values.size(); ++index) {
        jstring text = toJava(env, values[index]);
        env->SetObjectArrayElement(array, static_cast<jsize>(index), text);
        env->DeleteLocalRef(text);
    }
    return array;
}

[[nodiscard]] inline std::vector<std::string> stringsOf(JNIEnv* env, jobjectArray array) {
    std::vector<std::string> values;
    const jsize count = array == nullptr ? 0 : env->GetArrayLength(array);
    for (jsize index = 0; index < count; ++index) {
        auto text = static_cast<jstring>(env->GetObjectArrayElement(array, index));
        values.push_back(text == nullptr ? std::string{} : toStd(env, text));
        env->DeleteLocalRef(text);
    }
    return values;
}

// No C++ exception may reach Java; a plugin worker thread has the call run on the tick thread and waits for it.
template <typename Result, typename Body>
[[nodiscard]] Result onServer(Result fallback, Body&& body) {
    const auto run = [&fallback, &body]() -> Result {
        try {
            return body();
        } catch (const std::exception& error) {
            BukkitBridge::current().log().warn([&error] { return std::string{"bridge call failed: "} + error.what(); });
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
} // namespace basalt::server::bukkit::jni
