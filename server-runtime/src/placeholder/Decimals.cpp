#include <basalt/server/runtime/placeholder/Decimals.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace basalt::server::runtime::placeholder::decimals {

std::optional<std::pair<std::string, std::int32_t>> split(std::string_view params) {
    const std::size_t separator = params.rfind('_');
    if (separator == std::string_view::npos || separator == 0 || separator == params.size() - 1) {
        return std::nullopt;
    }
    const std::string_view suffix = params.substr(separator + 1);
    if (suffix.size() > 1 || suffix[0] < '0' || suffix[0] > '9') {
        return std::nullopt;
    }
    return std::pair{std::string{params.substr(0, separator)}, static_cast<std::int32_t>(suffix[0] - '0')};
}

std::string format(double value, std::int32_t digits) {
    if (digits <= 0) {
        return std::to_string(static_cast<std::int64_t>(std::round(value)));
    }
    std::int64_t scale = 1;
    for (std::int32_t step = 0; step < std::min(digits, kMaxDigits); step++) {
        scale *= 10;
    }
    const auto scaled = static_cast<std::int64_t>(std::round(std::abs(value) * static_cast<double>(scale)));
    std::string fraction = std::to_string(scaled % scale);
    while (static_cast<std::int32_t>(fraction.size()) < digits) {
        fraction.insert(fraction.begin(), '0');
    }
    const std::string sign = value < 0 && scaled != 0 ? "-" : "";
    return sign + std::to_string(scaled / scale) + "." + fraction;
}

} // namespace basalt::server::runtime::placeholder::decimals
