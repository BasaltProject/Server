#include <basalt/server/runtime/world/PrimedTntQueue.hpp>

#include <cstdint>

namespace basalt::server::runtime::world {

void PrimedTntQueue::primeTnt(std::int32_t x, std::int32_t y, std::int32_t z, std::string_view event) {
    const WorldWriteLock lock{*mutex_};
    primedTnt_.push_back(Primed{.x = x, .y = y, .z = z, .event = std::string{event}});
}

std::vector<PrimedTntQueue::Primed> PrimedTntQueue::takePrimedTnt() {
    const WorldWriteLock lock{*mutex_};
    std::vector<Primed> lit;
    lit.swap(primedTnt_);
    return lit;
}

} // namespace basalt::server::runtime::world
