#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/command/ParticleArgument.hpp>
#include <basalt/server/runtime/command/WorldArguments.hpp>

#include <algorithm>
#include <cstdint>
#include <string>

namespace basalt::server::runtime::command {
namespace {

namespace play = wire::protocol::play;
namespace reg = server::registry;

using play::ParticleOptions;
using play::ParticleOptionShape;
using wire::common::Identifier;
using wire::nbt::NbtCompound;
using wire::nbt::NbtList;
using wire::nbt::NbtTag;
using wire::text::Component;

[[nodiscard]] CommandSyntaxError invalid(StringReader& reader, std::string_view what) {
    return CommandSyntaxError{
        .component = Component::translatable("particle.invalidOptions", {Component::text(std::string{what})}),
        .input = reader.string(),
        .cursor = reader.cursor()};
}

[[nodiscard]] std::optional<float> number(const NbtTag* tag) {
    if (tag == nullptr) {
        return std::nullopt;
    }
    if (const auto* value = tag->get<float>(); value != nullptr) {
        return *value;
    }
    if (const auto* value = tag->get<double>(); value != nullptr) {
        return static_cast<float>(*value);
    }
    if (const auto* value = tag->get<std::int32_t>(); value != nullptr) {
        return static_cast<float>(*value);
    }
    if (const auto* value = tag->get<std::int8_t>(); value != nullptr) {
        return static_cast<float>(*value);
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<std::int32_t> integer(const NbtTag* tag) {
    const std::optional<float> value = number(tag);
    return value.has_value() ? std::optional{static_cast<std::int32_t>(*value)} : std::nullopt;
}

/// parity: `ARGB.colorFromFloat`, which rounds each channel to a byte.
[[nodiscard]] std::int32_t packColor(float alpha, float red, float green, float blue) {
    const auto channel = [](float value) {
        return std::clamp(static_cast<std::int32_t>((value * 255.0F) + 0.5F), 0, 255);
    };
    return (channel(alpha) << 24) | (channel(red) << 16) | (channel(green) << 8) | channel(blue);
}

/// parity: `ExtraCodecs.RGB_COLOR_CODEC`.
[[nodiscard]] std::expected<std::int32_t, CommandSyntaxError> color(const NbtTag* tag, StringReader& reader) {
    if (const std::optional<std::int32_t> packed = integer(tag); packed.has_value()) {
        return *packed;
    }
    const auto* list = tag == nullptr ? nullptr : tag->get<NbtList>();
    if (list == nullptr || list->size() < 3) {
        return std::unexpected{invalid(reader, "color")};
    }
    const float alpha = list->size() >= 4 ? number(&(*list)[3]).value_or(0.0F) : 1.0F;
    return packColor(alpha, number(&(*list)[0]).value_or(0.0F), number(&(*list)[1]).value_or(0.0F),
                     number(&(*list)[2]).value_or(0.0F));
}

[[nodiscard]] std::expected<std::int32_t, CommandSyntaxError> blockState(const NbtTag* tag, StringReader& reader) {
    std::optional<std::string> name;
    if (tag != nullptr) {
        if (const auto* text = tag->get<std::string>(); text != nullptr) {
            name = *text;
        } else if (const auto* compound = tag->get<NbtCompound>(); compound != nullptr) {
            if (const NbtTag* inner = compound->find("id"); inner != nullptr) {
                if (const auto* named = inner->get<std::string>(); named != nullptr) {
                    name = *named;
                }
            }
        }
    }
    if (!name.has_value()) {
        return std::unexpected{invalid(reader, "block_state")};
    }
    const std::expected<Identifier, wire::common::IdentifierError> id = Identifier::parse(*name);
    const std::optional<std::int32_t> stateId = id.has_value() ? reg::blocks::stateIdOf(*id, {}) : std::nullopt;
    if (!stateId.has_value()) {
        return std::unexpected{invalid(reader, *name)};
    }
    return *stateId;
}

[[nodiscard]] std::expected<ParticleOptions::Item, CommandSyntaxError> item(const NbtTag* tag, StringReader& reader) {
    std::optional<std::string> name;
    std::int32_t count = 1;
    if (tag != nullptr) {
        if (const auto* text = tag->get<std::string>(); text != nullptr) {
            name = *text;
        } else if (const auto* compound = tag->get<NbtCompound>(); compound != nullptr) {
            if (const NbtTag* inner = compound->find("id"); inner != nullptr) {
                if (const auto* named = inner->get<std::string>(); named != nullptr) {
                    name = *named;
                }
            }
            count = integer(compound->find("count")).value_or(1);
        }
    }
    if (!name.has_value()) {
        return std::unexpected{invalid(reader, "item")};
    }
    const std::expected<Identifier, wire::common::IdentifierError> id = Identifier::parse(*name);
    const reg::Item* entry = id.has_value() ? reg::items::registry().byName(*id) : nullptr;
    if (entry == nullptr) {
        return std::unexpected{invalid(reader, *name)};
    }
    return ParticleOptions::Item{.itemId = entry->protocolId, .count = count};
}

[[nodiscard]] std::expected<ParticleOptions::Trail, CommandSyntaxError> trail(const NbtCompound& options,
                                                                              StringReader& reader) {
    const NbtTag* target = options.find("target");
    const auto* list = target == nullptr ? nullptr : target->get<NbtList>();
    if (list == nullptr || list->size() < 3) {
        return std::unexpected{invalid(reader, "target")};
    }
    const std::expected<std::int32_t, CommandSyntaxError> tint = color(options.find("color"), reader);
    if (!tint.has_value()) {
        return std::unexpected{tint.error()};
    }
    const std::optional<std::int32_t> duration = integer(options.find("duration"));
    if (!duration.has_value()) {
        return std::unexpected{invalid(reader, "duration")};
    }
    return ParticleOptions::Trail{.targetX = static_cast<double>(number(&(*list)[0]).value_or(0.0F)),
                                  .targetY = static_cast<double>(number(&(*list)[1]).value_or(0.0F)),
                                  .targetZ = static_cast<double>(number(&(*list)[2]).value_or(0.0F)),
                                  .color = *tint,
                                  .duration = *duration};
}

[[nodiscard]] std::expected<ParticleOptions::Vibration, CommandSyntaxError> vibration(const NbtCompound& options,
                                                                                      StringReader& reader) {
    const NbtTag* destination = options.find("destination");
    const auto* compound = destination == nullptr ? nullptr : destination->get<NbtCompound>();
    if (compound == nullptr) {
        return std::unexpected{invalid(reader, "destination")};
    }
    const NbtTag* pos = compound->find("pos");
    const auto* list = pos == nullptr ? nullptr : pos->get<NbtList>();
    if (list == nullptr || list->size() < 3) {
        return std::unexpected{invalid(reader, "pos")};
    }
    return ParticleOptions::Vibration{.target = play::BlockPos{integer(&(*list)[0]).value_or(0),
                                                               integer(&(*list)[1]).value_or(0),
                                                               integer(&(*list)[2]).value_or(0)},
                                      .arrivalTicks = integer(options.find("arrival_in_ticks")).value_or(0)};
}

[[nodiscard]] std::expected<std::optional<ParticleOptions>, CommandSyntaxError>
optionsOf(ParticleOptionShape shape, const NbtCompound& options, StringReader& reader) {
    switch (shape) {
    case ParticleOptionShape::None:
        return std::optional<ParticleOptions>{};
    case ParticleOptionShape::BlockState: {
        const auto state = blockState(options.find("block_state"), reader);
        if (!state.has_value()) {
            return std::unexpected{state.error()};
        }
        return ParticleOptions{ParticleOptions::BlockState{.stateId = *state}};
    }
    case ParticleOptionShape::Color: {
        const auto tint = color(options.find("color"), reader);
        if (!tint.has_value()) {
            return std::unexpected{tint.error()};
        }
        return ParticleOptions{ParticleOptions::Color{.color = *tint}};
    }
    case ParticleOptionShape::Spell: {
        // parity: both fields are optional, defaulting to -1 (white) and a power of one.
        std::int32_t tint = -1;
        if (options.find("color") != nullptr) {
            const auto read = color(options.find("color"), reader);
            if (!read.has_value()) {
                return std::unexpected{read.error()};
            }
            tint = *read;
        }
        return ParticleOptions{
            ParticleOptions::Spell{.color = tint, .power = number(options.find("power")).value_or(1.0F)}};
    }
    case ParticleOptionShape::Dust: {
        const auto tint = color(options.find("color"), reader);
        if (!tint.has_value()) {
            return std::unexpected{tint.error()};
        }
        return ParticleOptions{
            ParticleOptions::Dust{.color = *tint, .scale = number(options.find("scale")).value_or(1.0F)}};
    }
    case ParticleOptionShape::DustTransition: {
        const auto from = color(options.find("from_color"), reader);
        if (!from.has_value()) {
            return std::unexpected{from.error()};
        }
        const auto to = color(options.find("to_color"), reader);
        if (!to.has_value()) {
            return std::unexpected{to.error()};
        }
        return ParticleOptions{ParticleOptions::DustTransition{
            .fromColor = *from, .toColor = *to, .scale = number(options.find("scale")).value_or(1.0F)}};
    }
    case ParticleOptionShape::Item: {
        const auto stack = item(options.find("item"), reader);
        if (!stack.has_value()) {
            return std::unexpected{stack.error()};
        }
        return ParticleOptions{*stack};
    }
    case ParticleOptionShape::Trail: {
        const auto built = trail(options, reader);
        if (!built.has_value()) {
            return std::unexpected{built.error()};
        }
        return ParticleOptions{*built};
    }
    case ParticleOptionShape::Number: {
        const std::optional<float> value = number(options.find("roll"));
        return ParticleOptions{
            ParticleOptions::Number{.value = value.value_or(number(options.find("power")).value_or(1.0F))}};
    }
    case ParticleOptionShape::Delay:
        return ParticleOptions{ParticleOptions::Delay{.ticks = integer(options.find("delay")).value_or(0)}};
    case ParticleOptionShape::Geyser: {
        const std::optional<std::int32_t> blocks = integer(options.find("water_blocks"));
        if (!blocks.has_value()) {
            return std::unexpected{invalid(reader, "water_blocks")};
        }
        return ParticleOptions{ParticleOptions::Geyser{.waterBlocks = *blocks}};
    }
    case ParticleOptionShape::GeyserBase: {
        const std::optional<std::int32_t> blocks = integer(options.find("water_blocks"));
        if (!blocks.has_value()) {
            return std::unexpected{invalid(reader, "water_blocks")};
        }
        const std::optional<float> impulse = number(options.find("burst_impulse_base"));
        if (!impulse.has_value()) {
            return std::unexpected{invalid(reader, "burst_impulse_base")};
        }
        return ParticleOptions{ParticleOptions::GeyserBase{.waterBlocks = *blocks, .burstImpulseBase = *impulse}};
    }
    case ParticleOptionShape::Vibration:
    default: {
        const auto built = vibration(options, reader);
        if (!built.has_value()) {
            return std::unexpected{built.error()};
        }
        return ParticleOptions{*built};
    }
    }
}

class ParticleArgumentType final : public ArgumentType {
public:
    [[nodiscard]] Parsed<ArgumentValue> parse(StringReader& reader, const CommandSource& source,
                                              const CommandEnvironment& environment) const override {
        const std::int32_t start = reader.cursor();
        const std::expected<Identifier, wire::common::IdentifierError> id = Identifier::parse(reader.readIdentifier());
        const reg::ParticleType* type = id.has_value() ? reg::particleTypes::of(*id) : nullptr;
        if (type == nullptr) {
            reader.setCursor(start);
            return std::unexpected{CommandSyntaxError{
                .component = Component::translatable(
                    "particle.notFound", {Component::text(id.has_value() ? id->toString() : std::string{})}),
                .input = reader.string(),
                .cursor = reader.cursor()}};
        }
        NbtCompound compound;
        if (reader.canRead() && reader.peek() == '{') {
            const Parsed<ArgumentValue> read = nbtCompoundArgument()->parse(reader, source, environment);
            if (!read.has_value()) {
                return std::unexpected{read.error()};
            }
            if (const auto* value = read->as<NbtCompound>(); value != nullptr) {
                compound = *value;
            }
        }
        const auto options = optionsOf(type->optionShape, compound, reader);
        if (!options.has_value()) {
            return std::unexpected{options.error()};
        }
        return ArgumentValue::of(ParsedParticle{.type = type, .options = *options});
    }

    [[nodiscard]] play::ArgumentParser wire() const override {
        return play::ArgumentParser{play::argumentType::kParticle};
    }
};

} // namespace

std::shared_ptr<const ArgumentType> particleArgument() {
    static const auto type = std::make_shared<const ParticleArgumentType>();
    return type;
}

} // namespace basalt::server::runtime::command
