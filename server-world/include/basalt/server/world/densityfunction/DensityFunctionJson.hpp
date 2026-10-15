#pragma once

#include <basalt/server/world/densityfunction/DensityFunction.hpp>

#include <expected>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>

namespace basalt::wire::common {
class JsonValue;
}

namespace basalt::server::world::densityfunction {

using WorldgenReader = std::function<std::optional<std::string>(std::string_view path)>;

/// parity: the `density_function` and `noise` codecs.
class DensityFunctionLoader {
public:
    explicit DensityFunctionLoader(WorldgenReader reader);

    [[nodiscard]] std::expected<Function, std::string> parse(const wire::common::JsonValue& json);

    [[nodiscard]] std::expected<Function, std::string> reference(std::string_view id);

    [[nodiscard]] std::expected<std::shared_ptr<const noise::NormalNoise>, std::string> noise(std::string_view id);

    [[nodiscard]] const WorldgenReader& reader() const noexcept { return reader_; }

private:
    [[nodiscard]] std::expected<Function, std::string> parseObject(const wire::common::JsonValue& json);
    [[nodiscard]] std::expected<std::shared_ptr<const Spline>, std::string>
    parseSpline(const wire::common::JsonValue& json);

    WorldgenReader reader_;
    std::recursive_mutex mutex_;
    std::map<std::string, Function, std::less<>> functions_;
    std::map<std::string, std::shared_ptr<const noise::NormalNoise>, std::less<>> noises_;
};

[[nodiscard]] std::expected<noise::NormalNoise, std::string> parseNoiseParameters(const wire::common::JsonValue& json);

[[nodiscard]] std::string resourcePath(std::string_view registry, std::string_view id);

} // namespace basalt::server::world::densityfunction
