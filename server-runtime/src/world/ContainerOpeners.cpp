#include <basalt/server/runtime/world/ContainerOpeners.hpp>
#include <basalt/wire/protocol/play/BlockPos.hpp>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <map>
#include <mutex>
#include <string>

namespace basalt::server::runtime::world::containerOpeners {
namespace {

[[nodiscard]] std::int64_t packed(std::int32_t x, std::int32_t y, std::int32_t z) {
    return wire::protocol::play::BlockPos{.x = x, .y = y, .z = z}.pack();
}

std::mutex& lock() {
    static std::mutex instance;
    return instance;
}

std::map<std::string, std::map<std::int64_t, std::int32_t>, std::less<>>& state() {
    static std::map<std::string, std::map<std::int64_t, std::int32_t>, std::less<>> instance;
    return instance;
}

std::atomic<bool>& anyOpen() {
    static std::atomic<bool> instance{false};
    return instance;
}

std::int32_t change(std::string_view world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t by) {
    const std::int64_t key = packed(x, y, z);
    const std::lock_guard<std::mutex> held{lock()};
    auto& all = state();
    const auto inWorld = all.find(world);
    std::map<std::int64_t, std::int32_t>& counts =
        inWorld != all.end() ? inWorld->second
                             : all.emplace(std::string{world}, std::map<std::int64_t, std::int32_t>{}).first->second;

    const auto at = counts.find(key);
    const std::int32_t next = std::max((at == counts.end() ? 0 : at->second) + by, 0);
    if (next == 0) {
        if (at != counts.end()) {
            counts.erase(at);
        }
    } else if (at != counts.end()) {
        at->second = next;
    } else {
        counts.emplace(key, next);
    }
    if (counts.empty()) {
        all.erase(std::string{world});
    }
    anyOpen().store(!all.empty());
    return next;
}

} // namespace

std::int32_t open(std::string_view world, std::int32_t x, std::int32_t y, std::int32_t z) {
    return change(world, x, y, z, 1);
}

std::int32_t close(std::string_view world, std::int32_t x, std::int32_t y, std::int32_t z) {
    return change(world, x, y, z, -1);
}

std::int32_t count(std::string_view world, std::int32_t x, std::int32_t y, std::int32_t z) {
    if (!anyOpen().load()) {
        return 0;
    }
    const std::lock_guard<std::mutex> held{lock()};
    const auto inWorld = state().find(world);
    if (inWorld == state().end()) {
        return 0;
    }
    const auto at = inWorld->second.find(packed(x, y, z));
    return at == inWorld->second.end() ? 0 : at->second;
}

void clear() {
    const std::lock_guard<std::mutex> held{lock()};
    state().clear();
    anyOpen().store(false);
}

} // namespace basalt::server::runtime::world::containerOpeners
