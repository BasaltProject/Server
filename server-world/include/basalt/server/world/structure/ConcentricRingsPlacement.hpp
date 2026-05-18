#pragma once

#include <cstdint>
#include <functional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace basalt::server::world::structure {

/// parity: `ConcentricRingsStructurePlacement` plus `generateRingPositions`.
class ConcentricRingsPlacement {
public:
    using NoiseBiomeAt = std::function<std::string_view(std::int32_t, std::int32_t, std::int32_t)>;

    ConcentricRingsPlacement(std::int32_t distance, std::int32_t spread, std::int32_t count,
                             std::set<std::string, std::less<>> preferredBiomes, NoiseBiomeAt noiseBiomeAt,
                             bool fixedBiomeSource = false)
        : distance_(distance), spread_(spread), count_(count), preferredBiomes_(std::move(preferredBiomes)),
          noiseBiomeAt_(std::move(noiseBiomeAt)), fixedBiomeSource_(fixedBiomeSource) {}

    [[nodiscard]] std::vector<std::pair<std::int32_t, std::int32_t>> ringPositions(std::int64_t seed) const;

private:
    std::int32_t distance_;
    std::int32_t spread_;
    std::int32_t count_;
    std::set<std::string, std::less<>> preferredBiomes_;
    NoiseBiomeAt noiseBiomeAt_;
    bool fixedBiomeSource_;
};

} // namespace basalt::server::world::structure
