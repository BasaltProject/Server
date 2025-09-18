#include <basalt/server/world/terrain/decoration/IntProvider.hpp>

#include <algorithm>
#include <stdexcept>

namespace basalt::server::world::terrain::decoration {

IntProvider IntProvider::of(std::int32_t value) {
    IntProvider provider;
    provider.kind_ = Kind::Constant;
    provider.first_ = value;
    return provider;
}

IntProvider IntProvider::uniform(std::int32_t min, std::int32_t max) {
    IntProvider provider;
    provider.kind_ = Kind::Uniform;
    provider.first_ = min;
    provider.second_ = max;
    return provider;
}

IntProvider IntProvider::clamped(IntProvider source, std::int32_t min, std::int32_t max) {
    IntProvider provider;
    provider.kind_ = Kind::Clamped;
    provider.first_ = min;
    provider.second_ = max;
    provider.source_ = std::make_shared<const IntProvider>(std::move(source));
    return provider;
}

IntProvider IntProvider::weighted(std::initializer_list<std::pair<std::int32_t, std::int32_t>> entries) {
    if (entries.size() == 0) {
        throw std::invalid_argument{"a weighted int provider needs at least one entry"};
    }
    IntProvider provider;
    provider.kind_ = Kind::Weighted;
    provider.entries_ = std::make_shared<const std::vector<std::pair<std::int32_t, std::int32_t>>>(entries);
    for (const auto& [value, weight] : *provider.entries_) {
        provider.totalWeight_ += weight;
    }
    return provider;
}

std::int32_t IntProvider::sample(random::RandomSource& random) const {
    switch (kind_) {
    case Kind::Constant:
        return first_;
    case Kind::Uniform:
        return first_ + random.nextInt(second_ - first_ + 1);
    case Kind::Clamped:
        return std::clamp(source_->sample(random), first_, second_);
    case Kind::Weighted:
        break;
    }
    std::int32_t roll = random.nextInt(totalWeight_);
    for (const auto& [value, weight] : *entries_) {
        if (roll < weight) {
            return value;
        }
        roll -= weight;
    }
    return entries_->back().first;
}

} // namespace basalt::server::world::terrain::decoration
