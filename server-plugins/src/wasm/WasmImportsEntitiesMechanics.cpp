#include "WasmValues.hpp"

#include <basalt/server/entity/Breeding.hpp>
#include <basalt/server/entity/Collision.hpp>
#include <basalt/server/entity/Combat.hpp>
#include <basalt/server/entity/Geometry.hpp>
#include <basalt/server/entity/Leash.hpp>
#include <basalt/server/entity/Projectiles.hpp>

#include <expected>
#include <functional>
#include <map>

namespace basalt::server::plugins::wasm {
namespace {
namespace geometry = entity::geometry;
namespace projectile = entity::projectile;
using wire::wasm::Value;

constexpr std::int32_t kBad = abi::kErrInvalid;

/// A failed read is one INVALID; a null answer is a null value.
struct Reader {
    const Value& map;
    bool ok = true;

    [[nodiscard]] double number(std::string_view key, std::optional<double> fallback = std::nullopt) {
        return numberOf(map.find(key), fallback);
    }

    [[nodiscard]] double numberOf(const Value* one, std::optional<double> fallback = std::nullopt) {
        if (one != nullptr && one->asFloat() != nullptr) {
            return *one->asFloat();
        }
        if (one != nullptr && one->asInt() != nullptr) {
            return static_cast<double>(*one->asInt());
        }
        if (!fallback.has_value()) {
            ok = false;
        }
        return fallback.value_or(0.0);
    }

    [[nodiscard]] bool flag(std::string_view key, bool fallback = false) const {
        const Value* one = map.find(key);
        return one != nullptr && one->asBool() != nullptr ? *one->asBool() : fallback;
    }

    [[nodiscard]] std::string text(std::string_view key) {
        const Value* one = map.find(key);
        if (one == nullptr || one->asString() == nullptr) {
            ok = false;
            return {};
        }
        return *one->asString();
    }

    [[nodiscard]] geometry::Point point(const Value* value) {
        geometry::Point at{};
        const Value::List* list = value == nullptr ? nullptr : value->asList();
        if (list == nullptr || list->size() != 3) {
            ok = false;
            return at;
        }
        for (std::size_t index = 0; index < 3; ++index) {
            at[index] = numberOf(&(*list)[index]);
        }
        return at;
    }

    [[nodiscard]] geometry::Point point(std::string_view key) { return point(map.find(key)); }

    [[nodiscard]] geometry::Box box(const Value* value) {
        if (value == nullptr || value->asMap() == nullptr) {
            ok = false;
            return {};
        }
        Reader inner{*value};
        const geometry::Box made =
            geometry::makeBox(inner.number("min_x"), inner.number("min_y"), inner.number("min_z"),
                              inner.number("max_x"), inner.number("max_y"), inner.number("max_z"));
        ok = ok && inner.ok;
        return made;
    }

    [[nodiscard]] geometry::Box box(std::string_view key) { return box(map.find(key)); }

    [[nodiscard]] std::vector<projectile::Candidate> candidates(std::string_view key) {
        std::vector<projectile::Candidate> out;
        const Value* one = map.find(key);
        if (one == nullptr || one->asList() == nullptr) {
            ok = false;
            return out;
        }
        for (const Value& entry : *one->asList()) {
            Reader inner{entry};
            projectile::Candidate candidate;
            candidate.box = inner.box(&entry);
            candidate.pickRadius = static_cast<float>(inner.number("pick_radius", 0.0));
            candidate.pickableFromInside = inner.flag("pick_from_inside");
            candidate.rootVehicle = static_cast<std::int32_t>(inner.number("root", 0.0));
            ok = ok && inner.ok;
            out.push_back(candidate);
        }
        return out;
    }

    [[nodiscard]] std::vector<entity::collision::Collider> colliders(std::string_view key) {
        std::vector<entity::collision::Collider> out;
        const Value* one = map.find(key);
        if (one == nullptr || one->asList() == nullptr) {
            ok = false;
            return out;
        }
        for (const Value& entry : *one->asList()) {
            Reader inner{entry};
            const geometry::Box made = inner.box(&entry);
            out.push_back({made, static_cast<std::int32_t>(inner.number("shape", 0.0))});
            ok = ok && inner.ok;
        }
        return out;
    }

