#include <basalt/server/runtime/world/RealNoiseParameters.hpp>

#include <utility>

namespace basalt::server::runtime::world {
namespace {

using server::world::noise::NormalNoise;

constexpr std::string_view kPrefix = "data/minecraft/worldgen/noise/";
constexpr std::string_view kJsonSuffix = ".json";
constexpr std::string_view kVanilla = "minecraft:";

} // namespace

const NormalNoise* RealNoiseParameters::get(std::string_view id) {
    {
        const std::lock_guard<std::mutex> guard(mutex_);
        if (const auto found = cache_.find(id); found != cache_.end()) {
            return &found->second;
        }
    }
    if (!readText_ || !parseParameters_) {
        return nullptr;
    }
    const std::optional<std::string> json =
        readText_(std::string{kPrefix} + std::string{id} + std::string{kJsonSuffix});
    if (!json.has_value()) {
        return nullptr;
    }
    const std::optional<NormalNoise::Parameters> parameters = parseParameters_(*json);
    if (!parameters.has_value()) {
        return nullptr;
    }
    server::random::RandomSource source = random_.fromHashOf(std::string{kVanilla} + std::string{id});
    NormalNoise noise = NormalNoise::create(source, parameters->firstOctave, parameters->amplitudes);
    const std::lock_guard<std::mutex> guard(mutex_);
    // Two threads racing to build the same noise derive it from the same seed, so either result is
    // equivalent and the loser's copy is simply discarded.
    return &cache_.try_emplace(std::string{id}, std::move(noise)).first->second;
}

} // namespace basalt::server::runtime::world
