#include <basalt/server/entity/LevelValue.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace basalt::server::entity::levelValue {
namespace {
float next(std::span<const double> program, std::size_t& at, int level) {
    if (at >= program.size()) {
        return 0.0F;
    }
    const auto take = [&] { return at < program.size() ? static_cast<float>(program[at++]) : 0.0F; };
    switch (static_cast<Op>(static_cast<int>(program[at++]))) {
    case Op::Constant:
        return take();
    case Op::Linear: {
        const float base = take();
        const float perLevel = take();
        return base + (perLevel * static_cast<float>(level - 1));
    }
    case Op::LevelsSquared:
        return static_cast<float>(level * level) + take();
    case Op::Clamped: {
        const float low = take();
        const float high = take();
        const float value = next(program, at, level);
        return std::clamp(value, low, high);
    }
    case Op::Fraction: {
        const float top = next(program, at, level);
        const float bottom = next(program, at, level);
        return bottom == 0.0F ? 0.0F : top / bottom;
    }
    case Op::Exponent: {
        const float base = next(program, at, level);
        const float power = next(program, at, level);
        return static_cast<float>(std::pow(static_cast<double>(base), static_cast<double>(power)));
    }
    case Op::Lookup: {
        const auto count = static_cast<std::size_t>(take());
        const std::size_t first = at;
        at = std::min(at + count, program.size());
        const float fallback = next(program, at, level);
        return level >= 1 && static_cast<std::size_t>(level) <= count &&
                       first + static_cast<std::size_t>(level) <= program.size()
                   ? static_cast<float>(program[first + static_cast<std::size_t>(level) - 1])
                   : fallback;
    }
    }
    return 0.0F;
}
} // namespace

float evaluate(std::span<const double> program, int level) {
    std::size_t at = 0;
    return next(program, at, level);
}

} // namespace basalt::server::entity::levelValue
