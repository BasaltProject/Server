#include "MobStateTable.hpp"

#include <basalt/server/runtime/entity/Boats.hpp>
#include <basalt/server/runtime/entity/Minecarts.hpp>

namespace basalt::server::runtime::plugins::entityOps::mobState {
namespace {

using entity::ServerEntity;
using namespace values;

template <typename T>
void cartMember(Table& table, std::string_view name, T entity::minecarts::CartState::* member, T fallback) {
    table[name] = Accessor{[member, fallback](const ServerEntity& entity) -> Value {
                               return toValue(entity.cart == nullptr ? fallback : entity.cart.get()->*member);
                           },
                           [member](ServerEntity& entity, const Value& value) {
                               if (!entity::minecarts::isMinecart(entity.type())) {
                                   return false;
                               }
                               fromValue(entity::minecarts::stateOf(entity).*member, value);
                               return true;
                           }};
}

void cartVector(Table& table, std::string_view name, std::array<double, 3> entity::minecarts::CartState::* member,
                std::size_t axis, double fallback) {
    table[name] = Accessor{[member, axis, fallback](const ServerEntity& entity) -> Value {
                               return entity.cart == nullptr ? fallback : (entity.cart.get()->*member)[axis];
                           },
                           [member, axis](ServerEntity& entity, const Value& value) {
                               if (!entity::minecarts::isMinecart(entity.type())) {
                                   return false;
                               }
                               (entity::minecarts::stateOf(entity).*member)[axis] = asReal(value);
                               return true;
                           }};
}

template <typename T>
void boatMember(Table& table, std::string_view name, T entity::boats::BoatState::* member, T fallback) {
    table[name] = Accessor{[member, fallback](const ServerEntity& entity) -> Value {
                               return toValue(entity.boat == nullptr ? fallback : entity.boat.get()->*member);
                           },
                           [member](ServerEntity& entity, const Value& value) {
                               if (!entity::boats::isBoat(entity.type())) {
                                   return false;
                               }
                               if (entity.boat == nullptr) {
                                   entity.boat = std::make_shared<entity::boats::BoatState>();
                               }
                               fromValue(entity.boat.get()->*member, value);
                               return true;
                           }};
}

} // namespace

void addVehicles(Table& table, Operations& /*operations*/) {
    cartMember(table, "cart_slow_when_empty", &entity::minecarts::CartState::slowWhenEmpty, true);
    cartMember(table, "cart_friction", &entity::minecarts::CartState::frictionState, std::int8_t{-1});
    cartVector(table, "cart_flying_x", &entity::minecarts::CartState::flyingVelocityMod, 0, 0.95F);
    cartVector(table, "cart_flying_y", &entity::minecarts::CartState::flyingVelocityMod, 1, 0.95F);
    cartVector(table, "cart_flying_z", &entity::minecarts::CartState::flyingVelocityMod, 2, 0.95F);
    cartVector(table, "cart_derailed_x", &entity::minecarts::CartState::derailedVelocityMod, 0, 0.5);
    cartVector(table, "cart_derailed_y", &entity::minecarts::CartState::derailedVelocityMod, 1, 0.5);
    cartVector(table, "cart_derailed_z", &entity::minecarts::CartState::derailedVelocityMod, 2, 0.5);
    table["cart_max_speed"] = Accessor{[](const ServerEntity& entity) -> Value {
                                           return entity.cart != nullptr && entity.cart->maxSpeedOverride.has_value()
                                                      ? *entity.cart->maxSpeedOverride
                                                      : entity::minecarts::kMaxSpeed;
                                       },
                                       [](ServerEntity& entity, const Value& value) {
                                           if (!entity::minecarts::isMinecart(entity.type())) {
                                               return false;
                                           }
                                           entity::minecarts::stateOf(entity).maxSpeedOverride = asReal(value);
                                           return true;
                                       }};
    cartMember(table, "cart_fuel", &entity::minecarts::CartState::fuel, std::int32_t{0});
    cartMember(table, "cart_push_x", &entity::minecarts::CartState::pushX, 0.0);
    cartMember(table, "cart_push_z", &entity::minecarts::CartState::pushZ, 0.0);
    cartMember(table, "cart_explosion_speed", &entity::minecarts::CartState::explosionSpeedFactor,
               entity::minecarts::kTntCartSpeedFactor);
    table["vehicle_damage"] =
        Accessor{[](const ServerEntity& entity) -> Value { return static_cast<double>(entity.vehicleDamage); },
                 [](ServerEntity& entity, const Value& value) {
                     entity.vehicleDamage = static_cast<float>(asReal(value));
                     return true;
                 }};
    boatMember(table, "boat_max_speed", &entity::boats::BoatState::maxSpeed, 0.4);
    boatMember(table, "boat_occupied_deceleration", &entity::boats::BoatState::occupiedDeceleration, 0.2);
    boatMember(table, "boat_unoccupied_deceleration", &entity::boats::BoatState::unoccupiedDeceleration, -1.0);
    boatMember(table, "boat_work_on_land", &entity::boats::BoatState::workOnLand, false);
    table["boat_status"] =
        Accessor{[](const ServerEntity& entity) -> Value {
                     return static_cast<std::int64_t>(entity.boat == nullptr ? entity::boats::Status::InAir
                                                                             : entity.boat->status);
                 },
                 nullptr};
}

} // namespace basalt::server::runtime::plugins::entityOps::mobState
