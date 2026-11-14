#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Tags.hpp>
#include <basalt/server/runtime/datapack/DataFiles.hpp>
#include <basalt/server/runtime/world/RealBlockTags.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <expected>
#include <optional>
#include <utility>

namespace basalt::server::runtime::world::realBlockTags {
namespace {

namespace reg = ::basalt::server::registry;

using wire::common::Identifier;

} // namespace

TagMembers load(const JarReader& jar, const wire::common::Logger& logger) {
    return datapack::resolveTags(
        jar.entriesUnder, jar.readText, "block",
        [](const Identifier& id) { return reg::blocks::registry().byName(id) != nullptr; }, logger);
}

namespace {

struct ParsedTag {
    std::string text;
    std::optional<Identifier> id;
};

/// Callers pass the same few literals over and over, so each tag is parsed once per thread.
const std::optional<Identifier>& parsedTag(std::string_view tag) {
    constexpr std::size_t kSlots = 8;
    thread_local std::array<ParsedTag, kSlots> slots;
    thread_local std::size_t used = 0;
    thread_local std::size_t next = 0;
    for (std::size_t index = 0; index < used; ++index) {
        if (slots[index].text == tag) {
            return slots[index].id;
        }
    }
    ParsedTag& slot = slots[next];
    next = (next + 1) % kSlots;
    used = std::max(used, next == 0 ? kSlots : next);
    slot.text = std::string{tag};
    const std::expected<Identifier, wire::common::IdentifierError> parsed = Identifier::parse(tag);
    slot.id = parsed.has_value() ? std::optional<Identifier>{*parsed} : std::nullopt;
    return slot.id;
}

} // namespace

const std::set<Identifier>& membersOf(const TagMembers& tags, std::string_view tag) {
    static const std::set<Identifier> empty;
    const std::optional<Identifier>& id = parsedTag(tag);
    if (!id.has_value()) {
        return empty;
    }
    const auto found = tags.find(*id);
    return found == tags.end() ? empty : found->second;
}

bool contains(const TagMembers& tags, std::string_view tag, const Identifier& block) {
    return membersOf(tags, tag).contains(block);
}

} // namespace basalt::server::runtime::world::realBlockTags
