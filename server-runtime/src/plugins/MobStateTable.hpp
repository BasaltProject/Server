#pragma once

#include "EntityOpsValues.hpp"

#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/plugins/EntityOps.hpp>

#include <functional>
#include <map>
#include <memory>
#include <string_view>
#include <type_traits>

namespace basalt::server::runtime::plugins::entityOps::mobState {

using Value = values::Value;
using Arguments = values::Arguments;

struct Accessor {
    std::function<Value(const entity::ServerEntity&)> read;
    std::function<bool(entity::ServerEntity&, const Value&)> write;
};

using Table = std::map<std::string_view, Accessor, std::less<>>;
using Operation = std::function<std::optional<Value>(const Environment&, entity::ServerEntity&, const Arguments&)>;
using Operations = std::map<std::string_view, Operation, std::less<>>;

template <typename T>
[[nodiscard]] Value toValue(const T& stored) {
    if constexpr (std::is_same_v<T, bool>) {
        return stored;
    } else if constexpr (std::is_enum_v<T>) {
        return static_cast<std::int64_t>(stored);
    } else if constexpr (std::is_integral_v<T>) {
        return static_cast<std::int64_t>(stored);
    } else {
        return static_cast<double>(stored);
    }
}

template <typename T>
void fromValue(T& stored, const Value& value) {
    if constexpr (std::is_same_v<T, bool>) {
        stored = values::asBool(value);
    } else if constexpr (std::is_enum_v<T>) {
        stored = static_cast<T>(values::asInt(value));
    } else if constexpr (std::is_integral_v<T>) {
        stored = static_cast<T>(values::asInt(value));
    } else {
        stored = static_cast<T>(values::asReal(value));
    }
}

using Ensure = void (*)(entity::ServerEntity&);

/// A plain member of a state struct hanging off the entity; `ensure` creates the state on first use.
template <typename State, typename T>
[[nodiscard]] Accessor viaState(std::shared_ptr<State> entity::ServerEntity::* holder, T State::* member,
                                bool writable = true, Ensure ensure = nullptr) {
    Accessor accessor;
    accessor.read = [holder, member](const entity::ServerEntity& entity) -> Value {
        const auto& state = entity.*holder;
        return state == nullptr ? Value{} : toValue(state.get()->*member);
    };
    if (writable) {
        accessor.write = [holder, member, ensure](entity::ServerEntity& entity, const Value& value) {
            if (entity.*holder == nullptr && ensure != nullptr) {
                ensure(entity);
            }
            auto& state = entity.*holder;
            if (state == nullptr) {
                return false;
            }
            fromValue(state.get()->*member, value);
            return true;
        };
    }
    return accessor;
}

void addFishingHook(Table& table, Operations& operations);

void addMobCommon(Table& table, Operations& operations);

void addVehicles(Table& table, Operations& operations);

void addVillager(Table& table, Operations& operations);

void addBeasts(Table& table, Operations& operations);

void addFlagOps(Table& table, Operations& operations);

void addMobMore(Table& table, Operations& operations);

} // namespace basalt::server::runtime::plugins::entityOps::mobState
