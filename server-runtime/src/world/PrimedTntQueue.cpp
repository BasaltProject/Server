#include <basalt/server/runtime/world/PrimedTntQueue.hpp>

namespace basalt::server::runtime::world {

void PrimedTntQueue::primeTnt(std::int32_t x, std::int32_t y, std::int32_t z) {
    const WorldWriteLock lock{*mutex_};
    primedTnt_.push_back(std::array<std::int32_t, 3>{x, y, z});
}

std::vector<std::array<std::int32_t, 3>> PrimedTntQueue::takePrimedTnt() {
    const WorldWriteLock lock{*mutex_};
    std::vector<std::array<std::int32_t, 3>> lit;
    lit.swap(primedTnt_);
    return lit;
}

} // namespace basalt::server::runtime::world
