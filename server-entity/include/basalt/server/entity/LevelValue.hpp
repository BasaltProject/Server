#pragma once

#include <span>

namespace basalt::server::entity::levelValue {

enum class Op : int {
    Constant = 0,
    Linear = 1,
    LevelsSquared = 2,
    Clamped = 3,
    Fraction = 4,
    Exponent = 5,
    Lookup = 6
};

/// parity: `LevelBasedValue.calculate`; the program is the tree in prefix order (Lookup: count, values, fallback).
[[nodiscard]] float evaluate(std::span<const double> program, int level);

} // namespace basalt::server::entity::levelValue
