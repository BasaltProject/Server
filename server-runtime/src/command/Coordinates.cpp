#include <basalt/server/runtime/command/Coordinates.hpp>

#include <cmath>
#include <string>

namespace basalt::server::runtime::command {
namespace {

namespace parserId = wire::protocol::play::argumentType;

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
        const Parsed<WorldCoordinate> y = WorldCoordinate::parse(reader, centre);
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

std::array<std::int32_t, 3> BlockCoordinates::resolve(const std::array<double, 3>& base) const {
    return {static_cast<std::int32_t>(std::floor(x.resolve(base[0]))),
            static_cast<std::int32_t>(std::floor(y.resolve(base[1]))),
            static_cast<std::int32_t>(std::floor(z.resolve(base[2])))};
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
