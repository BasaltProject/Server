#pragma once

#include <basalt/server/random/PositionalRandomFactory.hpp>
#include <basalt/server/world/densityfunction/DensityFunctionJson.hpp>
#include <basalt/server/world/densityfunction/SamplerContext.hpp>

#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>

namespace basalt::server::world::noise {
class Noise;
}

namespace basalt::server::world::densityfunction {

/// parity: `RandomState`.
class RandomState {
public:
    RandomState(std::int64_t seed, bool legacyRandomSource, std::shared_ptr<DensityFunctionLoader> loader);

    [[nodiscard]] std::int64_t seed() const noexcept { return seed_; }

    /// parity: `RandomState.getOrCreateNoise`. Throws when the jar lacks the noise.
    [[nodiscard]] std::shared_ptr<const noise::Noise> noise(std::string_view id);

    /// parity: `RandomState.getOrCreateRandomFactory`.
    [[nodiscard]] random::PositionalRandomFactory randomFactory(std::string_view name) const;

    [[nodiscard]] const random::PositionalRandomFactory& random() const noexcept { return random_; }

    [[nodiscard]] Sampler sampler(const Function& function) { return compiler_->sampler(function); }

    [[nodiscard]] DensityFunctionLoader& loader() noexcept { return *loader_; }

private:
    class Context;

    std::int64_t seed_;
    bool legacy_;
    random::PositionalRandomFactory random_;
    std::shared_ptr<DensityFunctionLoader> loader_;
    std::mutex noiseMutex_;
    std::map<std::string, std::shared_ptr<const noise::Noise>, std::less<>> noises_;
    std::unique_ptr<DensityCompiler> compiler_;
};

} // namespace basalt::server::world::densityfunction
