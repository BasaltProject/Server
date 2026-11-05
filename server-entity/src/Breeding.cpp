#include <basalt/server/entity/Breeding.hpp>

#include <algorithm>
#include <cmath>

namespace basalt::server::entity::breeding {

double offspringAttribute(double parentA, double parentB, double min, double max, double first, double second,
                          double third) {
    const double a = std::clamp(parentA, min, max);
    const double b = std::clamp(parentB, min, max);
    const double margin = 0.15 * (max - min);
    const double range = std::abs(a - b) + (margin * 2.0);
    const double average = (a + b) / 2.0;
    const double quality = ((first + second + third) / 3.0) - 0.5;
    const double value = average + (range * quality);
    if (value > max) {
        return max - (value - max);
    }
    if (value < min) {
        return min + (min - value);
    }
    return value;
}

} // namespace basalt::server::entity::breeding
