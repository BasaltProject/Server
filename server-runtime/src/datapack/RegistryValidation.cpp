#include <basalt/server/runtime/datapack/DataFiles.hpp>
#include <basalt/server/runtime/datapack/PackRepository.hpp>
#include <basalt/server/runtime/datapack/RegistrySchemas.hpp>
#include <basalt/server/runtime/datapack/RegistryValidation.hpp>
#include <basalt/server/runtime/datapack/ReloadableElements.hpp>
#include <basalt/wire/common/json/Json.hpp>

#include <algorithm>
#include <array>
#include <map>

namespace basalt::server::runtime::datapack {
namespace {

/// Every registry a pack's JSON elements are loaded into. parity: `RELOADABLE_REGISTRIES` and the
/// data-driven part of `WORLD_REGISTRIES`.
constexpr std::array<std::string_view, 58> kRegistries{"loot_table",
                                                       "predicate",
                                                       "item_modifier",
                                                       "context_int_provider",
                                                       "context_float_provider",
                                                       "slot_source",
                                                       "recipe",
                                                       "advancement",
                                                       "damage_type",
                                                       "enchantment",
                                                       "enchantment_provider",
                                                       "dialog",
                                                       "timeline",
                                                       "world_clock",
                                                       "dimension_type",
                                                       "banner_pattern",
                                                       "painting_variant",
                                                       "trim_material",
                                                       "trim_pattern",
                                                       "jukebox_song",
                                                       "instrument",
                                                       "wolf_variant",
                                                       "wolf_sound_variant",
                                                       "pig_variant",
                                                       "pig_sound_variant",
                                                       "cat_variant",
                                                       "cat_sound_variant",
                                                       "cow_variant",
                                                       "cow_sound_variant",
                                                       "chicken_variant",
                                                       "chicken_sound_variant",
                                                       "frog_variant",
                                                       "zombie_nautilus_variant",
                                                       "villager_trade",
                                                       "trade_set",
                                                       "test_instance",
                                                       "test_environment",
                                                       "chat_type",
                                                       "decorated_pot_pattern",
                                                       "block_transformer",
                                                       "worldgen/biome",
                                                       "trial_spawner",
                                                       "sulfur_cube_archetype",
                                                       "worldgen/block_state_provider",
                                                       "worldgen/carver",
                                                       "worldgen/configured_feature",
                                                       "worldgen/placed_feature",
                                                       "worldgen/structure",
                                                       "worldgen/structure_set",
                                                       "worldgen/processor_list",
                                                       "worldgen/template_pool",
                                                       "worldgen/noise_settings",
                                                       "worldgen/noise",
                                                       "worldgen/density_function",
                                                       "worldgen/world_preset",
                                                       "worldgen/flat_level_generator_preset",
                                                       "worldgen/multi_noise_biome_source_parameter_list",
                                                       "dimension"};

/// The first eight are `RELOADABLE_REGISTRIES`; the rest are only read at a start.
constexpr std::size_t kReloadable = 8;

} // namespace

std::vector<std::string> builtInPackIds(const PackRepository& repository) {
    std::vector<std::string> ids;
    for (const Pack* pack : repository.availablePacks()) {
        if (pack->source != PackSource::World) {
            ids.push_back(pack->id);
        }
    }
    return ids;
}

std::vector<ElementError> invalidElements(const ResourceStack& resources, const std::vector<std::string>& builtInPacks,
                                          bool worldRegistries) {
    std::vector<ElementError> errors;
    std::size_t index = 0;
    const EntriesUnder entriesUnder = [&resources](std::string_view prefix) { return resources.entriesUnder(prefix); };
    std::optional<ItemTags> itemTags;
    const auto tagsFor = [&]() -> const ItemTags& {
        if (!itemTags.has_value()) {
            itemTags = itemTagsOf(resources);
        }
        return *itemTags;
    };
    for (const std::string_view registry : kRegistries) {
        if (index++ >= kReloadable && !worldRegistries) {
            break;
        }
        for (const auto& [id, name] : dataFiles(entriesUnder, registry)) {
            const std::optional<std::string> pack = resources.packOf(name);
            if (!pack.has_value() || std::ranges::contains(builtInPacks, *pack)) {
                continue;
            }
            const std::optional<std::string> text = resources.readText(name);
            const auto json = text.has_value() ? wire::common::parseJson(*text)
                                               : std::expected<wire::common::JsonValue, wire::common::JsonError>{
                                                     std::unexpected{wire::common::JsonError::SyntaxError}};
            if (!json.has_value()) {
                errors.push_back(ElementError{.registry = "minecraft:" + std::string{registry},
                                              .element = id.toString(),
                                              .message = "Failed to parse " + id.toString() + " from pack " + *pack});
            } else if (json->get<wire::common::JsonObject>() == nullptr) {
                errors.push_back(
                    ElementError{.registry = "minecraft:" + std::string{registry},
                                 .element = id.toString(),
                                 .message = "Not a JSON object: " + id.toString() + " from pack " + *pack});
            } else if (const std::optional<std::string> schema =
                           index <= kReloadable ? reloadableElementError(registry, *text, resources, tagsFor())
                                                : schemaError(registry, *json->get<wire::common::JsonObject>());
                       schema.has_value()) {
                errors.push_back(ElementError{.registry = "minecraft:" + std::string{registry},
                                              .element = id.toString(),
                                              .message = *schema + " in " + id.toString() + " from pack " + *pack});
            }
        }
    }
    return errors;
}

void logElementErrors(const std::vector<ElementError>& errors, const wire::common::Logger& logger) {
    std::map<std::string, std::map<std::string, std::string>> byRegistry;
    for (const ElementError& error : errors) {
        byRegistry[error.registry][error.element] = error.message;
    }
    std::string text = "Registry loading errors:\n";
    for (const auto& [registry, elements] : byRegistry) {
        text += "> Errors in registry " + registry + ":\n";
        for (const auto& [element, message] : elements) {
            text += ">> Errors in element " + element + ":\n" + message + "\n";
        }
    }
    logger.error([&text] { return text; });
}

} // namespace basalt::server::runtime::datapack
