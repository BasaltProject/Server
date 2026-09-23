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

/// Reads the worldgen files a density function names, from wherever the server keeps the jar's
/// data. `path` is relative to `data/<namespace>/`, without the extension.
using WorldgenReader = std::function<std::optional<std::string>(std::string_view path)>;

/// parity: the `density_function` and `noise` codecs. Parses density functions from JSON and
/// resolves the references between files, each file parsed once so a reference is one node.
///
/// Every failure names the file and the field: a world must not generate from half a router.
class DensityFunctionLoader {
public:
    explicit DensityFunctionLoader(WorldgenReader reader);

    [[nodiscard]] std::expected<Function, std::string> parse(const wire::common::JsonValue& json);

    /// A `worldgen/density_function` entry by id, as a reference node.
    [[nodiscard]] std::expected<Function, std::string> reference(std::string_view id);

    /// A `worldgen/noise` entry by id.
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

/// Parses the fields of a `worldgen/noise` file.
[[nodiscard]] std::expected<noise::NormalNoise, std::string> parseNoiseParameters(const wire::common::JsonValue& json);

/// `minecraft:foo/bar` as `foo/bar` under its namespace, the path the reader takes.
[[nodiscard]] std::string resourcePath(std::string_view registry, std::string_view id);

} // namespace basalt::server::world::densityfunction
