#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/item/ItemComponentCodecs.hpp>
#include <basalt/server/runtime/item/ItemComponentPayloads.hpp>
#include <basalt/server/runtime/registry/VanillaRegistryData.hpp>
#include <basalt/wire/io/ByteBuffer.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <ranges>
#include <span>
#include <string>
#include <vector>

namespace basalt::server::runtime::item::itemComponentPayloads {
namespace {

namespace reg = server::registry;

using wire::common::Identifier;
using wire::io::ByteBuffer;
using wire::nbt::NbtCompound;
using wire::nbt::NbtList;
using wire::nbt::NbtTag;
using wire::protocol::play::ItemComponent;

constexpr float kDefaultMaxReach = 3.0F;
constexpr float kDefaultCreativeReach = 5.0F;
constexpr std::int32_t kDefaultSwingDuration = 6;

constexpr std::array<std::string_view, 16> kDyeColours{"white", "orange", "magenta",    "light_blue", "yellow", "lime",
                                                       "pink",  "gray",   "light_gray", "cyan",       "purple", "blue",
                                                       "brown", "green",  "red",        "black"};

constexpr std::array<std::string_view, 3> kSwingAnimations{"none", "whack", "stab"};

constexpr std::array<std::string_view, 2> kMapPostProcessing{"lock", "scale"};

/// The components vanilla declares `persistent` and never `networkSynchronized`.
constexpr std::array<std::string_view, 6> kPersistentOnly{"minecraft:intangible_projectile",
                                                          "minecraft:map_decorations",
                                                          "minecraft:debug_stick_state",
                                                          "minecraft:recipes",
                                                          "minecraft:lock",
                                                          "minecraft:container_loot"};

using Encoder = std::function<bool(ByteBuffer&, const NbtTag&)>;

/// parity: `FireworkExplosion.Shape`, whose id is its place in this list.
constexpr std::array<std::string_view, 5> kFireworkShapes{"small_ball", "large_ball", "star", "creeper", "burst"};

/// parity: `FireworkExplosion.COLOR_LIST_STREAM_CODEC`.
bool writeColours(ByteBuffer& buffer, const NbtCompound& explosion, std::string_view field) {
    const NbtTag* colours = explosion.find(field);
    std::vector<std::int32_t> values;
    if (colours != nullptr) {
        if (const auto* ints = colours->get<std::vector<std::int32_t>>(); ints != nullptr) {
            values = *ints;
        } else if (const auto* list = colours->get<NbtList>(); list != nullptr) {
            for (const NbtTag& each : *list) {
                const auto* colour = each.get<std::int32_t>();
                if (colour == nullptr) {
                    return false;
                }
                values.push_back(*colour);
            }
        } else {
            return false;
        }
    }
    wire::protocol::varInt::write(buffer, static_cast<std::int32_t>(values.size()));
    for (const std::int32_t colour : values) {
        buffer.writeInt(colour);
    }
    return true;
}

/// parity: `FireworkExplosion.STREAM_CODEC`.
bool writeExplosion(ByteBuffer& buffer, const NbtTag& value) {
    const NbtCompound* explosion = value.get<NbtCompound>();
    if (explosion == nullptr) {
        return false;
    }
    const auto shape = std::ranges::find(kFireworkShapes, explosion->getString("shape"));
    if (shape == kFireworkShapes.end()) {
        return false;
    }
    wire::protocol::varInt::write(buffer, static_cast<std::int32_t>(shape - kFireworkShapes.begin()));
    if (!writeColours(buffer, *explosion, "colors") || !writeColours(buffer, *explosion, "fade_colors")) {
        return false;
    }
    wire::protocol::writeBoolean(buffer, explosion->getBoolean("has_trail"));
    wire::protocol::writeBoolean(buffer, explosion->getBoolean("has_twinkle"));
    return true;
}

[[nodiscard]] std::optional<std::int32_t> componentId(std::string_view name) {
    const std::expected<Identifier, wire::common::IdentifierError> parsed = Identifier::parse(name);
    if (!parsed.has_value()) {
        return std::nullopt;
    }
    return reg::dataComponents::idOf(*parsed);
}

[[nodiscard]] std::optional<float> numberOf(const NbtTag* value) {
    if (value == nullptr) {
        return std::nullopt;
    }
    if (const float* asFloat = value->get<float>(); asFloat != nullptr) {
        return *asFloat;
    }
    if (const double* asDouble = value->get<double>(); asDouble != nullptr) {
        return static_cast<float>(*asDouble);
    }
    if (const std::int32_t* asInt = value->get<std::int32_t>(); asInt != nullptr) {
        return static_cast<float>(*asInt);
    }
    if (const std::int8_t* asByte = value->get<std::int8_t>(); asByte != nullptr) {
        return static_cast<float>(*asByte);
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<float> floatField(const NbtTag& value, std::string_view field) {
    const NbtCompound* compound = value.get<NbtCompound>();
    return compound == nullptr ? std::nullopt : numberOf(compound->find(field));
}

[[nodiscard]] std::optional<std::int32_t> intField(const NbtTag& value, std::string_view field) {
    const std::optional<float> number = floatField(value, field);
    return number.has_value() ? std::optional{static_cast<std::int32_t>(*number)} : std::nullopt;
}

[[nodiscard]] std::optional<bool> boolField(const NbtTag& value, std::string_view field) {
    const std::optional<float> number = floatField(value, field);
    return number.has_value() ? std::optional{*number != 0.0F} : std::nullopt;
}

[[nodiscard]] std::string_view stringOf(const NbtTag* value) {
    if (value == nullptr) {
        return {};
    }
    const std::string* text = value->get<std::string>();
    return text == nullptr ? std::string_view{} : std::string_view{*text};
}

/// parity: `ByteBufCodecs.holderSet`.
[[nodiscard]] bool writeHolderSet(ByteBuffer& buffer, const NbtTag& value,
                                  const std::function<std::optional<std::int32_t>(const Identifier&)>& idOf) {
    if (const std::string* text = value.get<std::string>(); text != nullptr) {
        if (text->starts_with('#')) {
            wire::protocol::varInt::write(buffer, 0);
            wire::protocol::writeString(buffer, std::string_view{*text}.substr(1));
            return true;
        }
        const std::expected<Identifier, wire::common::IdentifierError> parsed = Identifier::parse(*text);
        if (!parsed.has_value()) {
            return false;
        }
        const std::optional<std::int32_t> id = idOf(*parsed);
        if (!id.has_value()) {
            return false;
        }
        wire::protocol::varInt::write(buffer, 2);
        wire::protocol::varInt::write(buffer, *id);
        return true;
    }
    const NbtList* list = value.get<NbtList>();
    if (list == nullptr) {
        return false;
    }
    std::vector<std::int32_t> ids;
    ids.reserve(list->size());
    for (const NbtTag& element : *list) {
        const std::string* name = element.get<std::string>();
        if (name == nullptr) {
            return false;
        }
        const std::expected<Identifier, wire::common::IdentifierError> parsed = Identifier::parse(*name);
        if (!parsed.has_value()) {
            return false;
        }
        const std::optional<std::int32_t> id = idOf(*parsed);
        if (!id.has_value()) {
            return false;
        }
        ids.push_back(*id);
    }
    wire::protocol::varInt::write(buffer, static_cast<std::int32_t>(ids.size()) + 1);
    for (const std::int32_t id : ids) {
        wire::protocol::varInt::write(buffer, id);
    }
    return true;
}

[[nodiscard]] Encoder floatEncoder() {
    return [](ByteBuffer& buffer, const NbtTag& value) {
        const std::optional<float> number = numberOf(&value);
        if (!number.has_value()) {
            return false;
        }
        buffer.writeFloat(*number);
        return true;
    };
}

[[nodiscard]] Encoder varIntEncoder() {
    return [](ByteBuffer& buffer, const NbtTag& value) {
        const std::optional<float> number = numberOf(&value);
        if (!number.has_value()) {
            return false;
        }
        wire::protocol::varInt::write(buffer, static_cast<std::int32_t>(*number));
        return true;
    };
}

[[nodiscard]] Encoder nbtEncoder() {
    return [](ByteBuffer& buffer, const NbtTag& value) {
        wire::protocol::writeAnonymousNbt(buffer, value);
        return true;
    };
}

[[nodiscard]] Encoder identifierEncoder() {
    return [](ByteBuffer& buffer, const NbtTag& value) {
        const std::string* text = value.get<std::string>();
        if (text == nullptr) {
            return false;
        }
        wire::protocol::writeString(buffer, *text);
        return true;
    };
}

[[nodiscard]] Encoder enumEncoder(std::span<const std::string_view> names) {
    return [names](ByteBuffer& buffer, const NbtTag& value) {
        const std::string* text = value.get<std::string>();
        if (text == nullptr) {
            return false;
        }
        const auto found = std::ranges::find(names, *text);
        if (found == names.end()) {
            return false;
        }
        wire::protocol::varInt::write(buffer, static_cast<std::int32_t>(found - names.begin()));
        return true;
    };
}

[[nodiscard]] std::optional<std::int32_t> itemProtocolId(const Identifier& item) {
    const reg::Item* entry = reg::items::registry().byName(item);
    return entry == nullptr ? std::nullopt : std::optional{entry->protocolId};
}

/// parity: `ItemStackTemplate.STREAM_CODEC`.
void writeStackTemplate(ByteBuffer& buffer, std::int32_t item, std::int32_t count, const NbtCompound* components) {
    wire::protocol::varInt::write(buffer, item);
    wire::protocol::varInt::write(buffer, count);
    const std::vector<ItemComponent> patch =
        components == nullptr
            ? std::vector<ItemComponent>{}
            : itemComponentCodecs::toWire(*components, [](const Identifier&) { return std::optional<std::int32_t>{}; });
    wire::protocol::varInt::write(buffer, static_cast<std::int32_t>(patch.size()));
    wire::protocol::varInt::write(buffer, 0);
    for (const ItemComponent& component : patch) {
        wire::protocol::varInt::write(buffer, component.id());
        component.writePayload(buffer);
    }
}

[[nodiscard]] const std::map<std::string, Encoder, std::less<>>& encoders() {
    static const std::map<std::string, Encoder, std::less<>> built = [] {
        std::map<std::string, Encoder, std::less<>> table;
        // parity: `ByteBufCodecs.FLOAT`, `VAR_INT` and `Identifier.STREAM_CODEC`, one value each.
        table.emplace("minecraft:minimum_attack_charge", floatEncoder());
        table.emplace("minecraft:potion_duration_scale", floatEncoder());
        table.emplace("minecraft:additional_trade_cost", varIntEncoder());
        table.emplace("minecraft:map_id", varIntEncoder());
        table.emplace("minecraft:ominous_bottle_amplifier", varIntEncoder());
        table.emplace("minecraft:item_model", identifierEncoder());
        table.emplace("minecraft:tooltip_style", identifierEncoder());
        table.emplace("minecraft:note_block_sound", identifierEncoder());
        table.emplace("minecraft:glider", [](ByteBuffer&, const NbtTag&) { return true; });
        table.emplace("minecraft:firework_explosion", writeExplosion);
        // parity: `ChargedProjectiles.STREAM_CODEC`.
        table.emplace("minecraft:charged_projectiles", [](ByteBuffer& buffer, const NbtTag& value) {
            const NbtList* stacks = value.get<NbtList>();
            if (stacks == nullptr) {
                return false;
            }
            wire::protocol::varInt::write(buffer, static_cast<std::int32_t>(stacks->size()));
            for (const NbtTag& each : *stacks) {
                const NbtCompound* stack = each.get<NbtCompound>();
                const std::expected<Identifier, wire::common::IdentifierError> id =
                    stack == nullptr ? std::unexpected(wire::common::IdentifierError{})
                                     : Identifier::parse(stack->getString("id"));
                const reg::Item* item = id.has_value() ? reg::items::registry().byName(*id) : nullptr;
                if (item == nullptr) {
                    return false;
                }
                writeStackTemplate(buffer, item->protocolId, stack->getInt("count", 1),
                                   stack->getCompound("components"));
            }
            return true;
        });
        // parity: `Fireworks.STREAM_CODEC`, the flight as a varint and then the explosions.
        table.emplace("minecraft:fireworks", [](ByteBuffer& buffer, const NbtTag& value) {
            const NbtCompound* fireworks = value.get<NbtCompound>();
            if (fireworks == nullptr) {
                return false;
            }
            wire::protocol::varInt::write(buffer, fireworks->getByte("flight_duration") & 0xFF);
            const NbtList* explosions = fireworks->getList("explosions");
            wire::protocol::varInt::write(buffer,
                                          explosions == nullptr ? 0 : static_cast<std::int32_t>(explosions->size()));
            if (explosions != nullptr) {
                for (const NbtTag& explosion : *explosions) {
                    if (!writeExplosion(buffer, explosion)) {
                        return false;
                    }
                }
            }
            return true;
        });

        // parity: `BannerPatternLayers.STREAM_CODEC`.
        table.emplace("minecraft:banner_patterns", [](ByteBuffer& buffer, const NbtTag& value) {
            const NbtList* layers = value.get<NbtList>();
            if (layers == nullptr) {
                return false;
            }
            wire::protocol::varInt::write(buffer, static_cast<std::int32_t>(layers->size()));
            for (const NbtTag& element : *layers) {
                const NbtCompound* layer = element.get<NbtCompound>();
                if (layer == nullptr) {
                    return false;
                }
                const std::string_view full = layer->getString("pattern");
                const std::size_t colon = full.find(':');
                const std::string_view pattern = colon == std::string_view::npos ? full : full.substr(colon + 1);
                const std::vector<std::string>& patterns = runtime::registry::vanillaRegistryData::bannerPatterns();
                const auto foundPattern = std::ranges::find(patterns, pattern);
                const auto foundColour = std::ranges::find(kDyeColours, layer->getString("color"));
                if (foundPattern == patterns.end() || foundColour == kDyeColours.end()) {
                    return false;
                }
                wire::protocol::varInt::write(buffer, static_cast<std::int32_t>(foundPattern - patterns.begin()) + 1);
                wire::protocol::varInt::write(buffer, static_cast<std::int32_t>(foundColour - kDyeColours.begin()));
            }
            return true;
        });
        table.emplace("minecraft:tooltip_display", [](ByteBuffer& buffer, const NbtTag& value) {
            const NbtCompound* display = value.get<NbtCompound>();
            if (display == nullptr) {
                return false;
            }
            wire::protocol::writeBoolean(buffer, boolField(value, "hide_tooltip").value_or(false));
            const NbtList* hidden = display->getList("hidden_components");
            wire::protocol::varInt::write(buffer, hidden == nullptr ? 0 : static_cast<std::int32_t>(hidden->size()));
            if (hidden == nullptr) {
                return true;
            }
            for (const NbtTag& element : *hidden) {
                const std::string* name = element.get<std::string>();
                if (name == nullptr) {
                    return false;
                }
                const std::optional<std::int32_t> component = componentId(*name);
                if (!component.has_value()) {
                    return false;
                }
                wire::protocol::varInt::write(buffer, *component);
            }
            return true;
        });

        // parity: the `idMapper` enums, written as their own ordinal.
        table.emplace("minecraft:dye", enumEncoder(kDyeColours));
        table.emplace("minecraft:base_color", enumEncoder(kDyeColours));
        table.emplace("minecraft:map_post_processing", enumEncoder(kMapPostProcessing));

        // parity: `ByteBufCodecs.fromCodecWithRegistries`.
        for (const std::string_view name : kPersistentOnly) {
            table.emplace(std::string{name}, nbtEncoder());
        }

        table.emplace("minecraft:enchantable", [](ByteBuffer& buffer, const NbtTag& value) {
            const std::optional<std::int32_t> level = intField(value, "value");
            if (!level.has_value()) {
                return false;
            }
            wire::protocol::varInt::write(buffer, *level);
            return true;
        });
        // parity: `Weapon`, whose two fields default to 1 and 0.
        table.emplace("minecraft:weapon", [](ByteBuffer& buffer, const NbtTag& value) {
            if (value.get<NbtCompound>() == nullptr) {
                return false;
            }
            wire::protocol::varInt::write(buffer, intField(value, "item_damage_per_attack").value_or(1));
            buffer.writeFloat(floatField(value, "disable_blocking_for_seconds").value_or(0.0F));
            return true;
        });
        // parity: `AttackRange`, six floats with vanilla's own defaults.
        table.emplace("minecraft:attack_range", [](ByteBuffer& buffer, const NbtTag& value) {
            if (value.get<NbtCompound>() == nullptr) {
                return false;
            }
            buffer.writeFloat(floatField(value, "min_reach").value_or(0.0F));
            buffer.writeFloat(floatField(value, "max_reach").value_or(kDefaultMaxReach));
            buffer.writeFloat(floatField(value, "min_creative_reach").value_or(0.0F));
            buffer.writeFloat(floatField(value, "max_creative_reach").value_or(kDefaultCreativeReach));
            buffer.writeFloat(floatField(value, "hitbox_margin").value_or(0.0F));
            buffer.writeFloat(floatField(value, "mob_factor").value_or(1.0F));
            return true;
        });
        table.emplace("minecraft:food", [](ByteBuffer& buffer, const NbtTag& value) {
            if (value.get<NbtCompound>() == nullptr) {
                return false;
            }
            const std::optional<std::int32_t> nutrition = intField(value, "nutrition");
            const std::optional<float> saturation = floatField(value, "saturation");
            if (!nutrition.has_value() || !saturation.has_value()) {
                return false;
            }
            wire::protocol::varInt::write(buffer, *nutrition);
            buffer.writeFloat(*saturation);
            wire::protocol::writeBoolean(buffer, boolField(value, "can_always_eat").value_or(false));
            return true;
        });
        table.emplace("minecraft:use_cooldown", [](ByteBuffer& buffer, const NbtTag& value) {
            const NbtCompound* compound = value.get<NbtCompound>();
            if (compound == nullptr) {
                return false;
            }
            const std::optional<float> seconds = floatField(value, "seconds");
            if (!seconds.has_value()) {
                return false;
            }
            buffer.writeFloat(*seconds);
            const NbtTag* group = compound->find("cooldown_group");
            const std::string* name = group == nullptr ? nullptr : group->get<std::string>();
            wire::protocol::writeBoolean(buffer, name != nullptr);
            if (name != nullptr) {
                wire::protocol::writeString(buffer, *name);
            }
            return true;
        });
        // parity: 26.3 split `swing_animation` into these two, with the same shape.
        const auto swing = [](ByteBuffer& buffer, const NbtTag& value) {
            const NbtCompound* compound = value.get<NbtCompound>();
            if (compound == nullptr) {
                return false;
            }
            const NbtTag* typeTag = compound->find("type");
            const std::string_view type = typeTag == nullptr ? std::string_view{"whack"} : stringOf(typeTag);
            const auto found = std::ranges::find(kSwingAnimations, type);
            if (found == kSwingAnimations.end()) {
                return false;
            }
            wire::protocol::varInt::write(buffer, static_cast<std::int32_t>(found - kSwingAnimations.begin()));
            wire::protocol::varInt::write(buffer, intField(value, "duration").value_or(kDefaultSwingDuration));
            return true;
        };
        table.emplace("minecraft:attack_animation", swing);
        table.emplace("minecraft:interact_animation", swing);
        // parity: `PotDecorations.STREAM_CODEC`.
        table.emplace("minecraft:pot_decorations", [](ByteBuffer& buffer, const NbtTag& value) {
            const NbtCompound* compound = value.get<NbtCompound>();
            if (compound == nullptr) {
                return false;
            }
            for (const std::string_view side : {"back", "left", "right", "front"}) {
                const NbtTag* entry = compound->find(side);
                wire::protocol::writeBoolean(buffer, entry != nullptr);
                if (entry == nullptr) {
                    continue;
                }
                const NbtCompound* stack = entry->get<NbtCompound>();
                const std::string_view name = stack == nullptr ? stringOf(entry) : stringOf(stack->find("id"));
                const std::expected<Identifier, wire::common::IdentifierError> item = Identifier::parse(name);
                const std::optional<std::int32_t> id = item.has_value() ? itemProtocolId(*item) : std::nullopt;
                if (!id.has_value()) {
                    return false;
                }
                writeStackTemplate(buffer, *id, stack == nullptr ? 1 : intField(*entry, "count").value_or(1),
                                   stack == nullptr ? nullptr : stack->getCompound("components"));
            }
            return true;
        });
        table.emplace("minecraft:repairable", [](ByteBuffer& buffer, const NbtTag& value) {
            const NbtCompound* compound = value.get<NbtCompound>();
            if (compound == nullptr) {
                return false;
            }
            const NbtTag* items = compound->find("items");
            return items != nullptr && writeHolderSet(buffer, *items, itemProtocolId);
        });
        // parity: `Tool`, a list of rules and three defaults.
        table.emplace("minecraft:tool", [](ByteBuffer& buffer, const NbtTag& value) {
            const NbtCompound* compound = value.get<NbtCompound>();
            if (compound == nullptr) {
                return false;
            }
            const NbtList* rules = compound->getList("rules");
            if (rules == nullptr) {
                return false;
            }
            wire::protocol::varInt::write(buffer, static_cast<std::int32_t>(rules->size()));
            for (const NbtTag& element : *rules) {
                const NbtCompound* rule = element.get<NbtCompound>();
                if (rule == nullptr) {
                    return false;
                }
                const NbtTag* blocks = rule->find("blocks");
                if (blocks == nullptr || !writeHolderSet(buffer, *blocks, reg::blocks::blockIdOf)) {
                    return false;
                }
                const std::optional<float> speed = floatField(element, "speed");
                wire::protocol::writeBoolean(buffer, speed.has_value());
                if (speed.has_value()) {
                    buffer.writeFloat(*speed);
                }
                const std::optional<bool> correct = boolField(element, "correct_for_drops");
                wire::protocol::writeBoolean(buffer, correct.has_value());
                if (correct.has_value()) {
                    wire::protocol::writeBoolean(buffer, *correct);
                }
            }
            buffer.writeFloat(floatField(value, "default_mining_speed").value_or(1.0F));
            wire::protocol::varInt::write(buffer, intField(value, "damage_per_block").value_or(1));
            wire::protocol::writeBoolean(buffer, boolField(value, "can_destroy_blocks_in_creative").value_or(true));
            return true;
        });
        return table;
    }();
    return built;
}

} // namespace

const std::set<std::string, std::less<>>& supported() {
    static const std::set<std::string, std::less<>> names = [] {
        std::set<std::string, std::less<>> built;
        for (const auto& [name, encoder] : encoders()) {
            built.insert(name);
        }
        return built;
    }();
    return names;
}

std::optional<ItemComponent> encode(std::string_view name, const NbtTag& value) {
    const auto encoder = encoders().find(name);
    if (encoder == encoders().end()) {
        return std::nullopt;
    }
    const std::optional<std::int32_t> id = componentId(name);
    if (!id.has_value()) {
        return std::nullopt;
    }
    ByteBuffer buffer;
    if (!encoder->second(buffer, value)) {
        return std::nullopt;
    }
    const std::span<const std::uint8_t> written = buffer.readable();
    return ItemComponent{*id, ItemComponent::Raw{.payload = std::vector<std::uint8_t>{written.begin(), written.end()}}};
}

} // namespace basalt::server::runtime::item::itemComponentPayloads
