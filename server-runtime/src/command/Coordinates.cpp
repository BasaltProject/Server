#include <basalt/server/runtime/command/Coordinates.hpp>
#include <basalt/server/runtime/command/ExecutionSource.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <cmath>
#include <cstdint>
#include <numbers>
#include <string>

namespace basalt::server::runtime::command {
namespace {

namespace parserId = wire::protocol::play::argumentType;

/// parity: `Player.getEyeHeight` standing, which is what `anchored eyes` adds.
constexpr double kEyeHeight = 1.62;

using wire::protocol::play::ArgumentParser;
using wire::text::Component;

[[nodiscard]] CommandSyntaxError incomplete(StringReader& reader, std::int32_t start, bool threeDimensional) {
    reader.setCursor(start);
    return CommandSyntaxError{.component = Component::translatable(threeDimensional ? "argument.pos3d.incomplete"
                                                                                    : "argument.pos2d.incomplete"),
                              .input = reader.string(),
                              .cursor = start};
}

/// The separator between two coordinates, which is always exactly one space.
[[nodiscard]] std::optional<CommandSyntaxError> expectSeparator(StringReader& reader, std::int32_t start,
                                                                bool threeDimensional) {
    if (!reader.canRead() || reader.peek() != ' ') {
        return incomplete(reader, start, threeDimensional);
    }
    reader.skip();
    return std::nullopt;
}

/// parity: `LocalCoordinates.readDouble`.
[[nodiscard]] Parsed<double> readLocal(StringReader& reader, std::int32_t start) {
    if (!reader.canRead()) {
        return std::unexpected{CommandSyntaxError{.component = Component::translatable("argument.pos.missing.double"),
                                                  .input = reader.string(),
                                                  .cursor = reader.cursor()}};
    }
    if (reader.peek() != '^') {
        reader.setCursor(start);
        return std::unexpected{CommandSyntaxError{
            .component = Component::translatable("argument.pos.mixed"), .input = reader.string(), .cursor = start}};
    }
    reader.skip();
    if (reader.canRead() && reader.peek() != ' ') {
        return reader.readDouble();
    }
    return 0.0;
}

/// parity: `LocalCoordinates.parse`, three `^` numbers or none at all.
[[nodiscard]] Parsed<std::array<WorldCoordinate, 3>> parseLocal(StringReader& reader) {
    const std::int32_t start = reader.cursor();
    std::array<WorldCoordinate, 3> read{};
    for (std::size_t axis = 0; axis < read.size(); ++axis) {
        if (axis > 0) {
            if (const std::optional<CommandSyntaxError> failure = expectSeparator(reader, start, true);
                failure.has_value()) {
                return std::unexpected{*failure};
            }
        }
        const Parsed<double> value = readLocal(reader, start);
        if (!value.has_value()) {
            return std::unexpected{value.error()};
        }
        read[axis] = WorldCoordinate{.relative = true, .value = *value};
    }
    return read;
}

/// parity: `Vec3.applyLocalCoordinatesToRotation`, with Mth's sine table.
[[nodiscard]] std::array<double, 3> turned(std::pair<float, float> rotation, double left, double up, double forwards) {
    constexpr auto kRadians = static_cast<float>(std::numbers::pi / 180.0);
    const float yCos = wire::common::mth::cos((rotation.first + 90.0F) * kRadians);
    const float ySin = wire::common::mth::sin((rotation.first + 90.0F) * kRadians);
    const float xCos = wire::common::mth::cos(-rotation.second * kRadians);
    const float xSin = wire::common::mth::sin(-rotation.second * kRadians);
    const float xCosUp = wire::common::mth::cos((-rotation.second + 90.0F) * kRadians);
    const float xSinUp = wire::common::mth::sin((-rotation.second + 90.0F) * kRadians);
    const std::array<double, 3> ahead{static_cast<double>(yCos * xCos), static_cast<double>(xSin),
                                      static_cast<double>(ySin * xCos)};
    const std::array<double, 3> above{static_cast<double>(yCos * xCosUp), static_cast<double>(xSinUp),
                                      static_cast<double>(ySin * xCosUp)};
    const std::array<double, 3> side{-((ahead[1] * above[2]) - (ahead[2] * above[1])),
                                     -((ahead[2] * above[0]) - (ahead[0] * above[2])),
                                     -((ahead[0] * above[1]) - (ahead[1] * above[0]))};
    return {(ahead[0] * forwards) + (above[0] * up) + (side[0] * left),
            (ahead[1] * forwards) + (above[1] * up) + (side[1] * left),
            (ahead[2] * forwards) + (above[2] * up) + (side[2] * left)};
}

/// parity: `EntityAnchor.apply`, the feet or the eyes of whoever runs it.
[[nodiscard]] std::array<double, 3> anchorOf(const CommandSource& source) {
    std::array<double, 3> at = source.position();
    if (const ExecutionSource* execution = source.asExecution(); execution != nullptr && execution->anchorEyes()) {
        at[1] += kEyeHeight;
    }
    return at;
}

[[nodiscard]] std::array<double, 3> localFrom(const std::array<double, 3>& base, std::pair<float, float> rotation,
                                              const WorldCoordinate& x, const WorldCoordinate& y,
                                              const WorldCoordinate& z) {
    const std::array<double, 3> offset = turned(rotation, x.value, y.value, z.value);
    return {base[0] + offset[0], base[1] + offset[1], base[2] + offset[2]};
}

[[nodiscard]] std::array<std::int32_t, 3> floored(const std::array<double, 3>& at) {
    return {static_cast<std::int32_t>(std::floor(at[0])), static_cast<std::int32_t>(std::floor(at[1])),
            static_cast<std::int32_t>(std::floor(at[2]))};
}

/// The four coordinate types differ in how many numbers they read and whether they centre-correct.
class CoordinatesArgument final : public ArgumentType {
public:
    enum class Shape : std::uint8_t { Vec3, Vec2, BlockPos, ColumnPos };

