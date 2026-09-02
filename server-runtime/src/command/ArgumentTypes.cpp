#include <basalt/server/runtime/command/ArgumentTypes.hpp>
#include <basalt/server/runtime/command/CommandEnvironment.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <vector>

namespace basalt::server::runtime::command {
namespace {

namespace parserId = wire::protocol::play::argumentType;

using wire::protocol::play::ArgumentParser;
using wire::protocol::play::Gamemode;
using wire::text::Component;

class WordArgument final : public ArgumentType {
public:
    [[nodiscard]] Parsed<ArgumentValue> parse(StringReader& reader, const CommandSource&,
                                              const CommandEnvironment&) const override {
        Parsed<std::string> word = reader.readString();
        if (!word.has_value()) {
            return std::unexpected{word.error()};
        }
        return ArgumentValue::of(std::move(*word));
    }

    [[nodiscard]] ArgumentParser wire() const override {
        return ArgumentParser{parserId::kBrigadierString,
                              ArgumentParser::StringProperties{.kind = ArgumentParser::StringKind::SingleWord}};
    }
};

class BoolArgument final : public ArgumentType {
public:
    [[nodiscard]] Parsed<ArgumentValue> parse(StringReader& reader, const CommandSource&,
                                              const CommandEnvironment&) const override {
        const std::int32_t start = reader.cursor();
        const std::string word = reader.readUnquotedString();
        if (word == "true") {
            return ArgumentValue::of(true);
        }
        if (word == "false") {
            return ArgumentValue::of(false);
        }
        reader.setCursor(start);
        return std::unexpected{
            reader.syntaxError(word.empty() ? std::string{"Expected bool"}
                                            : "Invalid boolean, expected true or false but found '" + word + "'")};
    }

    [[nodiscard]] ArgumentParser wire() const override { return ArgumentParser{parserId::kBrigadierBool}; }
};

/// The shared shape of the three bounded number types: read, then refuse what is out of range.
template <typename T, std::int32_t kParserId>
class BoundedArgument final : public ArgumentType {
public:
    BoundedArgument(T min, T max, std::string name) : min_(min), max_(max), name_(std::move(name)) {}

    [[nodiscard]] Parsed<ArgumentValue> parse(StringReader& reader, const CommandSource&,
                                              const CommandEnvironment&) const override {
        const std::int32_t start = reader.cursor();
        T value{};
        if constexpr (std::is_same_v<T, std::int32_t>) {
            const Parsed<std::int32_t> read = reader.readInt();
            if (!read.has_value()) {
                return std::unexpected{read.error()};
            }
            value = *read;
        } else {
            const Parsed<double> read = reader.readDouble();
            if (!read.has_value()) {
                return std::unexpected{read.error()};
            }
            value = static_cast<T>(*read);
        }
        if (value < min_) {
            reader.setCursor(start);
            return std::unexpected{
                reader.syntaxError(name_ + " must not be less than " + written(min_) + ", found " + written(value))};
        }
        if (value > max_) {
            reader.setCursor(start);
            return std::unexpected{
                reader.syntaxError(name_ + " must not be more than " + written(max_) + ", found " + written(value))};
        }
        return ArgumentValue::of(value);
    }

    [[nodiscard]] ArgumentParser wire() const override {
        return ArgumentParser{kParserId, ArgumentParser::Bounds<T>{.min = min_, .max = max_}};
    }

private:
    [[nodiscard]] static std::string written(T value) {
        if constexpr (std::is_same_v<T, std::int32_t>) {
            return std::to_string(value);
        } else {
            std::string text = std::to_string(value);
            // Trailing zeroes are noise in a message the player reads; brigadier prints the shortest
            // form of the same number.
            while (text.size() > 1 && text.back() == '0') {
                text.pop_back();
            }
            if (!text.empty() && text.back() == '.') {
                text.pop_back();
            }
            return text;
        }
    }

    T min_;
    T max_;
    std::string name_;
};

/// parity: `AngleArgument` wraps into -180..180 rather than rejecting 540.
[[nodiscard]] Parsed<float> readAngle(StringReader& reader) {
    if (!reader.canRead()) {
        return std::unexpected{reader.syntaxError("Expected angle")};
    }
    if (reader.peek() == '~') {
        reader.skip();
    }
    float value = 0.0F;
    if (reader.canRead() && reader.peek() != ' ') {
        const Parsed<double> read = reader.readDouble();
        if (!read.has_value()) {
            return std::unexpected{read.error()};
        }
        value = static_cast<float>(*read);
    }
    float wrapped = std::fmod(value, 360.0F);
    if (wrapped < 0.0F) {
        wrapped += 360.0F;
    }
    return wrapped >= 180.0F ? wrapped - 360.0F : wrapped;
}

class AngleArgument final : public ArgumentType {
public:
    [[nodiscard]] Parsed<ArgumentValue> parse(StringReader& reader, const CommandSource&,
                                              const CommandEnvironment&) const override {
        const Parsed<float> angle = readAngle(reader);
        if (!angle.has_value()) {
            return std::unexpected{angle.error()};
        }
        return ArgumentValue::of(*angle);
    }

