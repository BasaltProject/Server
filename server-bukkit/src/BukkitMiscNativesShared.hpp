#pragma once

#include "JniSupport.hpp"
#include "NativeTable.hpp"

namespace basalt::server::bukkit {
void registerMiscNativesData(JNIEnv* env, jclass natives);
void registerMiscNativesBoard(JNIEnv* env, jclass natives);
void registerMiscNativesExtras(JNIEnv* env, jclass natives);
void registerMiscNativesMaps(JNIEnv* env, jclass natives);

namespace shared {
using plugins::PluginWorldRef;

[[nodiscard]] inline std::shared_ptr<PluginWorldRef> worldOf(JNIEnv* env, jstring name) {
    return BukkitBridge::current().services().worlds().byName(toStd(env, name));
}

[[nodiscard]] inline plugins::PluginStorage& storage() { return BukkitBridge::current().services().storage(); }

using Blob = std::optional<std::vector<std::uint8_t>>;

[[nodiscard]] inline plugins::PluginAccess& access() { return BukkitBridge::current().services().access(); }

template <typename Result, typename Body>
[[nodiscard]] Result accessCall(Result fallback, Body&& body) {
    return jni::onServer<Result>(std::move(fallback), std::forward<Body>(body));
}

[[nodiscard]] inline jboolean flag(bool value) { return value ? JNI_TRUE : JNI_FALSE; }

[[nodiscard]] inline plugins::PluginServerControl& control() {
    return BukkitBridge::current().services().serverControl();
}

} // namespace shared
} // namespace basalt::server::bukkit
