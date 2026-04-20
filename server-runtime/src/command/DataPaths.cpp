#include <basalt/server/runtime/command/DataPaths.hpp>
#include <basalt/wire/nbt/Snbt.hpp>

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <span>
#include <vector>

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

/// parity: `NbtPathArgument.IndexedElementNode`.
[[nodiscard]] std::optional<std::size_t> indexIn(const NbtList& list, std::int32_t index) {
    const std::int64_t actual = index < 0 ? static_cast<std::int64_t>(list.size()) + index : index;
    if (actual < 0 || actual >= static_cast<std::int64_t>(list.size())) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(actual);
}

struct Write {
    const NbtTag* value = nullptr;
};

[[nodiscard]] bool replaceElement(NbtList& list, std::size_t at, const NbtTag& value) {
    if (list.size() == 1 && value.type() != list.elementType()) {
        list = NbtList{value.type(), {value}};
        return true;
    }
    if (value.type() != list.elementType()) {
        return false;
    }
    list[at] = value;
    return true;
}

std::int32_t apply(NbtTag& node, std::span<const Step> path, const Write& write) {
    const Step& step = path.front();
    const bool last = path.size() == 1;
    std::int32_t changed = 0;
    if (step.kind == Step::Kind::Key) {
        auto* compound = node.get<NbtCompound>();
        if (compound == nullptr) {
            return 0;
        }
        if (last) {
            if (write.value != nullptr) {
                compound->set(step.name, *write.value);
                return 1;
            }
            return compound->remove(step.name) ? 1 : 0;
        }
        if (compound->find(step.name) == nullptr) {
            if (write.value == nullptr) {
                return 0;
            }
            // parity: `createPreferredParentTag`, a list before an index or filter, else a compound.
            const bool listNext = path[1].kind != Step::Kind::Key;
            compound->set(step.name, listNext ? NbtTag::ofList(NbtList{}) : NbtTag::ofCompound(NbtCompound{}));
        }
        return apply(*compound->find(step.name), path.subspan(1), write);
    }
    auto* list = node.get<NbtList>();
    if (list == nullptr) {
        return 0;
    }
    std::vector<std::size_t> hits;
    if (step.kind == Step::Kind::Index) {
        if (const std::optional<std::size_t> at = indexIn(*list, step.index); at.has_value()) {
            hits.push_back(*at);
        }
    } else {
        for (std::size_t at = 0; at < list->size(); ++at) {
            if (matches((*list)[at], step.filter)) {
                hits.push_back(at);
            }
        }
    }
    if (!last) {
        for (const std::size_t at : hits) {
            changed += apply((*list)[at], path.subspan(1), write);
        }
        return changed;
    }
    if (write.value != nullptr) {
        for (const std::size_t at : hits) {
            changed += replaceElement(*list, at, *write.value) ? 1 : 0;
        }
        return changed;
    }
    std::vector<NbtTag> kept;
    for (std::size_t at = 0; at < list->size(); ++at) {
        if (std::ranges::find(hits, at) == hits.end()) {
            kept.push_back((*list)[at]);
        }
    }
    *list = NbtList{list->elementType(), std::move(kept)};
    return static_cast<std::int32_t>(hits.size());
}

[[nodiscard]] std::optional<NbtCompound> written(const NbtCompound& root, std::string_view path, const Write& write) {
    const std::optional<std::vector<Step>> walk = steps(path);
    if (!walk.has_value() || walk->empty()) {
        return std::nullopt;
    }
    NbtTag copy = NbtTag::ofCompound(root);
    if (apply(copy, *walk, write) == 0) {
        return std::nullopt;
    }
    return *copy.get<NbtCompound>();
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
        std::optional<NbtTag> next;
        if (step.kind == Step::Kind::Key) {
            const auto* compound = current.get<NbtCompound>();
            const NbtTag* found = compound == nullptr ? nullptr : compound->find(step.name);
            if (found != nullptr) {
                next = *found;
            }
        } else if (const auto* list = current.get<NbtList>(); list != nullptr) {
            if (step.kind == Step::Kind::Index) {
                if (const std::optional<std::size_t> at = indexIn(*list, step.index); at.has_value()) {
                    next = (*list)[*at];
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
    return written(root, path, Write{.value = &value});
}

std::optional<NbtCompound> remove(const NbtCompound& root, std::string_view path) {
    return written(root, path, Write{});
}

} // namespace basalt::server::runtime::command::dataPaths
