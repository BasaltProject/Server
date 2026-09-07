#include <basalt/server/runtime/function/FunctionLibrary.hpp>
#include <basalt/wire/io/FileIO.hpp>

#include <algorithm>
#include <cstdint>
#include <utility>

namespace basalt::server::runtime::function {
namespace {

using wire::common::Identifier;

constexpr std::string_view kFunctionSuffix = ".mcfunction";

[[nodiscard]] std::string_view trim(std::string_view line) {
    // parity: `String.trim`, which cuts at everything up to and including the space, not at what
    // the current locale happens to call whitespace.
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
    // The final newline terminates the last line rather than starting an empty one, which is what
    // reading a file line by line gives and what decides whether a trailing backslash has anything
    // left to join to.
    while (start < text.size()) {
        const std::size_t end = text.find('\n', start);
        if (end == std::string_view::npos) {
            lines.push_back(text.substr(start));
            break;
        }
        // A carriage return from a pack authored on Windows needs no handling of its own: it is
        // below the space, so the trim takes it with the rest.
        lines.push_back(text.substr(start, end - start));
        start = end + 1;
    }
    return lines;
}

[[nodiscard]] bool continues(std::string_view line) { return !line.empty() && line.back() == '\\'; }

} // namespace

std::optional<std::vector<std::string>> parseFunction(std::string_view text) {
    const std::vector<std::string_view> raw = splitLines(text);
    std::vector<std::string> kept;
    for (std::size_t index = 0; index < raw.size(); ++index) {
        std::string line{trim(raw[index])};
        while (continues(line)) {
            if (index + 1 == raw.size()) {
                // parity: vanilla refuses the whole file rather than running the half it understood.
                return std::nullopt;
            }
            line.pop_back();
            line += trim(raw[++index]);
        }
        if (line.empty() || line.front() == '#') {
            continue;
        }
        kept.push_back(std::move(line));
    }
    return kept;
}

std::vector<std::string> FunctionLibrary::enabledPacks() const {
    std::vector<std::string> enabled;
    for (const std::string& pack : order_) {
        if (isEnabled(pack)) {
            enabled.push_back(pack);
        }
    }
    return enabled;
}

const ServerFunction* FunctionLibrary::get(const Identifier& id) const {
    const auto at = functions_.find(id);
    if (at == functions_.end() || disabledPacks_.contains(at->second.pack)) {
        return nullptr;
    }
    return &at->second;
}

std::vector<const ServerFunction*> FunctionLibrary::all() const {
    std::vector<const ServerFunction*> every;
    for (const std::pair<const Identifier, ServerFunction>& one : functions_) {
        if (!disabledPacks_.contains(one.second.pack)) {
            every.push_back(&one.second);
        }
    }
    return every;
}

bool FunctionLibrary::isEnabled(std::string_view pack) const {
    return std::ranges::find(availablePacks_, pack) != availablePacks_.end() && !disabledPacks_.contains(pack);
}

bool FunctionLibrary::setEnabled(std::string_view pack, bool enabled, const Position& position) {
    if (std::ranges::find(availablePacks_, pack) == availablePacks_.end()) {
        return false;
    }
    bool changed = false;
    if (enabled) {
        changed = disabledPacks_.erase(std::string{pack}) != 0;
        place(pack, position);
    } else {
        changed = disabledPacks_.emplace(pack).second;
    }
    return changed;
}

void FunctionLibrary::place(std::string_view pack, const Position& position) {
    std::erase(order_, pack);
    const auto indexOf = [this](std::string_view other) {
        const auto at = std::ranges::find(order_, other);
        return at == order_.end() ? -1 : static_cast<std::int64_t>(at - order_.begin());
    };
    switch (position.kind) {
    case Position::Kind::First:
        order_.insert(order_.begin(), std::string{pack});
        return;
    case Position::Kind::Before:
        order_.insert(order_.begin() + std::max<std::int64_t>(indexOf(position.other), 0), std::string{pack});
        return;
    case Position::Kind::After:
        // The pack was just taken out of the order, so the index of anything still in it is below
        // the size and the clamp never bites. It stays because a `place` that walked off the end
        // would be a crash, not a misplacement.
        order_.insert(order_.begin() +
                          std::min<std::int64_t>(indexOf(position.other) + 1, static_cast<std::int64_t>(order_.size())),
                      std::string{pack});
        return;
    case Position::Kind::Last:
    default:
        order_.emplace_back(pack);
        return;
    }
}

bool FunctionLibrary::create(const std::string& pack, const std::string& description) {
    const std::string directory = root_ + "/" + pack;
    if (wire::io::fileIO::isDirectory(directory)) {
        return false;
    }
    wire::io::fileIO::createDirectories(directory + "/data");
    wire::io::fileIO::writeText(directory + "/pack.mcmeta",
                                R"({"pack":{"pack_format":0,"description":")" + description + R"("}})");
    static_cast<void>(reload());
    return true;
}

std::size_t FunctionLibrary::reload() {
    functions_.clear();
    availablePacks_.clear();
    if (wire::io::fileIO::isDirectory(root_)) {
        availablePacks_ = wire::io::fileIO::list(root_);
        std::ranges::sort(availablePacks_);
    }
    for (const std::string& pack : availablePacks_) {
        if (std::ranges::find(order_, pack) == order_.end()) {
            order_.push_back(pack);
        }
    }
    for (const std::string& pack : availablePacks_) {
        // Listing something that is not a directory answers nothing, so this only saves the walk.
        const std::string dataRoot = root_ + "/" + pack + "/data";
        if (!wire::io::fileIO::isDirectory(dataRoot)) {
            continue;
        }
        for (const std::string& nameSpace : wire::io::fileIO::list(dataRoot)) {
            const std::string functionRoot = dataRoot + "/" + nameSpace + "/function";
            if (!wire::io::fileIO::isDirectory(functionRoot)) {
                continue;
            }
            loadDirectory(pack, nameSpace, functionRoot, "");
        }
    }
    logger_.info([this] {
        return "Loaded " + std::to_string(functions_.size()) + " function(s) from " +
               std::to_string(availablePacks_.size()) + " datapack(s).";
    });
    return functions_.size();
}

void FunctionLibrary::loadDirectory(const std::string& pack, const std::string& nameSpace, const std::string& directory,
                                    const std::string& prefix) {
    for (const std::string& entry : wire::io::fileIO::list(directory)) {
        const std::string path = directory + "/" + entry;
        if (wire::io::fileIO::isDirectory(path)) {
            loadDirectory(pack, nameSpace, path, prefix + entry + "/");
            continue;
        }
        if (!std::string_view{entry}.ends_with(kFunctionSuffix)) {
            continue;
        }
        const std::optional<std::string> text = wire::io::fileIO::readText(path);
        if (!text.has_value()) {
            continue;
        }
        std::optional<std::vector<std::string>> lines = parseFunction(*text);
        if (!lines.has_value()) {
            logger_.warn([&path] { return "Line continuation at end of file in " + path + ", skipped."; });
            continue;
        }
        Identifier id{nameSpace, prefix + entry.substr(0, entry.size() - kFunctionSuffix.size())};
        functions_.insert_or_assign(id, ServerFunction{.id = id, .lines = std::move(*lines), .pack = pack});
    }
}

} // namespace basalt::server::runtime::function
