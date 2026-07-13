#include "BukkitMiscNativesShared.hpp"
#include "NativeTable.hpp"

namespace basalt::server::bukkit {
void registerMethods(JNIEnv* env, jclass natives, const JNINativeMethod* methods, jint count) {
    if (env->RegisterNatives(natives, methods, count) != 0) {
        BukkitBridge::current().log().error(
            [] { return std::string{"a native table has a method the runtime does not declare"}; });
        env->ExceptionDescribe();
        env->ExceptionClear();
    }
}

void registerMiscNatives(JNIEnv* env, jclass natives) {
    registerMiscNativesData(env, natives);
    registerMiscNativesBoard(env, natives);
    registerMiscNativesExtras(env, natives);
    registerMiscNativesMaps(env, natives);
}
} // namespace basalt::server::bukkit
