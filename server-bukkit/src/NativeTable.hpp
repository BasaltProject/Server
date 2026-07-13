#pragma once

#include "BridgeInternal.hpp"

namespace basalt::server::bukkit {
void registerMethods(JNIEnv* env, jclass natives, const JNINativeMethod* methods, jint count);

[[nodiscard]] inline JNINativeMethod nativeMethod(const char* name, const char* signature, void* function) {
    return JNINativeMethod{const_cast<char*>(name), const_cast<char*>(signature), function};
}
} // namespace basalt::server::bukkit
