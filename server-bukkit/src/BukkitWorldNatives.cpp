#include "BukkitWorldNativesShared.hpp"

namespace basalt::server::bukkit {
void registerWorldNatives(JNIEnv* env, jclass natives) {
    registerWorldNativesRegistry(env, natives);
    registerWorldNativesBlocks(env, natives);
    registerWorldNativesEntities(env, natives);
    registerWorldNativesChunks(env, natives);
    registerWorldNativesJobs(env, natives);
}
} // namespace basalt::server::bukkit