    [[nodiscard]] ArgumentParser wire() const override { return ArgumentParser{parserId::kAngle}; }
};

class RotationArgument final : public ArgumentType {
public:
    [[nodiscard]] Parsed<ArgumentValue> parse(StringReader& reader, const CommandSource&,
                                              const CommandEnvironment&) const override {
        const std::int32_t start = reader.cursor();
        const Parsed<float> yaw = readAngle(reader);
        if (!yaw.has_value()) {
            return std::unexpected{yaw.error()};
        }
        if (!reader.canRead() || reader.peek() != ' ') {
            reader.setCursor(start);
            return std::unexpected{
                CommandSyntaxError{.component = Component::translatable("argument.rotation.incomplete"),
                                   .input = reader.string(),
                                   .cursor = start}};
        }
        reader.skip();
        const Parsed<float> pitch = readAngle(reader);
        if (!pitch.has_value()) {
            return std::unexpected{pitch.error()};
        }
        return ArgumentValue::of(std::pair{*yaw, *pitch});
    }

    [[nodiscard]] ArgumentParser wire() const override { return ArgumentParser{parserId::kRotation}; }
};

/// parity: `TimeArgument`, whose units are ticks, seconds (20) and days (24000), and which rounds
/// the product down rather than to nearest.
class TimeArgument final : public ArgumentType {
public:
    explicit TimeArgument(std::int32_t min) : min_(min) {}

    [[nodiscard]] Parsed<ArgumentValue> parse(StringReader& reader, const CommandSource&,
                                              const CommandEnvironment&) const override {
        const std::int32_t start = reader.cursor();
        const Parsed<double> value = reader.readDouble();
        if (!value.has_value()) {
            return std::unexpected{value.error()};
        }
        char unit = 't';
        if (reader.canRead() && (reader.peek() == 'd' || reader.peek() == 's' || reader.peek() == 't')) {
            unit = reader.read();
        }
        const std::int32_t multiplier = unit == 'd' ? 24000 : (unit == 's' ? 20 : 1);
        const auto ticks = static_cast<std::int32_t>(*value * static_cast<double>(multiplier));
        if (ticks < min_) {
            reader.setCursor(start);
            return std::unexpected{
                CommandSyntaxError{.component = Component::translatable(
                                       "argument.time.tick_count_too_low",
                                       {Component::text(std::to_string(min_)), Component::text(std::to_string(ticks))}),
                                   .input = reader.string(),
                                   .cursor = reader.cursor()}};
        }
        return ArgumentValue::of(ticks);
    }

    [[nodiscard]] ArgumentParser wire() const override {
        return ArgumentParser{parserId::kTime, ArgumentParser::TimeProperties{.min = min_}};
    }

private:
    std::int32_t min_;
};

/// Everything left on the line. `minecraft:message` reads the same text; what makes it a distinct
/// type is that vanilla resolves `@selector` mentions and signs it, neither of which exists yet
/// (documented gap, same as signed chat).
class RestOfLineArgument final : public ArgumentType {
public:
    explicit RestOfLineArgument(bool asMessage) : asMessage_(asMessage) {}

    [[nodiscard]] Parsed<ArgumentValue> parse(StringReader& reader, const CommandSource&,
                                              const CommandEnvironment&) const override {
        std::string text = reader.remaining();
        reader.setCursor(static_cast<std::int32_t>(reader.string().size()));
        return ArgumentValue::of(std::move(text));
    }

    [[nodiscard]] ArgumentParser wire() const override {
        if (asMessage_) {
            return ArgumentParser{parserId::kMessage};
        }
        return ArgumentParser{parserId::kBrigadierString,
                              ArgumentParser::StringProperties{.kind = ArgumentParser::StringKind::GreedyPhrase}};
    }

private:
    bool asMessage_;
};

/// Basalt has no profile cache yet, so this only carries the name through; `/op` on a name nobody
/// has ever used is accepted, exactly as vanilla accepts it.
class GameProfileArgument final : public ArgumentType {
public:
    [[nodiscard]] Parsed<ArgumentValue> parse(StringReader& reader, const CommandSource&,
                                              const CommandEnvironment&) const override {
        Parsed<std::string> name = reader.readString();
        if (!name.has_value()) {
            return std::unexpected{name.error()};
        }
        return ArgumentValue::of(std::move(*name));
    }