    [[nodiscard]] entity::leash::Body body(std::string_view key) {
        const Value* one = map.find(key);
        entity::leash::Body made;
        if (one == nullptr || one->asMap() == nullptr) {
            ok = false;
            return made;
        }
        Reader inner{*one};
        made.position = inner.point("position");
        made.width = static_cast<float>(inner.number("width"));
        made.height = static_cast<float>(inner.number("height"));
        made.yaw = static_cast<float>(inner.number("yaw"));
        ok = ok && inner.ok;
        return made;
    }
};

[[nodiscard]] Value real(double one) { return Value{one}; }

[[nodiscard]] Value real(float one) { return Value{static_cast<double>(one)}; }

[[nodiscard]] Value pointOf(const geometry::Point& at) {
    return Value{Value::List{Value{at[0]}, Value{at[1]}, Value{at[2]}}};
}

[[nodiscard]] Value boxOf(const geometry::Box& box) {
    return Value{Value::Map{{"min_x", real(box.minX)},
                            {"min_y", real(box.minY)},
                            {"min_z", real(box.minZ)},
                            {"max_x", real(box.maxX)},
                            {"max_y", real(box.maxY)},
                            {"max_z", real(box.maxZ)}}};
}

[[nodiscard]] Value hitOf(const projectile::Hit& hit) {
    if (hit.index < 0) {
        return Value{};
    }
    return Value{Value::Map{{"index", Value{static_cast<std::int64_t>(hit.index) + 1}}, {"point", pointOf(hit.at)}}};
}

using Mechanic = std::function<Value(Reader&)>;

const std::map<std::string_view, Mechanic, std::less<>>& mechanics() {
    namespace combat = entity::combat;
    static const std::map<std::string_view, Mechanic, std::less<>> kAll = {
        {"combat.after_armor",
         [](Reader& in) {
             return real(combat::afterArmour(
                 static_cast<float>(in.number("damage")), static_cast<float>(in.number("armor")),
                 static_cast<float>(in.number("toughness")), static_cast<float>(in.number("fraction_add", 0.0))));
         }},
        {"combat.after_protection",
         [](Reader& in) {
             return real(combat::afterProtection(static_cast<float>(in.number("damage")),
                                                 static_cast<float>(in.number("protection"))));
         }},
        {"combat.after_resistance",
         [](Reader& in) {
             return real(combat::afterResistance(static_cast<float>(in.number("damage")),
                                                 static_cast<std::int32_t>(in.number("amplifier"))));
         }},
        {"combat.after_difficulty",
         [](Reader& in) {
             const float damage = static_cast<float>(in.number("damage"));
             return real(combat::afterDifficulty(damage, in.text("difficulty")));
         }},
        {"combat.scales_with_difficulty",
         [](Reader& in) {
             const std::string scaling = in.text("scaling");
             return Value{combat::scalesWithDifficulty(scaling, in.flag("caused_by_living_non_player"))};
         }},
        {"combat.absorb",
         [](Reader& in) {
             const combat::Absorbed split =
                 combat::absorb(static_cast<float>(in.number("damage")), static_cast<float>(in.number("absorption")));
             return Value{Value::Map{{"health", real(split.health)}, {"absorption", real(split.absorption)}}};
         }},
        {"combat.fall_damage",
         [](Reader& in) {
             const double distance = in.number("distance");
             return Value{
                 static_cast<std::int64_t>(combat::fallDamage(distance, in.number("safe", combat::kSafeFallDistance)))};
         }},
        {"breeding.offspring_attribute",
         [](Reader& in) {
             return real(entity::breeding::offspringAttribute(in.number("a"), in.number("b"), in.number("min"),
                                                              in.number("max"), in.number("first"), in.number("second"),
                                                              in.number("third")));
         }},
        {"physics.clip",
         [](Reader& in) {
             const geometry::Box box = in.box("box");
             const geometry::Point from = in.point("from");
             const std::optional<geometry::Point> at = geometry::clip(box, from, in.point("to"));
             return at.has_value() ? pointOf(*at) : Value{};
         }},
        {"physics.collide",
         [](Reader& in) {
             const auto colliders = in.colliders("colliders");
             const geometry::Box box = in.box("box");
             return pointOf(entity::collision::sweep(colliders, box, in.point("movement")));
         }},
        {"physics.step_up",
         [](Reader& in) {
             const auto colliders = in.colliders("colliders");
             const geometry::Box grounded = in.box("grounded");
             const double offset = in.number("grounded_offset_y");
             const geometry::Point movement = in.point("movement");
             const auto up = static_cast<float>(in.number("max_up_step"));
             const std::optional<geometry::Point> stepped =
                 entity::collision::stepUp(colliders, grounded, offset, movement, up, in.number("reach_squared"));
             return stepped.has_value() ? pointOf(*stepped) : Value{};
         }},
        {"physics.rotate_towards_movement",
         [](Reader& in) {
             const geometry::Point motion = in.point("motion");
             const std::optional<projectile::Rotation> turned = projectile::rotateTowardsMovement(
                 motion[0], motion[1], motion[2], static_cast<float>(in.number("x_rot_o")),
                 static_cast<float>(in.number("y_rot_o")), static_cast<float>(in.number("x_rot")),
                 static_cast<float>(in.number("y_rot")), static_cast<float>(in.number("speed")));
             if (!turned.has_value()) {
                 return Value{};
             }
             return Value{Value::Map{{"x_rot_o", real(turned->xRotO)},
                                     {"y_rot_o", real(turned->yRotO)},
                                     {"x_rot", real(turned->xRot)},
                                     {"y_rot", real(turned->yRot)}}};
         }},
        {"physics.closest_hit",
         [](Reader& in) {
             const auto candidates = in.candidates("candidates");
             const geometry::Point from = in.point("from");
             const geometry::Point to = in.point("to");
             return hitOf(projectile::closestHit(candidates, from, to, static_cast<float>(in.number("margin"))));
         }},
        {"physics.picked_hit",
         [](Reader& in) {
             const auto candidates = in.candidates("candidates");
             const auto root = static_cast<std::int32_t>(in.number("root"));
             const geometry::Point from = in.point("from");
             const geometry::Point to = in.point("to");
             return hitOf(projectile::pickedHit(candidates, root, from, to, in.number("max")));
         }},
        {"physics.projectile_margin",
         [](Reader& in) { return real(projectile::margin(static_cast<std::int32_t>(in.number("tick_count")))); }},
        {"physics.shulker_box",
         [](Reader& in) {
             const auto size = static_cast<float>(in.number("size"));
             const auto direction = static_cast<std::int32_t>(in.number("direction"));
             const auto from = static_cast<float>(in.number("progress_from"));
             const auto to = static_cast<float>(in.number("progress_to"));
             return boxOf(projectile::shulkerProgressBox(size, direction, from, to, in.point("position")));
         }},
        {"physics.leash_angular_friction",
         [](Reader& in) {
             return real(entity::leash::angularFriction(
                 in.flag("on_ground"), static_cast<float>(in.number("block_friction", 0.6)), in.flag("in_liquid")));
         }},
        {"physics.leash_pull",
         [](Reader& in) {
             const entity::leash::Body self = in.body("self");
             const entity::leash::Body holder = in.body("holder");
             const double slack = in.number("slack", 6.0);
             const geometry::Point selfMotion = in.point("self_motion");
             const geometry::Point holderMotion = in.point("holder_motion");
             const entity::leash::Pull pull = entity::leash::elasticPull(
                 self, holder, in.flag("quad"), slack, selfMotion, holderMotion, in.point("known_motion"));
             if (!pull.pulled) {
                 return Value{};
             }
             return Value{Value::Map{{"torque", real(pull.torque)}, {"push", pointOf(pull.push)}}};
         }},
    };
    return kAll;
}

void entityMechanic(void*, abi::Call& c) {
    const abi::GuestMemory memory = c.memory();
    const std::optional<std::string_view> name = memory.string(c.i32(0), c.i32(1));
    if (!name) {
        c.ret(kBad);
        return;
    }
    const auto found = mechanics().find(*name);
    if (found == mechanics().end()) {
        c.ret(abi::kErrAbsent);
        return;
    }
    std::expected<Value, std::int32_t> args = Value{Value::Map{}};
    if (c.i32(3) != 0) {
        args = readValue(memory, c.i32(2), c.i32(3));
    }
    if (!args || args->asMap() == nullptr) {
        c.ret(kBad);
        return;
    }
    Reader reader{*args};
    const Value answer = found->second(reader);
    c.ret(reader.ok ? putValue(memory, c.i32(4), c.i32(5), answer) : kBad);
}
} // namespace

void addEntitiesMechanicsImports(State& state, std::vector<abi::HostImport>& out) {
    out.push_back({"entity_mechanic", entityMechanic, &state});
}
} // namespace basalt::server::plugins::wasm
