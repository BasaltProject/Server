#include "DataFix.hpp"

#include <basalt/metamorph/datafix/DataFixer.hpp>

#include <utility>
#include <vector>

namespace basalt::server::runtime::plugins::dataFix {
namespace {

namespace datafix = metamorph::datafix;
using wire::nbt::NbtCompound;
using wire::nbt::NbtList;
using wire::nbt::NbtTag;
using wire::nbt::NbtType;

constexpr std::int32_t kFlatChunkVersion = 2832;
constexpr std::int32_t kFieldRenameVersion = 5006;
constexpr std::int32_t kStackCountVersion = 3837;

[[nodiscard]] NbtTag listOf(NbtCompound element) {
    std::vector<NbtTag> elements;
    elements.push_back(NbtTag::ofCompound(std::move(element)));
    return NbtTag::ofList(NbtList{NbtType::Compound, std::move(elements)});
}

[[nodiscard]] std::optional<NbtCompound> firstOf(const NbtCompound& holder, std::string_view key) {
    const NbtTag* found = holder.find(key);
    const NbtList* list = found == nullptr ? nullptr : found->get<NbtList>();
    if (list == nullptr || list->empty()) {
        return std::nullopt;
    }
    const NbtCompound* first = (*list)[0].get<NbtCompound>();
    return first == nullptr ? std::nullopt : std::optional<NbtCompound>{*first};
}

[[nodiscard]] const NbtCompound* compoundIn(const NbtCompound& holder, std::string_view key) {
    const NbtTag* found = holder.find(key);
    return found == nullptr ? nullptr : found->get<NbtCompound>();
}

} // namespace

std::int32_t currentVersion() { return datafix::kCurrentVersion; }

std::optional<NbtCompound> fixDocument(std::string_view kind, NbtCompound document, std::int32_t fromVersion) {
    if (fromVersion >= datafix::kCurrentVersion) {
        return document;
    }
    if (kind == "chunk") {
        datafix::upgrade(datafix::Domain::Chunk, document, fromVersion);
        return document;
    }
    NbtCompound root;
    if (kind == "entity") {
        root.set("Entities", listOf(std::move(document)));
        datafix::upgrade(datafix::Domain::Entities, root, fromVersion);
        return firstOf(root, "Entities");
    }
    if (kind == "item") {
        root.set("Items", listOf(std::move(document)));
        datafix::upgrade(datafix::Domain::Entities, root, fromVersion);
        return firstOf(root, "Items");
    }
    if (kind == "block_entity") {
        if (fromVersion >= kFlatChunkVersion) {
            root.set("block_entities", listOf(std::move(document)));
        } else {
            NbtCompound level;
            level.set("TileEntities", listOf(std::move(document)));
            root.set("Level", NbtTag::ofCompound(std::move(level)));
        }
        datafix::upgrade(datafix::Domain::Chunk, root, fromVersion);
        if (std::optional<NbtCompound> found = firstOf(root, "block_entities"); found.has_value()) {
            return found;
        }
        const NbtCompound* level = compoundIn(root, "Level");
        return level == nullptr ? std::nullopt : firstOf(*level, "TileEntities");
    }
    return std::nullopt;
}

namespace {

[[nodiscard]] NbtCompound stateEntry(std::string_view text, bool oldFields) {
    NbtCompound entry;
    const std::size_t open = text.find('[');
    entry.putString(oldFields ? "Name" : "id", std::string{text.substr(0, open)});
    if (open == std::string_view::npos) {
        return entry;
    }
    NbtCompound properties;
    std::string_view rest = text.substr(open + 1);
    if (!rest.empty() && rest.back() == ']') {
        rest.remove_suffix(1);
    }
    while (!rest.empty()) {
        const std::size_t comma = rest.find(',');
        const std::string_view pair = rest.substr(0, comma);
        if (const std::size_t equals = pair.find('='); equals != std::string_view::npos) {
            properties.putString(pair.substr(0, equals), std::string{pair.substr(equals + 1)});
        }
        rest = comma == std::string_view::npos ? std::string_view{} : rest.substr(comma + 1);
    }
    entry.set(oldFields ? "Properties" : "properties", NbtTag::ofCompound(std::move(properties)));
    return entry;
}

[[nodiscard]] std::string stateText(const NbtCompound& entry) {
    std::string text{entry.getString(entry.contains("id") ? "id" : "Name")};
    const NbtCompound* properties = compoundIn(entry, entry.contains("properties") ? "properties" : "Properties");
    if (properties == nullptr || properties->empty()) {
        return text;
    }
    text += '[';
    for (std::size_t index = 0; index < properties->names().size(); ++index) {
        if (index > 0) {
            text += ',';
        }
        text += properties->names()[index] + '=' + std::string{properties->getString(properties->names()[index])};
    }
    return text + ']';
}

/// Only the 1.18 chunk layout is fixed: an older version's palette renames need the whole old section to be read.
[[nodiscard]] std::optional<NbtCompound> paletteChunk(NbtCompound entry, std::int32_t fromVersion) {
    NbtCompound states;
    states.set("palette", listOf(std::move(entry)));
    NbtCompound section;
    section.putByte("Y", 0);
    section.set("block_states", NbtTag::ofCompound(std::move(states)));
    NbtCompound root;
    root.set("sections", listOf(std::move(section)));
    datafix::upgrade(datafix::Domain::Chunk, root, fromVersion);
    const std::optional<NbtCompound> fixed = firstOf(root, "sections");
    const NbtCompound* held = fixed.has_value() ? compoundIn(*fixed, "block_states") : nullptr;
    return held == nullptr ? std::nullopt : firstOf(*held, "palette");
}

} // namespace

std::optional<std::string> fixName(std::string_view kind, std::string_view text, std::int32_t fromVersion) {
    if (fromVersion >= datafix::kCurrentVersion) {
        return std::string{text};
    }
    if (kind == "block_state") {
        const std::optional<NbtCompound> fixed =
            paletteChunk(stateEntry(text, fromVersion < kFieldRenameVersion), fromVersion);
        return fixed.has_value() ? std::optional<std::string>{stateText(*fixed)} : std::nullopt;
    }
    if (kind == "item") {
        NbtCompound stack;
        stack.putString("id", std::string{text});
        if (fromVersion < kStackCountVersion) {
            stack.putByte("Count", 1);
        } else {
            stack.putInt("count", 1);
        }
        const std::optional<NbtCompound> fixed = fixDocument("item", std::move(stack), fromVersion);
        return fixed.has_value() ? std::optional<std::string>{std::string{fixed->getString("id")}} : std::nullopt;
    }
    if (kind == "biome") {
        NbtCompound biomes;
        std::vector<NbtTag> names;
        names.push_back(NbtTag::ofString(std::string{text}));
        biomes.set("palette", NbtTag::ofList(NbtList{NbtType::String, std::move(names)}));
        NbtCompound section;
        section.putByte("Y", 0);
        section.set("biomes", NbtTag::ofCompound(std::move(biomes)));
        NbtCompound root;
        root.set("sections", listOf(std::move(section)));
        datafix::upgrade(datafix::Domain::Chunk, root, fromVersion);
        const std::optional<NbtCompound> fixed = firstOf(root, "sections");
        const NbtCompound* held = fixed.has_value() ? compoundIn(*fixed, "biomes") : nullptr;
        const NbtTag* palette = held == nullptr ? nullptr : held->find("palette");
        const NbtList* list = palette == nullptr ? nullptr : palette->get<NbtList>();
        const std::string* name = list == nullptr || list->empty() ? nullptr : (*list)[0].get<std::string>();
        return name == nullptr ? std::nullopt : std::optional<std::string>{*name};
    }
    return std::nullopt;
}

} // namespace basalt::server::runtime::plugins::dataFix
