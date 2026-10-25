#pragma once

#include "JniSupport.hpp"

#include <basalt/server/plugins/PluginWorldEnv.hpp>

namespace basalt::server::bukkit {
[[nodiscard]] inline std::shared_ptr<plugins::PluginWorldRef> envWorldOf(JNIEnv* env, jstring name) {
    return BukkitBridge::current().services().worlds().byName(toStd(env, name));
}

template <typename Each>
inline std::string joinWith(const char separator, const Each& each, const std::size_t count) {
    std::string out;
    for (std::size_t at = 0; at < count; ++at) {
        out += (at == 0 ? "" : std::string(1, separator)) + each(at);
    }
    return out;
}
} // namespace basalt::server::bukkit
