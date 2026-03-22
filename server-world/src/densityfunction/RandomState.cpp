#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/densityfunction/RandomState.hpp>
#include <basalt/server/world/noise/NormalNoise.hpp>

#include <stdexcept>

namespace basalt::server::world::densityfunction {
namespace {

[[nodiscard]] random::PositionalRandomFactory positionalFor(std::int64_t seed, bool legacy) {
    random::RandomSource source = legacy ? random::RandomSource::legacy(seed) : random::RandomSource::xoroshiro(seed);
    return source.forkPositional();
}

} // namespace

/// parity: the `CompileContext` `RandomState` hands its compiler.
class RandomState::Context final : public CompileContext {
public:
    explicit Context(RandomState& owner) : owner_(&owner) {}

    std::shared_ptr<const noise::Noise> createNoise(const std::string& noiseId) override {
        // parity: the Nether's two climate noises keep the old sequential seeding, off the raw seed.
        if (noiseId == "minecraft:nether/temperature" || noiseId == "minecraft:nether/vegetation") {
            const std::expected<std::shared_ptr<const noise::NormalNoise>, std::string> parameters =
                owner_->loader_->noise(noiseId);
            if (!parameters.has_value()) {
                throw std::runtime_error{parameters.error()};
            }
            random::RandomSource source =
                random::RandomSource::legacy(owner_->seed_ + (noiseId == "minecraft:nether/vegetation" ? 1 : 0));
            return (*parameters)->createForLegacyNetherBiome(source);
        }
        return owner_->noise(noiseId);
    }

    random::RandomSource createRandom(std::string_view seedName) override {
        if (owner_->legacy_ && seedName == "minecraft:terrain") {
            return random::RandomSource::legacy(owner_->seed_);
        }
        return owner_->random_.fromHashOf(seedName);
    }

    random::RandomSource createEndIslandRandom() override { return random::RandomSource::legacy(owner_->seed_); }

private:
    RandomState* owner_;
};

RandomState::RandomState(std::int64_t seed, bool legacyRandomSource, std::shared_ptr<DensityFunctionLoader> loader)
    : seed_(seed), legacy_(legacyRandomSource), random_(positionalFor(seed, legacyRandomSource)),
      loader_(std::move(loader)), compiler_(std::make_unique<DensityCompiler>(std::make_unique<Context>(*this))) {}

std::shared_ptr<const noise::Noise> RandomState::noise(std::string_view id) {
    const std::string key = id.find(':') == std::string_view::npos ? "minecraft:" + std::string{id} : std::string{id};
    const std::lock_guard<std::mutex> guard{noiseMutex_};
    if (const auto found = noises_.find(key); found != noises_.end()) {
        return found->second;
    }
    const std::expected<std::shared_ptr<const noise::NormalNoise>, std::string> parameters = loader_->noise(key);
    if (!parameters.has_value()) {
        throw std::runtime_error{parameters.error()};
    }
    random::RandomSource source = random_.fromHashOf(key);
    std::shared_ptr<const noise::Noise> made = (*parameters)->create(source);
    noises_.emplace(key, made);
    return made;
}

random::PositionalRandomFactory RandomState::randomFactory(std::string_view name) const {
    random::RandomSource source = random_.fromHashOf(name);
    return source.forkPositional();
}

} // namespace basalt::server::world::densityfunction
