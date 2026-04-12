#pragma once

#include <basalt/wire/common/Identifier.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::world {
class WorldState;
}

namespace basalt::server::runtime::entity {

class ServerEntity;

/// Boats and rafts, with and without a chest. parity: `AbstractBoat` and its four kinds.
namespace boats {

/// parity: `AbstractBoat.DATA_ID_PADDLE_LEFT`, `PADDLE_RIGHT` and `BUBBLE_TIME`, after
/// `VehicleEntity`'s three.
inline constexpr std::uint8_t kPaddleLeftDataIndex = 11;
inline constexpr std::uint8_t kPaddleRightDataIndex = 12;

/// parity: the `outOfControlTicks` past which a sunk boat stops taking riders and throws them off.
inline constexpr float kOutOfControlTicks = 60.0F;

[[nodiscard]] bool isBoat(const wire::common::Identifier& type);

/// parity: `AbstractChestBoat`.
[[nodiscard]] bool hasChest(const wire::common::Identifier& type);

/// parity: `getMaxPassengers`: two, or one beside a chest.
[[nodiscard]] std::int32_t maxPassengers(const wire::common::Identifier& type);

/// parity: `AbstractBoat.Status`, in its declared order.
enum class Status : std::uint8_t { InWater, UnderWater, UnderFlowingWater, OnLand, InAir };

/// What a boat keeps between turns. parity: `AbstractBoat`'s own fields.
struct BoatState {
    Status status = Status::InAir;
    Status oldStatus = Status::InAir;
    double waterLevel = 0.0;
    float landFriction = 0.0F;
    double lastYd = 0.0;
    /// parity: `paddlePositions`, how far round each oar has turned.
    std::array<float, 2> paddlePositions{};
};

/// parity: `rideHeight`: a third of the boat, or eight ninths of a raft.
[[nodiscard]] double rideHeight(const ServerEntity& boat);

/// What one turn of a boat left for its caller to do.
struct TickResult {
    /// parity: sunk for three seconds: everybody aboard goes.
    bool ejectAll = false;
    /// Mobs the boat scooped up, already seated in it.
    std::vector<ServerEntity*> boarded;

    /// parity: the oar strokes that splash this turn, as the sound and where it is heard.
    struct Stroke {
        std::string_view sound;
        double x = 0.0;
        double y = 0.0;
        double z = 0.0;
    };

    std::vector<Stroke> strokes;
};

/// Whether an entity type is in `cannot_be_pushed_onto_boats`.
using CannotBoard = std::function<bool(const wire::common::Identifier& type)>;

/// parity: `AbstractBoat.tick` on the server: the status, the out-of-control clock, the float and the
/// move when no player drives it, and the mobs it picks up as it bumps into them.
[[nodiscard]] TickResult tick(world::WorldState& world, ServerEntity& boat, bool drivenByPlayer,
                              const std::vector<ServerEntity*>& living, const CannotBoard& cannotBoard);

} // namespace boats

} // namespace basalt::server::runtime::entity
