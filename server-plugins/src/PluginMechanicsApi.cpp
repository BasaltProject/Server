#include "PluginApiShared.hpp"

#include <basalt/server/entity/Breeding.hpp>
#include <basalt/server/entity/Collision.hpp>
#include <basalt/server/entity/Combat.hpp>
#include <basalt/server/entity/Geometry.hpp>
#include <basalt/server/entity/Leash.hpp>
#include <basalt/server/entity/LevelValue.hpp>
#include <basalt/server/entity/Projectiles.hpp>

#include <span>
#include <vector>

namespace basalt::server::plugins::pluginApi::internal {
namespace {
namespace geometry = entity::geometry;
namespace projectile = entity::projectile;

double numberAt(LuaArgs& args, std::int32_t table, std::int64_t position, double fallback = 0.0) {
    args.pushIndex(table, position);
    const double value = args.isNumber(-1) ? args.toNumber(-1) : fallback;
    args.pop(1);
    return value;
}

double numberIn(LuaArgs& args, std::int32_t table, std::string_view key, double fallback = 0.0) {
    args.pushField(table, key);
    const double value = args.isNumber(-1) ? args.toNumber(-1) : fallback;
    args.pop(1);
    return value;
}

bool flagIn(LuaArgs& args, std::int32_t table, std::string_view key) {
    args.pushField(table, key);
    const bool value = args.isBoolean(-1) && args.toBoolean(-1);
    args.pop(1);
    return value;
}

geometry::Point pointAt(LuaArgs& args, std::int32_t index) {
    if (!args.isTable(index)) {
        args.raise("expected a point { x, y, z }");
    }
    return {numberAt(args, index, 1), numberAt(args, index, 2), numberAt(args, index, 3)};
}

geometry::Box boxIn(LuaArgs& args, std::int32_t table) {
    return geometry::makeBox(numberIn(args, table, "min_x"), numberIn(args, table, "min_y"),
                             numberIn(args, table, "min_z"), numberIn(args, table, "max_x"),
                             numberIn(args, table, "max_y"), numberIn(args, table, "max_z"));
}

void pushBox(LuaArgs& args, const geometry::Box& box) {
    args.pushTable();
    const std::array<std::pair<const char*, double>, 6> fields{{{"min_x", box.minX},
                                                                {"min_y", box.minY},
                                                                {"min_z", box.minZ},
                                                                {"max_x", box.maxX},
                                                                {"max_y", box.maxY},
                                                                {"max_z", box.maxZ}}};
    for (const auto& [key, value] : fields) {
        args.pushNumber(value);
        args.setField(key);
    }
}

void pushPoint(LuaArgs& args, const std::array<double, 3>& point) {
    args.pushTable();
    for (std::size_t at = 0; at < 3; ++at) {
        args.pushNumber(point[at]);
        args.setIndex(static_cast<std::int64_t>(at) + 1);
    }
}

std::vector<projectile::Candidate> candidatesAt(LuaArgs& args, std::int32_t index) {
    std::vector<projectile::Candidate> out;
    const std::int64_t count = args.isTable(index) ? args.length(index) : 0;
    out.reserve(static_cast<std::size_t>(count));
    for (std::int64_t at = 1; at <= count; ++at) {
        args.pushIndex(index, at);
        const std::int32_t entry = args.top();
        projectile::Candidate candidate;
        candidate.box = boxIn(args, entry);
        candidate.pickRadius = static_cast<float>(numberIn(args, entry, "pick_radius"));
        candidate.pickableFromInside = flagIn(args, entry, "pick_from_inside");
        candidate.rootVehicle = static_cast<std::int32_t>(numberIn(args, entry, "root"));
        out.push_back(candidate);
        args.pop(1);
    }
    return out;
}

std::vector<entity::collision::Collider> collidersAt(LuaArgs& args, std::int32_t index) {
    std::vector<entity::collision::Collider> out;
    const std::int64_t count = args.isTable(index) ? args.length(index) : 0;
    out.reserve(static_cast<std::size_t>(count));
    for (std::int64_t at = 1; at <= count; ++at) {
        args.pushIndex(index, at);
        const std::int32_t entry = args.top();
        out.push_back({boxIn(args, entry), static_cast<std::int32_t>(numberIn(args, entry, "shape"))});
        args.pop(1);
    }
    return out;
}

int pushHit(LuaArgs& args, const projectile::Hit& hit) {
    if (hit.index < 0) {
        args.pushNil();
        return 1;
    }
    args.pushInteger(hit.index + 1);
    pushPoint(args, hit.at);
    return 2;
}

geometry::Point triple(LuaArgs& args, std::int32_t table, std::string_view key) {
    args.pushField(table, key);
    const geometry::Point value = pointAt(args, args.top());
    args.pop(1);
    return value;
}

entity::leash::Body bodyIn(LuaArgs& args, std::int32_t table, std::string_view key) {
    args.pushField(table, key);
    const std::int32_t entry = args.top();
    entity::leash::Body body;
    body.position = triple(args, entry, "position");
    body.width = static_cast<float>(numberIn(args, entry, "width"));
    body.height = static_cast<float>(numberIn(args, entry, "height"));
    body.yaw = static_cast<float>(numberIn(args, entry, "yaw"));
    args.pop(1);
    return body;
}

constexpr int kMaxLevelValueDepth = 16;

void encodeLevelValue(LuaArgs& args, std::int32_t index, std::vector<double>& program, int depth) {
    using Op = entity::levelValue::Op;
    const auto emit = [&](Op op) { program.push_back(static_cast<double>(static_cast<int>(op))); };
    if (args.isNumber(index) || depth > kMaxLevelValueDepth || !args.isTable(index)) {
        emit(Op::Constant);
        program.push_back(args.isNumber(index) ? args.toNumber(index) : 0.0);
        return;
    }
    const std::string type = stringField(args, index, "type").value_or("constant");
    const std::string name = type.starts_with("minecraft:") ? type.substr(10) : type;
    const auto child = [&](std::string_view key) {
        args.pushField(index, key);
        encodeLevelValue(args, args.top(), program, depth + 1);
        args.pop(1);
    };
    if (name == "linear") {
        emit(Op::Linear);
        program.push_back(numberIn(args, index, "base"));
        program.push_back(numberIn(args, index, "per_level_above_first"));
    } else if (name == "levels_squared") {
        emit(Op::LevelsSquared);
        program.push_back(numberIn(args, index, "added"));
    } else if (name == "clamped") {
        emit(Op::Clamped);
        program.push_back(numberIn(args, index, "min"));
        program.push_back(numberIn(args, index, "max"));
        child("value");
    } else if (name == "fraction") {
        emit(Op::Fraction);
        child("numerator");
        child("denominator");
    } else if (name == "exponent") {
        emit(Op::Exponent);
        child("base");
        child("power");
    } else if (name == "lookup") {
        emit(Op::Lookup);
        args.pushField(index, "values");
        const std::int64_t count = args.isTable(-1) ? args.length(args.top()) : 0;
        program.push_back(static_cast<double>(count));
        for (std::int64_t at = 1; at <= count; ++at) {
            program.push_back(numberAt(args, args.top(), at));
        }
        args.pop(1);
        child("fallback");
    } else {
        emit(Op::Constant);
        program.push_back(numberIn(args, index, "value"));
    }
}

void pushFloat(LuaArgs& args, float value) { args.pushNumber(static_cast<double>(value)); }

void installEnchantments(LuaArgs& lua) {
    lua.pushTable();
    wire::lua::pushHostFunction(lua, "enchantments.level_value", [](LuaArgs& args) {
        std::vector<double> program;
        encodeLevelValue(args, 1, program, 0);
        pushFloat(args, entity::levelValue::evaluate(program, static_cast<int>(args.checkInteger(2))));
        return 1;
    });
    lua.setField("level_value");
    lua.setGlobal("enchantments");
}

void installBreeding(LuaArgs& lua) {
    lua.pushTable();
    wire::lua::pushHostFunction(lua, "breeding.offspring_attribute", [](LuaArgs& args) {
        args.pushNumber(entity::breeding::offspringAttribute(
            args.checkNumber(1), args.checkNumber(2), args.checkNumber(3), args.checkNumber(4), args.checkNumber(5),
            args.checkNumber(6), args.checkNumber(7)));
        return 1;
    });
    lua.setField("offspring_attribute");
    lua.setGlobal("breeding");
}

void installCombat(LuaArgs& lua) {
    namespace combat = entity::combat;
    lua.pushTable();
    wire::lua::pushHostFunction(lua, "combat.after_armor", [](LuaArgs& args) {
        pushFloat(args, combat::afterArmour(
                            static_cast<float>(args.checkNumber(1)), static_cast<float>(args.checkNumber(2)),
                            static_cast<float>(args.checkNumber(3)), static_cast<float>(args.numberOr(4, 0.0))));
        return 1;
    });
    lua.setField("after_armor");
    wire::lua::pushHostFunction(lua, "combat.after_protection", [](LuaArgs& args) {
        pushFloat(args, combat::afterProtection(static_cast<float>(args.checkNumber(1)),
                                                static_cast<float>(args.checkNumber(2))));
        return 1;
    });
    lua.setField("after_protection");
    wire::lua::pushHostFunction(lua, "combat.after_resistance", [](LuaArgs& args) {
        pushFloat(args, combat::afterResistance(static_cast<float>(args.checkNumber(1)),
                                                static_cast<std::int32_t>(args.checkInteger(2))));
        return 1;
    });
    lua.setField("after_resistance");
    wire::lua::pushHostFunction(lua, "combat.after_difficulty", [](LuaArgs& args) {
        pushFloat(args, combat::afterDifficulty(static_cast<float>(args.checkNumber(1)), args.checkString(2)));
        return 1;
    });
    lua.setField("after_difficulty");
    wire::lua::pushHostFunction(lua, "combat.scales_with_difficulty", [](LuaArgs& args) {
        args.pushBoolean(combat::scalesWithDifficulty(args.checkString(1), args.booleanOr(2, false)));
        return 1;
    });
    lua.setField("scales_with_difficulty");
    wire::lua::pushHostFunction(lua, "combat.absorb", [](LuaArgs& args) {
        const combat::Absorbed split =
            combat::absorb(static_cast<float>(args.checkNumber(1)), static_cast<float>(args.checkNumber(2)));
        pushFloat(args, split.health);
        pushFloat(args, split.absorption);
        return 2;
    });
    lua.setField("absorb");
    wire::lua::pushHostFunction(lua, "combat.fall_damage", [](LuaArgs& args) {
        args.pushInteger(combat::fallDamage(args.checkNumber(1), args.numberOr(2, combat::kSafeFallDistance)));
        return 1;
    });
    lua.setField("fall_damage");
    lua.setGlobal("combat");
}

void installPhysics(LuaArgs& lua) {
    lua.pushTable();
    wire::lua::pushHostFunction(lua, "physics.clip", [](LuaArgs& args) {
        const auto at = geometry::clip(boxIn(args, 1), pointAt(args, 2), pointAt(args, 3));
        if (!at.has_value()) {
            args.pushNil();
            return 1;
        }
        pushPoint(args, *at);
        return 1;
    });
    lua.setField("clip");
    wire::lua::pushHostFunction(lua, "physics.collide", [](LuaArgs& args) {
        const auto colliders = collidersAt(args, 1);
        pushPoint(args, entity::collision::sweep(colliders, boxIn(args, 2), pointAt(args, 3)));
        return 1;
    });
    lua.setField("collide");
    wire::lua::pushHostFunction(lua, "physics.step_up", [](LuaArgs& args) {
        const auto colliders = collidersAt(args, 1);
        const auto stepped = entity::collision::stepUp(colliders, boxIn(args, 2), args.checkNumber(3), pointAt(args, 4),
                                                       static_cast<float>(args.checkNumber(5)), args.checkNumber(6));
        if (!stepped.has_value()) {
            args.pushNil();
            return 1;
        }
        pushPoint(args, *stepped);
        return 1;
    });
    lua.setField("step_up");
    wire::lua::pushHostFunction(lua, "physics.rotate_towards_movement", [](LuaArgs& args) {
        const geometry::Point motion = pointAt(args, 1);
        const auto turned = projectile::rotateTowardsMovement(
            motion[0], motion[1], motion[2], static_cast<float>(args.checkNumber(2)),
            static_cast<float>(args.checkNumber(3)), static_cast<float>(args.checkNumber(4)),
            static_cast<float>(args.checkNumber(5)), static_cast<float>(args.checkNumber(6)));
        if (!turned.has_value()) {
            args.pushNil();
            return 1;
        }
        args.pushTable();
        const std::array<std::pair<const char*, float>, 4> fields{
            {{"x_rot_o", turned->xRotO}, {"y_rot_o", turned->yRotO}, {"x_rot", turned->xRot}, {"y_rot", turned->yRot}}};
        for (const auto& [key, value] : fields) {
            pushFloat(args, value);
            args.setField(key);
        }
        return 1;
    });
    lua.setField("rotate_towards_movement");
    wire::lua::pushHostFunction(lua, "physics.closest_hit", [](LuaArgs& args) {
        const std::vector<projectile::Candidate> candidates = candidatesAt(args, 1);
        return pushHit(args, projectile::closestHit(candidates, pointAt(args, 2), pointAt(args, 3),
                                                    static_cast<float>(args.checkNumber(4))));
    });
    lua.setField("closest_hit");
    wire::lua::pushHostFunction(lua, "physics.picked_hit", [](LuaArgs& args) {
        const std::vector<projectile::Candidate> candidates = candidatesAt(args, 1);
        return pushHit(args, projectile::pickedHit(candidates, static_cast<std::int32_t>(args.checkInteger(2)),
                                                   pointAt(args, 3), pointAt(args, 4), args.checkNumber(5)));
    });
    lua.setField("picked_hit");
    wire::lua::pushHostFunction(lua, "physics.projectile_margin", [](LuaArgs& args) {
        pushFloat(args, projectile::margin(static_cast<std::int32_t>(args.checkInteger(1))));
        return 1;
    });
    lua.setField("projectile_margin");
    wire::lua::pushHostFunction(lua, "physics.shulker_box", [](LuaArgs& args) {
        pushBox(args, projectile::shulkerProgressBox(static_cast<float>(args.checkNumber(1)),
                                                     static_cast<std::int32_t>(args.checkInteger(2)),
                                                     static_cast<float>(args.checkNumber(3)),
                                                     static_cast<float>(args.checkNumber(4)), pointAt(args, 5)));
        return 1;
    });
    lua.setField("shulker_box");
    wire::lua::pushHostFunction(lua, "physics.leash_angular_friction", [](LuaArgs& args) {
        pushFloat(args,
                  entity::leash::angularFriction(args.booleanOr(1, false), static_cast<float>(args.numberOr(2, 0.6)),
                                                 args.booleanOr(3, false)));
        return 1;
    });
    lua.setField("leash_angular_friction");
    wire::lua::pushHostFunction(lua, "physics.leash_pull", [](LuaArgs& args) {
        if (!args.isTable(1)) {
            args.raise("physics.leash_pull{ self, holder, quad, slack, self_motion, holder_motion, known_motion }");
        }
        const entity::leash::Pull pull =
            entity::leash::elasticPull(bodyIn(args, 1, "self"), bodyIn(args, 1, "holder"), flagIn(args, 1, "quad"),
                                       numberIn(args, 1, "slack", 6.0), triple(args, 1, "self_motion"),
                                       triple(args, 1, "holder_motion"), triple(args, 1, "known_motion"));
        if (!pull.pulled) {
            args.pushNil();
            return 1;
        }
        args.pushTable();
        args.pushNumber(pull.torque);
        args.setField("torque");
        pushPoint(args, pull.push);
        args.setField("push");
        return 1;
    });
    lua.setField("leash_pull");
    lua.setGlobal("physics");
}
} // namespace

void installMechanics(LuaArgs& lua) {
    installCombat(lua);
    installBreeding(lua);
    installEnchantments(lua);
    installPhysics(lua);
}
} // namespace basalt::server::plugins::pluginApi::internal
