#include <basalt/metamorph/datafix/DataFixer.hpp>
#include <basalt/server/plugins/PluginDatafix.hpp>
#include <basalt/wire/nbt/NbtIO.hpp>

#include <algorithm>
#include <array>
#include <map>
#include <mutex>

namespace basalt::server::plugins::datafix {
namespace {

namespace fixer = metamorph::datafix;

constexpr std::string_view kSavedPrefix = "saved_data:";

constexpr std::array<std::string_view, 9> kSavedData{"random_sequences",
                                                     "raids",
                                                     "map",
                                                     "map_index",
                                                     "scoreboard",
                                                     "tickets",
                                                     "world_border",
                                                     "custom_boss_events",
                                                     "structure_feature_indices"};

struct Target {
    fixer::Domain domain = fixer::Domain::Chunk;
    std::string savedData;
};

[[nodiscard]] std::optional<Target> targetOf(std::string_view type) {
    if (type.starts_with(kSavedPrefix) && type.size() > kSavedPrefix.size()) {
        return Target{fixer::Domain::SavedData, std::string{type.substr(kSavedPrefix.size())}};
    }
    if (std::ranges::find(kSavedData, type) != kSavedData.end()) {
        return Target{fixer::Domain::SavedData, std::string{type}};
    }
    fixer::Domain domain{};
    if (fixer::domainFromName(type, domain) && domain != fixer::Domain::SavedData) {
        return Target{domain, {}};
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<wire::nbt::NbtCompound> readRoot(std::span<const std::uint8_t> nbt) {
    const std::expected<wire::nbt::AnonymousTag, wire::nbt::NbtError> read = wire::nbt::nbtIO::readAnonymous(nbt);
    const wire::nbt::NbtCompound* root = read.has_value() ? read->tag.get<wire::nbt::NbtCompound>() : nullptr;
    return root == nullptr ? std::nullopt : std::optional<wire::nbt::NbtCompound>{*root};
}

[[nodiscard]] Outcome written(wire::nbt::NbtCompound root) {
    return {wire::nbt::nbtIO::writeAnonymous(wire::nbt::NbtTag::ofCompound(std::move(root))), {}};
}

/// The root a saved data file keeps under `data`, wrapped the way the fixer expects it.
[[nodiscard]] wire::nbt::NbtCompound containerOf(const Target& target, wire::nbt::NbtCompound data) {
    if (target.domain != fixer::Domain::SavedData) {
        return data;
    }
    wire::nbt::NbtCompound container;
    container.set("data", wire::nbt::NbtTag::ofCompound(std::move(data)));
    return container;
}

[[nodiscard]] wire::nbt::NbtCompound dataOf(const Target& target, wire::nbt::NbtCompound container) {
    if (target.domain != fixer::Domain::SavedData) {
        return container;
    }
    const wire::nbt::NbtTag* held = container.find("data");
    const wire::nbt::NbtCompound* data = held == nullptr ? nullptr : held->get<wire::nbt::NbtCompound>();
    return data == nullptr ? wire::nbt::NbtCompound{} : *data;
}

} // namespace

bool supports(std::string_view type) { return targetOf(type).has_value(); }

std::vector<std::string> types() {
    std::vector<std::string> all;
    for (std::uint8_t index = 0; index <= static_cast<std::uint8_t>(fixer::Domain::Advancements); ++index) {
        const std::string_view name = fixer::domainName(static_cast<fixer::Domain>(index));
        if (name != "saved_data") {
            all.emplace_back(name);
        }
    }
    all.insert(all.end(), kSavedData.begin(), kSavedData.end());
    return all;
}

Outcome upgrade(std::string_view type, std::span<const std::uint8_t> nbt, std::int32_t fromVersion,
                std::int32_t toVersion) {
    const std::optional<Target> target = targetOf(type);
    if (!target.has_value()) {
        return {std::nullopt, "unknown data type " + std::string{type}};
    }
    std::optional<wire::nbt::NbtCompound> root = readRoot(nbt);
    if (!root.has_value()) {
        return {std::nullopt, "the NBT is not a compound"};
    }
    wire::nbt::NbtCompound container = containerOf(*target, std::move(*root));
    fixer::Context context;
    context.savedData = target->savedData;
    if (std::string refusal = fixer::upgrade(target->domain, container, fromVersion, context, toVersion);
        !refusal.empty()) {
        return {std::nullopt, std::move(refusal)};
    }
    return written(dataOf(*target, std::move(container)));
}

Outcome applyStep(std::string_view type, std::string_view name, std::int32_t versionKey,
                  std::span<const std::uint8_t> nbt) {
    const std::optional<Target> target = targetOf(type);
    if (!target.has_value()) {
        return {std::nullopt, "unknown data type " + std::string{type}};
    }
    std::optional<wire::nbt::NbtCompound> root = readRoot(nbt);
    if (!root.has_value()) {
        return {std::nullopt, "the NBT is not a compound"};
    }
    wire::nbt::NbtCompound container = containerOf(*target, std::move(*root));
    fixer::Context context;
    context.savedData = target->savedData;
    std::string failure;
    context.failure = &failure;
    if (!fixer::applyStep(name, versionKey, target->domain, container, context)) {
        return {std::nullopt,
                "no step " + std::string{name} + " at " + std::to_string(versionKey) + " for " + std::string{type}};
    }
    if (!failure.empty()) {
        return {std::nullopt, std::move(failure)};
    }
    return written(dataOf(*target, std::move(container)));
}

std::string missing(std::string_view type, std::int32_t fromVersion, std::int32_t toVersion) {
    const std::optional<Target> target = targetOf(type);
    return target.has_value() ? fixer::missingStep(target->domain, fromVersion, toVersion, target->savedData)
                              : "unknown data type " + std::string{type};
}

std::vector<Step> steps(std::string_view type) {
    std::vector<Step> found;
    const std::optional<Target> target = targetOf(type);
    if (!target.has_value()) {
        return found;
    }
    for (const fixer::StepInfo& step : fixer::steps()) {
        if ((step.domains & (1U << static_cast<std::uint32_t>(target->domain))) != 0) {
            found.push_back({step.version, step.subVersion, std::string{step.name}});
        }
    }
    return found;
}

metamorph::datafix::LegacyBlockState legacyState(std::int32_t id) { return fixer::legacyBlockState(id); }

metamorph::datafix::LegacyBlockState modernState(std::int32_t id) {
    static std::mutex lock;
    static std::map<std::int32_t, fixer::LegacyBlockState> cache;
    const std::scoped_lock guard{lock};
    if (const auto found = cache.find(id); found != cache.end()) {
        return found->second;
    }
    using wire::nbt::NbtCompound;
    using wire::nbt::NbtTag;
    std::vector<std::int8_t> blocks(4096, 0);
    std::vector<std::int8_t> data(2048, 0);
    blocks[0] = static_cast<std::int8_t>((id >> 4) & 0xFF);
    data[0] = static_cast<std::int8_t>(id & 0xF);
    NbtCompound section;
    section.putByte("Y", 0);
    section.putByteArray("Blocks", std::move(blocks));
    section.putByteArray("Data", std::move(data));
    wire::nbt::NbtList sections{wire::nbt::NbtType::Compound};
    sections.push(NbtTag::ofCompound(std::move(section)));
    NbtCompound level;
    level.putInt("xPos", 0);
    level.putInt("zPos", 0);
    level.set("Sections", NbtTag::ofList(std::move(sections)));
    NbtCompound chunk;
    chunk.putInt("DataVersion", 1343);
    chunk.set("Level", NbtTag::ofCompound(std::move(level)));
    fixer::LegacyBlockState state = fixer::legacyBlockState(id);
    if (fixer::upgrade(fixer::Domain::Chunk, chunk, 1343).empty()) {
        const wire::nbt::NbtList* list = chunk.getList("sections");
        for (std::size_t at = 0; list != nullptr && at < list->size(); ++at) {
            const NbtCompound* section = (*list)[at].get<NbtCompound>();
            const NbtTag* y = section == nullptr ? nullptr : section->find("Y");
            const bool origin = y != nullptr && ((y->get<std::int8_t>() != nullptr && *y->get<std::int8_t>() == 0) ||
                                                 (y->get<std::int32_t>() != nullptr && *y->get<std::int32_t>() == 0));
            if (!origin) {
                continue;
            }
            const NbtCompound* states = section->getCompound("block_states");
            const wire::nbt::NbtList* palette = states == nullptr ? nullptr : states->getList("palette");
            for (std::size_t entryAt = 0; palette != nullptr && entryAt < palette->size(); ++entryAt) {
                const NbtCompound* entry = (*palette)[entryAt].get<NbtCompound>();
                if (entry == nullptr || (entryAt + 1 < palette->size() && entry->getString("id") == "minecraft:air")) {
                    continue;
                }
                state.name = std::string{entry->getString("id")};
                state.properties.clear();
                const NbtCompound* properties = entry->getCompound("properties");
                for (std::size_t index = 0; properties != nullptr && index < properties->size(); ++index) {
                    if (const std::string* value = properties->values()[index].get<std::string>(); value != nullptr) {
                        state.properties[properties->names()[index]] = *value;
                    }
                }
                break;
            }
            break;
        }
    }
    cache.emplace(id, state);
    return state;
}

} // namespace basalt::server::plugins::datafix
