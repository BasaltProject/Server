#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/structure/StructureTemplate.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace basalt::server::world::structure {
namespace {

constexpr std::array<std::string_view, 4> kHorizontalNames{"north", "east", "south", "west"};

[[nodiscard]] std::string lowerName(StructureDirection direction) { return std::string{serializedName(direction)}; }

[[nodiscard]] std::int32_t signedRotation(std::int32_t rotation) { return rotation > 8 ? rotation - 16 : rotation; }

/// parity: `StairBlock.mirror`.
[[nodiscard]] std::optional<std::string> mirrorStairShape(const std::string& shape, StructureMirror mirror,
                                                          const std::string* facing) {
    if (facing == nullptr) {
        return std::nullopt;
    }
    const std::optional<StructureDirection> axis = directionFromName(*facing);
    if (!axis.has_value()) {
        return std::nullopt;
    }
    bool applies = false;
    if (mirror == StructureMirror::LeftRight) {
        applies = *axis == StructureDirection::North || *axis == StructureDirection::South;
    } else if (mirror == StructureMirror::FrontBack) {
        applies = *axis == StructureDirection::East || *axis == StructureDirection::West;
    }
    if (!applies) {
        return std::nullopt;
    }
    if (shape == "outer_left") {
        return "outer_right";
    }
    if (shape == "outer_right") {
        return "outer_left";
    }
    if (shape == "inner_left" && mirror == StructureMirror::LeftRight) {
        return "inner_right";
    }
    if (shape == "inner_right" && mirror == StructureMirror::LeftRight) {
        return "inner_left";
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<std::string> transformHorizontal(std::string_view direction, StructureMirror mirror,
                                                             StructureRotation rotation) {
    const std::optional<StructureDirection> parsed = directionFromName(direction);
    if (!parsed.has_value()) {
        return std::nullopt;
    }
    return lowerName(rotated(mirrored(*parsed, mirror), rotation));
}

[[nodiscard]] std::optional<std::string> rotateRailShape(const std::string& shape, StructureMirror mirror,
                                                         StructureRotation rotation) {
    static constexpr std::string_view kAscending = "ascending_";
    if (shape.starts_with(kAscending)) {
        const std::optional<std::string> direction =
            transformHorizontal(std::string_view{shape}.substr(kAscending.size()), mirror, rotation);
        if (!direction.has_value()) {
            return std::nullopt;
        }
        return std::string{kAscending} + *direction;
    }
    std::vector<std::string_view> parts;
    if (shape == "north_south") {
        parts = {"north", "south"};
    } else if (shape == "east_west") {
        parts = {"east", "west"};
    } else if (shape == "south_east") {
        parts = {"south", "east"};
    } else if (shape == "south_west") {
        parts = {"south", "west"};
    } else if (shape == "north_west") {
        parts = {"north", "west"};
    } else if (shape == "north_east") {
        parts = {"north", "east"};
    } else {
        return std::nullopt;
    }
    std::set<std::string> directions;
    for (const std::string_view part : parts) {
        if (const std::optional<std::string> moved = transformHorizontal(part, mirror, rotation); moved.has_value()) {
            directions.insert(*moved);
        }
    }
    static const std::vector<std::pair<std::set<std::string>, std::string>> kShapes{
        {{"north", "south"}, "north_south"}, {{"east", "west"}, "east_west"},   {{"south", "east"}, "south_east"},
        {{"south", "west"}, "south_west"},   {{"north", "west"}, "north_west"}, {{"north", "east"}, "north_east"}};
    for (const auto& [set, name] : kShapes) {
        if (directions == set) {
            return name;
        }
    }
    return std::nullopt;
}

} // namespace

std::int32_t StructureTemplate::transformedStateId(const std::int32_t stateId,
                                                   const std::map<std::string, std::string>& properties,
                                                   const StructureMirror mirror, const StructureRotation rotation) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    if (block == nullptr) {
        return stateId;
    }
    std::map<std::string, std::string> transformed = properties;
    if (const auto facing = properties.find("facing"); facing != properties.end()) {
        if (const std::optional<StructureDirection> parsed = directionFromName(facing->second); parsed.has_value()) {
            transformed["facing"] = lowerName(rotated(mirrored(*parsed, mirror), rotation));
        }
    }
    if (const auto axis = properties.find("axis"); axis != properties.end()) {
        const bool swaps =
            rotation == StructureRotation::Clockwise90 || rotation == StructureRotation::Counterclockwise90;
        if (swaps && (axis->second == "x" || axis->second == "z")) {
            transformed["axis"] = axis->second == "x" ? "z" : "x";
        }
    }
    std::vector<std::string_view> connections;
    for (const std::string_view name : kHorizontalNames) {
        if (properties.contains(std::string{name})) {
            connections.push_back(name);
        }
    }
    if (!connections.empty()) {
        for (const std::string_view name : connections) {
            transformed.erase(std::string{name});
        }
        for (const std::string_view name : connections) {
            const std::optional<StructureDirection> direction = directionFromName(name);
            if (!direction.has_value()) {
                continue;
            }
            transformed[lowerName(rotated(mirrored(*direction, mirror), rotation))] = properties.at(std::string{name});
        }
    }
    if (const auto rotationProperty = properties.find("rotation"); rotationProperty != properties.end()) {
        std::int32_t original = 0;
        bool parsed = !rotationProperty->second.empty();
        for (const char character : rotationProperty->second) {
            if (character < '0' || character > '9') {
                parsed = false;
                break;
            }
            original = original * 10 + (character - '0');
        }
        if (parsed) {
            std::int32_t mirroredRotation = original;
            if (mirror == StructureMirror::LeftRight) {
                mirroredRotation = (8 - signedRotation(original) + 16) % 16;
            } else if (mirror == StructureMirror::FrontBack) {
                mirroredRotation = (16 - signedRotation(original)) % 16;
            }
            std::int32_t rotatedValue = mirroredRotation;
            if (rotation == StructureRotation::Clockwise90) {
                rotatedValue = (mirroredRotation + 4) % 16;
            } else if (rotation == StructureRotation::Clockwise180) {
                rotatedValue = (mirroredRotation + 8) % 16;
            } else if (rotation == StructureRotation::Counterclockwise90) {
                rotatedValue = (mirroredRotation + 12) % 16;
            }
            transformed["rotation"] = std::to_string(rotatedValue);
        }
    }
    // parity: `DoorBlock.mirror` cycles the hinge for *any* mirror.
    if (mirror != StructureMirror::None) {
        if (const auto hinge = properties.find("hinge"); hinge != properties.end()) {
            if (hinge->second == "left") {
                transformed["hinge"] = "right";
            } else if (hinge->second == "right") {
                transformed["hinge"] = "left";
            }
        }
    }
    if (const auto shape = properties.find("shape"); shape != properties.end()) {
        std::optional<std::string> transformedShape = rotateRailShape(shape->second, mirror, rotation);
        if (!transformedShape.has_value()) {
            const auto facing = properties.find("facing");
            transformedShape =
                mirrorStairShape(shape->second, mirror, facing == properties.end() ? nullptr : &facing->second);
        }
        if (transformedShape.has_value()) {
            transformed["shape"] = *transformedShape;
        }
    }
    return registry::blocks::stateIdOf(*block, transformed).value_or(stateId);
}

} // namespace basalt::server::world::structure
