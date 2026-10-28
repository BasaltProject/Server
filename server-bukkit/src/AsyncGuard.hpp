#pragma once

#include "BridgeInternal.hpp"

#include <string>
#include <utility>

namespace basalt::server::bukkit::jni {
[[nodiscard]] inline bool onServerThread() {
    BukkitBridge& bridge = BukkitBridge::current();
    return bridge.stopping() || bridge.plugins().onPluginThread();
}

/// Throws Paper's IllegalStateException off the server thread; the native must return at once when this is true.
[[nodiscard]] inline bool refuseAsync(JNIEnv* env, const char* operation) {
    if (onServerThread()) {
        return false;
    }
    jclass type = env->FindClass("java/lang/IllegalStateException");
    if (type != nullptr) {
        const std::string message = std::string{"Asynchronous "} + operation + "!";
        env->ThrowNew(type, message.c_str());
        env->DeleteLocalRef(type);
    }
    return true;
}

/// Runs now on the server thread, or is queued for the next tick without waiting for it.
template <typename Body>
void postToServer(Body&& body) {
    if (onServerThread()) {
        body();
        return;
    }
    BukkitBridge& bridge = BukkitBridge::current();
    static_cast<void>(bridge.services().tasks().after(0, std::function<void()>(std::forward<Body>(body))));
}
} // namespace basalt::server::bukkit::jni
