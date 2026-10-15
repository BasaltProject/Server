#pragma once

namespace basalt::server::entity::component {
struct Position {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    friend bool operator==(const Position&, const Position&) = default;
};

struct Velocity {
    double dx = 0.0;
    double dy = 0.0;
    double dz = 0.0;

    friend bool operator==(const Velocity&, const Velocity&) = default;
};
} // namespace basalt::server::entity::component
