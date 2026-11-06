#pragma once

#include <basalt/server/plugins/PluginWorldgen.hpp>

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace basalt::server::world::terrain {
struct NoiseRouter;
}

namespace basalt::server::world::noise {
class Noise;
}

namespace basalt::server::runtime::plugins {

class ServerWorldgen final : public server::plugins::PluginWorldgen {
public:
    struct Hooks {
        std::function<std::shared_ptr<const server::world::terrain::NoiseRouter>(std::string_view settings,
                                                                                 std::int64_t seed)>
            router;
    };

    explicit ServerWorldgen(Hooks hooks) : hooks_(std::move(hooks)) {}

    [[nodiscard]] std::optional<server::plugins::PluginTerrainShape> shape(std::string_view settings) override;
    [[nodiscard]] std::optional<double> density(std::string_view settings, std::int64_t seed, std::string_view function,
                                                std::int32_t x, std::int32_t y, std::int32_t z) override;
    [[nodiscard]] std::optional<double> noise(std::string_view settings, std::int64_t seed, std::string_view id,
                                              double x, double y, double z) override;
    [[nodiscard]] std::optional<std::int64_t> createNoise(std::string_view id,
                                                          const server::plugins::PluginNoiseSeed& seed) override;
    [[nodiscard]] std::optional<std::int64_t> createNoise(const server::plugins::PluginNoiseParameters& parameters,
                                                          const server::plugins::PluginNoiseSeed& seed) override;
    [[nodiscard]] std::optional<double> sampleNoise(std::int64_t handle, double x, double y, double z) override;
    [[nodiscard]] std::optional<std::pair<double, double>> handleRange(std::int64_t handle) override;
    void releaseNoise(std::int64_t handle) override;
    [[nodiscard]] std::optional<std::pair<double, double>> noiseRange(std::string_view settings, std::int64_t seed,
                                                                      std::string_view id) override;
    [[nodiscard]] std::optional<std::int32_t> baseHeight(std::string_view settings, std::int64_t seed, std::int32_t x,
                                                         std::int32_t z, bool oceanFloor) override;
    [[nodiscard]] std::vector<std::int32_t> baseColumn(std::string_view settings, std::int64_t seed, std::int32_t x,
                                                       std::int32_t z) override;

private:
    [[nodiscard]] std::shared_ptr<const server::world::terrain::NoiseRouter> router(std::string_view settings,
                                                                                    std::int64_t seed) const;

    [[nodiscard]] std::int64_t keep(std::shared_ptr<const server::world::noise::Noise> noise);
    [[nodiscard]] std::shared_ptr<const server::world::noise::Noise> find(std::int64_t handle) const;

    Hooks hooks_;
    mutable std::mutex noisesMutex_;
    std::unordered_map<std::int64_t, std::shared_ptr<const server::world::noise::Noise>> noises_;
    std::atomic<std::int64_t> nextHandle_{1};
};

} // namespace basalt::server::runtime::plugins