    explicit CoordinatesArgument(Shape shape) : shape_(shape) {}

    [[nodiscard]] Parsed<ArgumentValue> parse(StringReader& reader, const CommandSource&,
                                              const CommandEnvironment&) const override {
        const std::int32_t start = reader.cursor();
        const bool threeDimensional = shape_ == Shape::Vec3 || shape_ == Shape::BlockPos;
        const bool centre = shape_ == Shape::Vec3;
        if (threeDimensional && reader.canRead() && reader.peek() == '^') {
            const Parsed<std::array<WorldCoordinate, 3>> local = parseLocal(reader);
            if (!local.has_value()) {
                return std::unexpected{local.error()};
            }
            const auto& [left, up, forwards] = *local;
            if (shape_ == Shape::Vec3) {
                return ArgumentValue::of(Coordinates{.x = left, .y = up, .z = forwards, .local = true});
            }
            return ArgumentValue::of(BlockCoordinates{.x = left, .y = up, .z = forwards, .local = true});
        }
        const Parsed<WorldCoordinate> x = WorldCoordinate::parse(reader, centre);
        if (!x.has_value()) {
            return std::unexpected{x.error()};
        }
        if (const std::optional<CommandSyntaxError> failure = expectSeparator(reader, start, threeDimensional);
            failure.has_value()) {
            return std::unexpected{*failure};
        }
        if (!threeDimensional) {
            const Parsed<WorldCoordinate> second = WorldCoordinate::parse(reader, centre);
            if (!second.has_value()) {
                return std::unexpected{second.error()};
            }
            if (shape_ == Shape::Vec2) {
                return ArgumentValue::of(Coordinates{.x = *x, .y = WorldCoordinate{}, .z = *second});
            }
            return ArgumentValue::of(ColumnCoordinates{.x = *x, .z = *second});
        }
        // parity: `WorldCoordinates.parseDouble` never centres the height.
        const Parsed<WorldCoordinate> y = WorldCoordinate::parse(reader, false);
        if (!y.has_value()) {
            return std::unexpected{y.error()};
        }
        if (const std::optional<CommandSyntaxError> failure = expectSeparator(reader, start, true);
            failure.has_value()) {
            return std::unexpected{*failure};
        }
        const Parsed<WorldCoordinate> z = WorldCoordinate::parse(reader, centre);
        if (!z.has_value()) {
            return std::unexpected{z.error()};
        }
        if (shape_ == Shape::Vec3) {
            return ArgumentValue::of(Coordinates{.x = *x, .y = *y, .z = *z});
        }
        return ArgumentValue::of(BlockCoordinates{.x = *x, .y = *y, .z = *z});
    }

