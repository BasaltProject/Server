#include <basalt/server/runtime/world/RegionStore.hpp>

#include <cstdint>
#include <cstdio>
#include <ctime>
#include <string>
#include <vector>

namespace {

double cpuSeconds() {
    timespec now{};
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &now);
    return static_cast<double>(now.tv_sec) + static_cast<double>(now.tv_nsec) / 1e9;
}

} // namespace

int main(int argc, char** argv) {
    using basalt::server::runtime::config::RegionFormatSetting;
    const std::string root = argc > 1 ? argv[1] : "worlds";
    basalt::server::runtime::world::RegionStore store{"world",
                                                      root,
                                                      basalt::server::world::kOverworldHeight,
                                                      12,
                                                      basalt::server::runtime::world::RegionStore::kByteBudget,
                                                      RegionFormatSetting::Bsw};
    std::vector<std::int32_t> chunks;
    for (std::int32_t regionX = -2; regionX <= 1; ++regionX) {
        for (std::int32_t regionZ = -2; regionZ <= 1; ++regionZ) {
            const std::vector<std::int32_t> found = store.storedChunks(regionX, regionZ);
            chunks.insert(chunks.end(), found.begin(), found.end());
        }
    }
    for (int pass = 0; pass < 3; ++pass) {
        const double before = cpuSeconds();
        std::size_t loaded = 0;
        for (std::size_t i = 0; i + 1 < chunks.size(); i += 2) {
            loaded += store.loadChunk(chunks[i], chunks[i + 1]).has_value() ? 1 : 0;
        }
        const double used = cpuSeconds() - before;
        std::printf("pass %d: %zu chunks, %.1f us cpu per chunk\n", pass, loaded,
                    used * 1e6 / static_cast<double>(loaded));
    }
}