    [[nodiscard]] ArgumentParser wire() const override { return ArgumentParser{parserId::kGameProfile}; }
};

class GameModeArgument final : public ArgumentType {
public:
    [[nodiscard]] Parsed<ArgumentValue> parse(StringReader& reader, const CommandSource&,
                                              const CommandEnvironment&) const override {
        const std::int32_t start = reader.cursor();
        const std::string name = reader.readUnquotedString();
        static constexpr std::array<std::pair<std::string_view, Gamemode>, 4> kModes{
            {{"survival", Gamemode::Survival},
             {"creative", Gamemode::Creative},
             {"adventure", Gamemode::Adventure},
             {"spectator", Gamemode::Spectator}}};
        std::string lowered;
        lowered.reserve(name.size());
        for (const char letter : name) {
            lowered.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(letter))));
        }
        for (const auto& [named, mode] : kModes) {
            if (lowered == named) {
                return ArgumentValue::of(mode);
            }
        }
        reader.setCursor(start);
        return std::unexpected{CommandSyntaxError{
            .component = Component::translatable("argument.gamemode.invalid", {Component::text(name)}),
            .input = reader.string(),
            .cursor = reader.cursor()}};
    }

    [[nodiscard]] ArgumentParser wire() const override { return ArgumentParser{parserId::kGamemode}; }
};

/// `minecraft:dimension` on the wire rather than `brigadier:string`: that is what `/execute in` and
/// `if dimension` declare. A Basalt world is not always a dimension id, so the completions are this
/// server's own world names and the client is told to ask for them rather than offer its three.
class WorldArgument final : public ArgumentType {
public:
    [[nodiscard]] Parsed<ArgumentValue> parse(StringReader& reader, const CommandSource&,
                                              const CommandEnvironment& environment) const override {
        const std::int32_t start = reader.cursor();
        const Parsed<std::string> name = reader.readString();
        if (!name.has_value()) {
            return std::unexpected{name.error()};
        }
        const std::map<std::string, world::LoadedWorld*, std::less<>>& worlds = environment.options().worlds;
        if (const auto found = worlds.find(*name); found != worlds.end()) {
            return ArgumentValue::of(found->second);
        }
        reader.setCursor(start);
        std::string known;
        for (const auto& [worldName, world] : worlds) {
            if (!known.empty()) {
                known += ", ";
            }
            known += worldName;
        }
        return std::unexpected{CommandSyntaxError{
            .component = Component::text("Unknown world '" + *name + "'. Loaded worlds: " + known + "."),
            .input = reader.string(),
            .cursor = reader.cursor()}};
    }

    [[nodiscard]] ArgumentParser wire() const override { return ArgumentParser{parserId::kDimension}; }

    [[nodiscard]] SuggestionProvider suggestions() const override {
        return [](const SuggestionContext& context) {
            std::vector<std::string> names;
            for (const auto& [name, world] : context.environment().options().worlds) {
                names.push_back(name);
            }
            return context.matching(names);
        };
    }

    [[nodiscard]] std::optional<std::string> clientSuggestions() const override {
        return std::string{suggestionProviders::kAskServer};
    }
};

} // namespace

std::shared_ptr<const ArgumentType> wordArgument() {
    static const auto type = std::make_shared<const WordArgument>();
    return type;
}

std::shared_ptr<const ArgumentType> boolArgument() {
    static const auto type = std::make_shared<const BoolArgument>();
    return type;
}

std::shared_ptr<const ArgumentType> doubleArgument(double min, double max) {
    return std::make_shared<const BoundedArgument<double, parserId::kBrigadierDouble>>(min, max, "Double");
}

std::shared_ptr<const ArgumentType> floatArgument(float min, float max) {
    return std::make_shared<const BoundedArgument<float, parserId::kBrigadierFloat>>(min, max, "Float");
}

std::shared_ptr<const ArgumentType> integerArgument(std::int32_t min, std::int32_t max) {
    return std::make_shared<const BoundedArgument<std::int32_t, parserId::kBrigadierInteger>>(min, max, "Integer");
}

std::shared_ptr<const ArgumentType> angleArgument() {
    static const auto type = std::make_shared<const AngleArgument>();
    return type;
}

std::shared_ptr<const ArgumentType> rotationArgument() {
    static const auto type = std::make_shared<const RotationArgument>();
    return type;
}

std::shared_ptr<const ArgumentType> timeArgument(std::int32_t min) { return std::make_shared<const TimeArgument>(min); }

std::shared_ptr<const ArgumentType> greedyStringArgument() {
    static const auto type = std::make_shared<const RestOfLineArgument>(false);
    return type;
}

std::shared_ptr<const ArgumentType> messageArgument() {
    static const auto type = std::make_shared<const RestOfLineArgument>(true);
    return type;
}

std::shared_ptr<const ArgumentType> gameProfileArgument() {
    static const auto type = std::make_shared<const GameProfileArgument>();
    return type;
}

std::shared_ptr<const ArgumentType> gameModeArgument() {
    static const auto type = std::make_shared<const GameModeArgument>();
    return type;
}

std::shared_ptr<const ArgumentType> worldArgument() {
    static const auto type = std::make_shared<const WorldArgument>();
    return type;
}

} // namespace basalt::server::runtime::command
