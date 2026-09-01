#pragma once

namespace basalt::server::entity::component {

/// A world-space position, in blocks. A prototype component, not tied to any real entity type yet.
struct Position {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    friend bool operator==(const Position&, const Position&) = default;
};

/// A per-tick velocity, in blocks per tick. A prototype component, as above.
struct Velocity {
    double dx = 0.0;
    double dy = 0.0;
    double dz = 0.0;

    friend bool operator==(const Velocity&, const Velocity&) = default;
};

} // namespace basalt::server::entity::component