    [[nodiscard]] ArgumentParser wire() const override {
        switch (shape_) {
        case Shape::Vec2:
            return ArgumentParser{parserId::kVec2};
        case Shape::BlockPos:
            return ArgumentParser{parserId::kBlockPos};
        case Shape::ColumnPos:
            return ArgumentParser{parserId::kColumnPos};
        case Shape::Vec3:
        default:
            return ArgumentParser{parserId::kVec3};
        }
    }

private:
    Shape shape_;
};

} // namespace

Parsed<WorldCoordinate> WorldCoordinate::parse(StringReader& reader, bool centerCorrect) {
    if (reader.canRead() && reader.peek() == '^') {
        return std::unexpected{CommandSyntaxError{.component = Component::translatable("argument.pos.mixed"),
                                                  .input = reader.string(),
                                                  .cursor = reader.cursor()}};
    }
    if (!reader.canRead()) {
        return std::unexpected{CommandSyntaxError{.component = Component::translatable("argument.pos.missing.double"),
                                                  .input = reader.string(),
                                                  .cursor = reader.cursor()}};
    }
    const bool relative = reader.peek() == '~';
    if (relative) {
        reader.skip();
    }
    const std::int32_t start = reader.cursor();
    double value = 0.0;
    if (reader.canRead() && reader.peek() != ' ') {
        const Parsed<double> read = reader.readDouble();
        if (!read.has_value()) {
            return std::unexpected{read.error()};
        }
        value = *read;
    }
    const std::string text =
        reader.string().substr(static_cast<std::size_t>(start), static_cast<std::size_t>(reader.cursor() - start));
    if (relative && text.empty()) {
        return WorldCoordinate{.relative = true, .value = 0.0};
    }
    const bool corrected = text.find('.') == std::string::npos && !relative && centerCorrect;
    return WorldCoordinate{.relative = relative, .value = corrected ? value + 0.5 : value};
}

std::array<double, 3> Coordinates::resolve(const std::array<double, 3>& base) const {
    if (local) {
        return localFrom(base, {0.0F, 0.0F}, x, y, z);
    }
    return {x.resolve(base[0]), y.resolve(base[1]), z.resolve(base[2])};
}

std::array<double, 3> Coordinates::resolve(const CommandSource& source) const {
    return local ? localFrom(anchorOf(source), source.rotation(), x, y, z) : resolve(source.position());
}

std::array<std::int32_t, 3> BlockCoordinates::resolve(const std::array<double, 3>& base) const {
    if (local) {
        return floored(localFrom(base, {0.0F, 0.0F}, x, y, z));
    }
    return floored({x.resolve(base[0]), y.resolve(base[1]), z.resolve(base[2])});
}

std::array<std::int32_t, 3> BlockCoordinates::resolve(const CommandSource& source) const {
    return local ? floored(localFrom(anchorOf(source), source.rotation(), x, y, z)) : resolve(source.position());
}

std::array<std::int32_t, 2> ColumnCoordinates::resolve(const std::array<double, 3>& base) const {
    return {static_cast<std::int32_t>(std::floor(x.resolve(base[0]))),
            static_cast<std::int32_t>(std::floor(z.resolve(base[2])))};
}

std::shared_ptr<const ArgumentType> vec3Argument() {
    static const auto type = std::make_shared<const CoordinatesArgument>(CoordinatesArgument::Shape::Vec3);
    return type;
}

std::shared_ptr<const ArgumentType> vec2Argument() {
    static const auto type = std::make_shared<const CoordinatesArgument>(CoordinatesArgument::Shape::Vec2);
    return type;
}

std::shared_ptr<const ArgumentType> blockPosArgument() {
    static const auto type = std::make_shared<const CoordinatesArgument>(CoordinatesArgument::Shape::BlockPos);
    return type;
}

std::shared_ptr<const ArgumentType> columnPosArgument() {
    static const auto type = std::make_shared<const CoordinatesArgument>(CoordinatesArgument::Shape::ColumnPos);
    return type;
}

} // namespace basalt::server::runtime::command
