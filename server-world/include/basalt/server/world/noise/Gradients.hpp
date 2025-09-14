#pragma once

#include <array>
#include <cstdint>

namespace basalt::server::world::noise {

/// parity: SimplexNoise.GRADIENT, the sixteen gradient vectors both noises share. Flattened,
/// because eight gradient dot products run per octave per sample and a row indirection plus three
/// int-to-double conversions used to be most of the work.
inline constexpr std::array<double, 16 * 3> kGradients = {1,  1, 0,  -1, 1,  0,  1, -1, 0,  -1, -1, 0, 1,  0, 1,  -1,
                                                          0,  1, 1,  0,  -1, -1, 0, -1, 0,  1,  1,  0, -1, 1, 0,  1,
                                                          -1, 0, -1, -1, 1,  1,  0, 0,  -1, 1,  -1, 1, 0,  0, -1, -1};

inline constexpr std::array<std::array<std::int32_t, 3>, 16> kGradientRows = {{{1, 1, 0},
                                                                               {-1, 1, 0},
                                                                               {1, -1, 0},
                                                                               {-1, -1, 0},
                                                                               {1, 0, 1},
                                                                               {-1, 0, 1},
                                                                               {1, 0, -1},
                                                                               {-1, 0, -1},
                                                                               {0, 1, 1},
                                                                               {0, -1, 1},
                                                                               {0, 1, -1},
                                                                               {0, -1, -1},
                                                                               {1, 1, 0},
                                                                               {0, -1, 1},
                                                                               {-1, 1, 0},
                                                                               {0, -1, -1}}};

} // namespace basalt::server::world::noise
