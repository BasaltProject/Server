#include <basalt/server/runtime/command/DataPaths.hpp>
#include <basalt/wire/nbt/Snbt.hpp>

#include <algorithm>
#include <charconv>

namespace basalt::server::runtime::command::dataPaths {
namespace {

using wire::nbt::NbtCompound;
using wire::nbt::NbtList;
using wire::nbt::NbtTag;

/// Splits on dots that are not inside a `{...}` filter, so `a[{id:"x.y"}].b` stays in one piece.
[[nodiscard]] std::vector<std::string> splitOutsideBraces(std::string_view path) {
    std::vector<std::string> parts;
    std::string current;
    std::int32_t depth = 0;
    for (const char character : path) {
        if (character == '{') {
            depth++;
            current.push_back(character);
        } else if (character == '}') {
            depth--;
            current.push_back(character);
        } else if (character == '.' && depth == 0) {
            parts.push_back(current);
            current.clear();
        } else {
            current.push_back(character);
        }
    }
    parts.push_back(current);
    return parts;
}

[[nodiscard]] std::optional<std::int32_t> toInt(std::string_view text) {
    std::int32_t value = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size()) {
        return std::nullopt;
    }
    return value;
}

/// parity: a compound filter matches when every key it names matches, recursively.
[[nodiscard]] bool matches(const NbtTag& candidate, const NbtCompound& filter) {
    const auto* compound = candidate.get<NbtCompound>();
    if (compound == nullptr) {
        return false;
    }
    for (const std::string& key : filter.names()) {
        const NbtTag* wanted = filter.find(key);
        const NbtTag* actual = compound->find(key);
        if (wanted == nullptr || actual == nullptr) {
            return false;
        }
        const auto* wantedCompound = wanted->get<NbtCompound>();
        const auto* actualCompound = actual->get<NbtCompound>();
        if (wantedCompound != nullptr && actualCompound != nullptr) {
            if (!matches(*actual, *wantedCompound)) {
                return false;
            }
        } else if (!(*wanted == *actual)) {
            return false;
        }
    }
    return true;
}

/// Walks a chain of keys, copying each compound on the way so the original is left alone. Answers
/// the cursor and the last key, or nothing when a step is not a key or is missing.
struct Descent {
    NbtCompound root;
    std::string lastKey;
};

[[nodiscard]] std::optional<Descent> descend(const NbtCompound& root, const std::vector<Step>& steps, bool creating) {
    if (steps.empty()) {
        return std::nullopt;
    }
    Descent walked{.root = merge(root, NbtCompound{}), .lastKey = {}};
    NbtCompound* cursor = &walked.root;
    for (std::size_t index = 0; index < steps.size(); index++) {
        if (steps[index].kind != Step::Kind::Key) {
            return std::nullopt;
        }
        if (index + 1 == steps.size()) {
            walked.lastKey = steps[index].name;
            return walked;
        }
        const NbtTag* next = cursor->find(steps[index].name);
        const auto* asCompound = next == nullptr ? nullptr : next->get<NbtCompound>();
        if (asCompound == nullptr && !creating) {
            return std::nullopt;
        }
        cursor->set(steps[index].name,
                    NbtTag::ofCompound(asCompound == nullptr ? NbtCompound{} : merge(*asCompound, NbtCompound{})));
        cursor = cursor->find(steps[index].name)->get<NbtCompound>();
    }
    return std::nullopt;
}

} // namespace

std::optional<std::vector<Step>> steps(std::string_view path) {
    std::vector<Step> built;
    for (const std::string& part : splitOutsideBraces(path)) {
        if (part.empty()) {
            continue;
        }
        const std::size_t bracket = part.find('[');
        if (bracket == std::string::npos) {
            built.push_back(Step{.kind = Step::Kind::Key, .name = part});
            continue;
        }
        if (bracket > 0) {
            built.push_back(Step{.kind = Step::Kind::Key, .name = part.substr(0, bracket)});
        }
        std::string rest = part.substr(bracket);
        while (rest.starts_with('[')) {
            const std::size_t close = rest.rfind(']');
            if (close == std::string::npos) {
                return std::nullopt;
            }
            const std::string inner = rest.substr(1, close - 1);
            if (inner.starts_with('{')) {
                const std::expected<NbtTag, wire::nbt::NbtError> filter = wire::nbt::snbt::parse(inner);
                const auto* compound = filter.has_value() ? filter->get<NbtCompound>() : nullptr;
                if (compound == nullptr) {
                    return std::nullopt;
                }
                built.push_back(Step{.kind = Step::Kind::Filter, .filter = *compound});
            } else {
                const std::optional<std::int32_t> index = toInt(inner);
                if (!index.has_value()) {
                    return std::nullopt;
                }
                built.push_back(Step{.kind = Step::Kind::Index, .index = *index});
            }
            rest = rest.substr(close + 1);
        }
    }
    return built;
}

