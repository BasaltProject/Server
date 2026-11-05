#pragma once

namespace basalt::server::entity::breeding {

/// parity: `AbstractHorse.createOffspringAttribute`, over the three random draws it takes in order.
[[nodiscard]] double offspringAttribute(double parentA, double parentB, double min, double max, double first,
                                        double second, double third);

} // namespace basalt::server::entity::breeding
