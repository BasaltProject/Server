#include <basalt/server/runtime/datapack/DataFiles.hpp>
#include <basalt/server/runtime/function/FunctionLibrary.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>
#include <basalt/wire/nbt/Snbt.hpp>

#include <algorithm>
#include <cmath>
#include <format>
#include <utility>

namespace basalt::server::runtime::function {
namespace {

using wire::common::Identifier;

/// parity: `CommandFunction.checkCommandLineLength`.
constexpr std::size_t kMaxLineLength = 2000000;

[[nodiscard]] std::string_view trim(std::string_view line) {
    // parity: `String.trim`, everything up to and including the space.
    while (!line.empty() && static_cast<unsigned char>(line.front()) <= ' ') {
        line.remove_prefix(1);
    }
    while (!line.empty() && static_cast<unsigned char>(line.back()) <= ' ') {
        line.remove_suffix(1);
    }
    return line;
}

[[nodiscard]] std::vector<std::string_view> splitLines(std::string_view text) {
    std::vector<std::string_view> lines;
    std::size_t start = 0;
    while (start < text.size()) {
        const std::size_t end = text.find('\n', start);
        if (end == std::string_view::npos) {
            lines.push_back(text.substr(start));
            break;
        }
        lines.push_back(text.substr(start, end - start));
        start = end + 1;
    }
    return lines;
}

/// parity: `StringTemplate.isValidVariableName`, letters, digits and `_`.
[[nodiscard]] bool validVariable(std::string_view name) {
    return std::ranges::all_of(name, [](char c) {
        const auto byte = static_cast<unsigned char>(c);
        return byte >= 0x80 || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
    });
}

/// parity: `StringTemplate.fromString`, with the variable names left for the caller to index.
[[nodiscard]] std::expected<std::pair<std::vector<std::string>, std::vector<std::string>>, std::string>
templateOf(std::string_view input) {
    std::vector<std::string> segments;
    std::vector<std::string> variables;
    std::size_t start = 0;
    std::size_t index = input.find('$');
    while (index != std::string_view::npos) {
        if (index != input.size() - 1 && input[index + 1] == '(') {
            segments.emplace_back(input.substr(start, index - start));
            const std::size_t end = input.find(')', index + 1);
            if (end == std::string_view::npos) {
                return std::unexpected{std::string{"Unterminated macro variable"}};
            }
            const std::string_view variable = input.substr(index + 2, end - index - 2);
            if (!validVariable(variable)) {
                return std::unexpected{"Invalid macro variable name '" + std::string{variable} + "'"};
            }
            variables.emplace_back(variable);
            start = end + 1;
            index = input.find('$', start);
        } else {
            index = input.find('$', index + 1);
        }
    }
    if (start == 0) {
        return std::unexpected{std::string{"No variables in macro"}};
    }
    if (start != input.size()) {
        segments.emplace_back(input.substr(start));
    }
    return std::pair{std::move(segments), std::move(variables)};
}

/// The first unquoted word, for the "did you mean" of a line that starts with a slash.
[[nodiscard]] std::string_view unquotedWord(std::string_view text) {
    std::size_t end = 0;
    while (end < text.size()) {
        const char c = text[end];
        const bool allowed = (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_' ||
                             c == '-' || c == '.' || c == '+';
        if (!allowed) {
            break;
        }
        ++end;
    }
    return text.substr(0, end);
}

/// parity: `new DecimalFormat("#")` with fifteen fraction digits: no exponent, no trailing zeros.
[[nodiscard]] std::string decimal(double value) {
    if (std::isnan(value)) {
        return "NaN";
    }
    if (std::isinf(value)) {
        return value < 0 ? "-∞" : "∞";
    }
    std::string text = std::format("{:.15f}", value);
    while (text.back() == '0') {
        text.pop_back();
    }
    if (text.back() == '.') {
        text.pop_back();
    }
    return text;
}

} // namespace

std::string MacroTemplate::substitute(const std::vector<std::string>& values) const {
    std::string built;
    for (std::size_t index = 0; index < variables.size(); ++index) {
        built += segments[index];
        built += values[static_cast<std::size_t>(variables[index])];
    }
    if (segments.size() > variables.size()) {
        built += segments.back();
    }
    return built;
}

std::optional<std::vector<std::pair<std::int32_t, std::string>>> joinLines(std::string_view text) {
    const std::vector<std::string_view> raw = splitLines(text);
    std::vector<std::pair<std::int32_t, std::string>> kept;
    for (std::size_t index = 0; index < raw.size(); ++index) {
        const auto number = static_cast<std::int32_t>(index + 1);
        std::string line{trim(raw[index])};
        while (!line.empty() && line.back() == '\\') {
            if (index + 1 == raw.size()) {
                return std::nullopt;
            }
            line.pop_back();
            line += trim(raw[++index]);
        }
        if (line.empty() || line.front() == '#') {
            continue;
        }
        kept.emplace_back(number, std::move(line));
    }
    return kept;
}

std::expected<ServerFunction, std::string> compileFunction(const Identifier& id, std::string_view text,
                                                           const CommandCheck& check) {
    const std::optional<std::vector<std::pair<std::int32_t, std::string>>> lines = joinLines(text);
    if (!lines.has_value()) {
        return std::unexpected{std::string{"Line continuation at end of file"}};
    }
    ServerFunction function{.id = id, .entries = {}, .parameters = {}, .isMacro = false};
    for (const auto& [number, line] : *lines) {
        if (line.size() > kMaxLineLength) {
            return std::unexpected{"Command too long: " + std::to_string(line.size()) +
                                   " characters, contents: " + line.substr(0, 512) + "..."};
        }
        if (line.front() == '/') {
            if (line.size() > 1 && line[1] == '/') {
                return std::unexpected{"Unknown or invalid command '" + line + "' on line " + std::to_string(number) +
                                       " (if you intended to make a comment, use '#' not '//')"};
            }
            return std::unexpected{"Unknown or invalid command '" + line + "' on line " + std::to_string(number) +
                                   " (did you mean '" + std::string{unquotedWord(std::string_view{line}.substr(1))} +
                                   "'? Do not use a preceding forwards slash.)"};
        }
        if (line.front() == '$') {
            auto parsed = templateOf(std::string_view{line}.substr(1));
            if (!parsed.has_value()) {
                return std::unexpected{"Can't parse function line " + std::to_string(number) + ": '" + line.substr(1) +
                                       "'"};
            }
            MacroTemplate macro{.segments = std::move(parsed->first), .variables = {}};
            for (const std::string& variable : parsed->second) {
                const auto at = std::ranges::find(function.parameters, variable);
                macro.variables.push_back(static_cast<std::int32_t>(at - function.parameters.begin()));
                if (at == function.parameters.end()) {
                    function.parameters.push_back(variable);
                }
            }
            function.isMacro = true;
            function.entries.push_back(FunctionEntry{.command = line.substr(1), .macro = std::move(macro)});
            continue;
        }
        if (check) {
            if (std::optional<std::string> error = check(line); error.has_value()) {
                return std::unexpected{"Whilst parsing command on line " + std::to_string(number) + ": " + *error};
            }
        }
        function.entries.push_back(FunctionEntry{.command = line, .macro = std::nullopt});
    }
    return function;
}

std::string macroArgument(const wire::nbt::NbtTag& value) {
    if (const auto* real = value.get<float>(); real != nullptr) {
        return decimal(static_cast<double>(*real));
    }
    if (const auto* real = value.get<double>(); real != nullptr) {
        return decimal(*real);
    }
    if (const auto* whole = value.get<std::int8_t>(); whole != nullptr) {
        return std::to_string(*whole);
    }
    if (const auto* whole = value.get<std::int16_t>(); whole != nullptr) {
        return std::to_string(*whole);
    }
    if (const auto* whole = value.get<std::int32_t>(); whole != nullptr) {
        return std::to_string(*whole);
    }
    if (const auto* whole = value.get<std::int64_t>(); whole != nullptr) {
        return std::to_string(*whole);
    }
    if (const auto* text = value.get<std::string>(); text != nullptr) {
        return *text;
    }
    return wire::nbt::snbt::stringify(value);
}

std::size_t FunctionLibrary::load(const EntriesUnder& entriesUnder, const ReadText& readText,
                                  const CommandCheck& check) {
    functions_.clear();
    tags_.clear();
    if (readText) {
        for (const auto& [id, entry] : datapack::dataFiles(entriesUnder, "function", ".mcfunction")) {
            const std::optional<std::string> text = readText(entry);
            if (!text.has_value()) {
                continue;
            }
            std::expected<ServerFunction, std::string> compiled = compileFunction(id, *text, check);
            if (!compiled.has_value()) {
                logger_.error([&] { return "Failed to load function " + id.toString() + ": " + compiled.error(); });
                continue;
            }
            functions_.insert_or_assign(id, std::move(*compiled));
        }
    }
    tags_ = datapack::resolveTagsOrdered(
        entriesUnder, readText, "function", [this](const Identifier& id) { return functions_.contains(id); }, logger_);
    logger_.info([this] {
        return "Loaded " + std::to_string(functions_.size()) + " function(s) and " + std::to_string(tags_.size()) +
               " function tag(s).";
    });
    return functions_.size();
}

const ServerFunction* FunctionLibrary::get(const Identifier& id) const {
    const auto found = functions_.find(id);
    return found == functions_.end() ? nullptr : &found->second;
}

std::vector<const ServerFunction*> FunctionLibrary::tag(const Identifier& id) const {
    std::vector<const ServerFunction*> members;
    const auto found = tags_.find(id);
    if (found == tags_.end()) {
        return members;
    }
    for (const Identifier& member : found->second) {
        if (const ServerFunction* function = get(member); function != nullptr) {
            members.push_back(function);
        }
    }
    return members;
}

std::vector<const ServerFunction*> FunctionLibrary::all() const {
    std::vector<const ServerFunction*> every;
    for (const auto& [id, function] : functions_) {
        every.push_back(&function);
    }
    return every;
}

std::vector<Identifier> FunctionLibrary::functionIds() const {
    std::vector<Identifier> ids;
    for (const auto& [id, function] : functions_) {
        ids.push_back(id);
    }
    return ids;
}

std::vector<Identifier> FunctionLibrary::tagIds() const {
    std::vector<Identifier> ids;
    for (const auto& [id, members] : tags_) {
        ids.push_back(id);
    }
    return ids;
}

} // namespace basalt::server::runtime::function
