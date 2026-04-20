#pragma once

#include <basalt/wire/common/Identifier.hpp>

#include <cstdint>
#include <optional>
#include <variant>
#include <vector>

namespace basalt::server::registry {
// parity: `Consumable.onConsumeEffects`.
class ConsumeEffect {
public:
    struct Apply {
        wire::common::Identifier effect;
        std::int32_t durationTicks = 0;
        std::int32_t amplifier = 0;
        float probability = 1.0F;

        friend bool operator==(const Apply&, const Apply&) = default;
    };

    struct Remove {
        wire::common::Identifier effect;

        friend bool operator==(const Remove&, const Remove&) = default;
    };

    struct ClearAll {
        friend bool operator==(const ClearAll&, const ClearAll&) = default;
    };

    // parity: `TeleportRandomlyConsumeEffect`, the chorus fruit's jump.
    struct TeleportRandomly {
        float diameter = 16.0F;

        friend bool operator==(const TeleportRandomly&, const TeleportRandomly&) = default;
    };

    using Payload = std::variant<Apply, Remove, ClearAll, TeleportRandomly>;

    explicit ConsumeEffect(Payload payload) : payload_(std::move(payload)) {}

    template <typename T>
    [[nodiscard]] const T* as() const noexcept {
        return std::get_if<T>(&payload_);
    }

    [[nodiscard]] const Payload& payload() const noexcept { return payload_; }

    friend bool operator==(const ConsumeEffect&, const ConsumeEffect&) = default;

private:
    Payload payload_;
};

// parity: the `minecraft:food` and `minecraft:consumable`.
struct FoodValue {
    std::int32_t nutrition = 0;
    float saturation = 0.0F;
    bool canAlwaysEat = false;
    float consumeSeconds = 0.0F;
    std::vector<ConsumeEffect> effects;
    // parity: `minecraft:use_remainder`: the bowl, the bottle or the bucket left in hand.
    std::optional<wire::common::Identifier> remainder;

    // parity: `Consumable.getConsumeTicks`, which is the seconds at twenty ticks each.
    [[nodiscard]] std::int32_t consumeTicks() const noexcept {
        return static_cast<std::int32_t>(consumeSeconds * 20.0F);
    }
};

namespace itemFood {
[[nodiscard]] const FoodValue* of(const wire::common::Identifier& item);
} // namespace itemFood
} // namespace basalt::server::registry