NbtCompound merge(const NbtCompound& base, const NbtCompound& incoming) {
    NbtCompound result;
    for (const std::string& key : base.names()) {
        if (const NbtTag* value = base.find(key); value != nullptr) {
            result.set(key, *value);
        }
    }
    for (const std::string& key : incoming.names()) {
        const NbtTag* value = incoming.find(key);
        if (value == nullptr) {
            continue;
        }
        const NbtTag* existing = result.find(key);
        const auto* existingCompound = existing == nullptr ? nullptr : existing->get<NbtCompound>();
        const auto* valueCompound = value->get<NbtCompound>();
        if (existingCompound != nullptr && valueCompound != nullptr) {
            result.set(key, NbtTag::ofCompound(merge(*existingCompound, *valueCompound)));
        } else {
            result.set(key, *value);
        }
    }
    return result;
}

std::optional<NbtTag> resolve(const NbtTag& root, std::string_view path) {
    const std::optional<std::vector<Step>> walk = steps(path);
    if (!walk.has_value()) {
        return std::nullopt;
    }
    NbtTag current = root;
    for (const Step& step : *walk) {
        // Copied out before `current` is reassigned: the tag being read lives inside `current`'s own
        // storage, and assigning over it first would destroy what is being copied.
        std::optional<NbtTag> next;
        if (step.kind == Step::Kind::Key) {
            const auto* compound = current.get<NbtCompound>();
            const NbtTag* found = compound == nullptr ? nullptr : compound->find(step.name);
            if (found != nullptr) {
                next = *found;
            }
        } else if (const auto* list = current.get<NbtList>(); list != nullptr) {
            if (step.kind == Step::Kind::Index) {
                if (step.index >= 0 && static_cast<std::size_t>(step.index) < list->size()) {
                    next = (*list)[static_cast<std::size_t>(step.index)];
                }
            } else {
                const auto found = std::ranges::find_if(
                    list->elements(), [&step](const NbtTag& candidate) { return matches(candidate, step.filter); });
                if (found != list->elements().end()) {
                    next = *found;
                }
            }
        }
        if (!next.has_value()) {
            return std::nullopt;
        }
        current = std::move(*next);
    }
    return current;
}

std::optional<NbtCompound> set(const NbtCompound& root, std::string_view path, const NbtTag& value) {
    const std::optional<std::vector<Step>> walk = steps(path);
    if (!walk.has_value()) {
        return std::nullopt;
    }
    std::optional<Descent> descended = descend(root, *walk, true);
    if (!descended.has_value()) {
        return std::nullopt;
    }
    // Walked again on the copy, since the cursor cannot be carried out of `descend` safely.
    NbtCompound* cursor = &descended->root;
    for (std::size_t index = 0; index + 1 < walk->size(); index++) {
        cursor = cursor->find((*walk)[index].name)->get<NbtCompound>();
    }
    cursor->set(descended->lastKey, value);
    return descended->root;
}

std::optional<NbtCompound> remove(const NbtCompound& root, std::string_view path) {
    const std::optional<std::vector<Step>> walk = steps(path);
    if (!walk.has_value()) {
        return std::nullopt;
    }
    std::optional<Descent> descended = descend(root, *walk, false);
    if (!descended.has_value()) {
        return std::nullopt;
    }
    NbtCompound* cursor = &descended->root;
    for (std::size_t index = 0; index + 1 < walk->size(); index++) {
        cursor = cursor->find((*walk)[index].name)->get<NbtCompound>();
    }
    if (!cursor->remove(descended->lastKey)) {
        return std::nullopt;
    }
    return descended->root;
}

} // namespace basalt::server::runtime::command::dataPaths
