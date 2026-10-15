#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/world/BlockTransformers.hpp>
#include <basalt/wire/common/json/Json.hpp>

#include <array>
#include <expected>
#include <utility>
#include <variant>

namespace basalt::server::runtime::world {

using wire::common::Identifier;
using wire::common::JsonObject;
using wire::common::JsonValue;

namespace {

/// parity: `BlockPredicate`'s `matching_blocks`.
struct Predicate {
    enum class Kind : std::uint8_t { Blocks, Tag, AllOf, AnyOf, Not, True, Unknown };
    Kind kind = Kind::Unknown;
    std::array<std::int32_t, 3> offset{};
    std::vector<Identifier> blocks;
    std::string tag;
    std::vector<Predicate> children;
};

} // namespace

/// parity: `RuleBasedStateProvider` and the two providers it hands out: a fixed state, or.
struct RealBlockTransformers::Provider {
    enum class Kind : std::uint8_t { Simple, CopyProperties, RuleBased, Unknown };
    Kind kind = Kind::Unknown;
    std::optional<std::int32_t> state;
    std::vector<std::pair<Predicate, std::shared_ptr<const Provider>>> rules;
    std::shared_ptr<const Provider> fallback;
};

namespace {

using Provider = RealBlockTransformers::Provider;

[[nodiscard]] const JsonObject* objectOf(const JsonValue* value) {
    return value == nullptr ? nullptr : value->get<JsonObject>();
}

[[nodiscard]] std::optional<std::string> textOf(const JsonValue* value) {
    const std::string* text = value == nullptr ? nullptr : value->get<std::string>();
    return text == nullptr ? std::nullopt : std::optional{*text};
}

[[nodiscard]] std::vector<Identifier> identifiersOf(const JsonValue* value) {
    std::vector<Identifier> ids;
    const auto add = [&ids](const std::string& text) {
        if (const auto id = Identifier::parse(text); id.has_value()) {
            ids.push_back(*id);
        }
    };
    if (const std::optional<std::string> one = textOf(value); one.has_value()) {
        add(*one);
    } else if (const auto* list = value == nullptr ? nullptr : value->get<std::vector<JsonValue>>(); list != nullptr) {
        for (const JsonValue& each : *list) {
            if (const std::optional<std::string> text = textOf(&each); text.has_value()) {
                add(*text);
            }
        }
    }
    return ids;
}

[[nodiscard]] std::optional<std::int32_t> stateOf(const JsonObject& object) {
    const std::optional<std::string> name = textOf(object.find("id"));
    const auto id = name.has_value() ? Identifier::parse(*name) : std::unexpected(wire::common::IdentifierError{});
    if (!id.has_value()) {
        return std::nullopt;
    }
    std::map<std::string, std::string> properties;
    if (const JsonObject* written = objectOf(object.find("properties")); written != nullptr) {
        for (std::size_t index = 0; index < written->names().size(); ++index) {
            if (const std::optional<std::string> value = textOf(&written->values()[index]); value.has_value()) {
                properties.emplace(written->names()[index], *value);
            }
        }
    }
    return registry::blockStateDefinitions::stateIdOf(*id, properties, std::nullopt);
}

[[nodiscard]] Predicate predicateOf(const JsonValue* value) {
    Predicate predicate;
    const JsonObject* object = objectOf(value);
    if (object == nullptr) {
        return predicate;
    }
    if (const auto* offset =
            object->find("offset") == nullptr ? nullptr : object->find("offset")->get<std::vector<JsonValue>>();
        offset != nullptr && offset->size() == 3) {
        for (std::size_t axis = 0; axis < 3; ++axis) {
            const std::int64_t* whole = (*offset)[axis].get<std::int64_t>();
            predicate.offset[axis] = whole == nullptr ? 0 : static_cast<std::int32_t>(*whole);
        }
    }
    const std::string type = textOf(object->find("type")).value_or("");
    if (type == "minecraft:matching_blocks") {
        predicate.kind = Predicate::Kind::Blocks;
        predicate.blocks = identifiersOf(object->find("blocks"));
    } else if (type == "minecraft:matching_block_tag") {
        predicate.kind = Predicate::Kind::Tag;
        predicate.tag = textOf(object->find("tag")).value_or("");
    } else if (type == "minecraft:all_of" || type == "minecraft:any_of") {
        predicate.kind = type == "minecraft:all_of" ? Predicate::Kind::AllOf : Predicate::Kind::AnyOf;
        if (const auto* list = object->find("predicates") == nullptr
                                   ? nullptr
                                   : object->find("predicates")->get<std::vector<JsonValue>>();
            list != nullptr) {
            for (const JsonValue& each : *list) {
                predicate.children.push_back(predicateOf(&each));
            }
        }
    } else if (type == "minecraft:not") {
        predicate.kind = Predicate::Kind::Not;
        predicate.children.push_back(predicateOf(object->find("predicate")));
    } else if (type == "minecraft:true") {
        predicate.kind = Predicate::Kind::True;
    }
    return predicate;
}

[[nodiscard]] std::shared_ptr<const Provider> providerOf(const JsonValue* value) {
    auto provider = std::make_shared<Provider>();
    const JsonObject* object = objectOf(value);
    if (object == nullptr) {
        return provider;
    }
    const std::optional<std::string> type = textOf(object->find("type"));
    if (!type.has_value() || *type == "minecraft:simple_state_provider") {
        const JsonObject* state = type.has_value() ? objectOf(object->find("state")) : object;
        provider->kind = Provider::Kind::Simple;
        provider->state = state == nullptr ? std::nullopt : stateOf(*state);
    } else if (*type == "minecraft:copy_properties") {
        const JsonObject* source = objectOf(object->find("source"));
        provider->kind = Provider::Kind::CopyProperties;
        provider->state = source == nullptr ? std::nullopt : stateOf(*source);
    } else if (*type == "minecraft:rule_based") {
        provider->kind = Provider::Kind::RuleBased;
        if (const auto* rules =
                object->find("rules") == nullptr ? nullptr : object->find("rules")->get<std::vector<JsonValue>>();
            rules != nullptr) {
            for (const JsonValue& each : *rules) {
                const JsonObject* rule = objectOf(&each);
                if (rule == nullptr) {
                    continue;
                }
                provider->rules.emplace_back(predicateOf(rule->find("if_true")), providerOf(rule->find("then")));
            }
        }
        if (object->find("fallback") != nullptr) {
            provider->fallback = providerOf(object->find("fallback"));
        }
    }
    return provider;
}

[[nodiscard]] bool test(const Predicate& predicate, const WorldState& world, std::int32_t x, std::int32_t y,
                        std::int32_t z, const RealBlockTransformers::Tagged& tagged) {
    const std::int32_t atX = x + predicate.offset[0];
    const std::int32_t atY = y + predicate.offset[1];
    const std::int32_t atZ = z + predicate.offset[2];
    const auto blockAt = [&]() -> const Identifier* {
        const std::optional<std::int32_t> state = world.blockState(atX, atY, atZ);
        return state.has_value() ? registry::blockStateDefinitions::blockOf(*state) : nullptr;
    };
    switch (predicate.kind) {
    case Predicate::Kind::Blocks: {
        const Identifier* block = blockAt();
        return block != nullptr && std::ranges::find(predicate.blocks, *block) != predicate.blocks.end();
    }
    case Predicate::Kind::Tag: {
        const Identifier* block = blockAt();
        return block != nullptr && tagged && tagged(predicate.tag, *block);
    }
    case Predicate::Kind::AllOf:
        return std::ranges::all_of(predicate.children,
                                   [&](const Predicate& child) { return test(child, world, atX, atY, atZ, tagged); });
    case Predicate::Kind::AnyOf:
        return std::ranges::any_of(predicate.children,
                                   [&](const Predicate& child) { return test(child, world, atX, atY, atZ, tagged); });
    case Predicate::Kind::Not:
        return !predicate.children.empty() && !test(predicate.children.front(), world, atX, atY, atZ, tagged);
    case Predicate::Kind::True:
        return true;
    case Predicate::Kind::Unknown:
        break;
    }
    return false;
}

[[nodiscard]] std::optional<std::int32_t> apply(const Provider& provider, const WorldState& world, std::int32_t x,
                                                std::int32_t y, std::int32_t z,
                                                const RealBlockTransformers::Tagged& tagged) {
    switch (provider.kind) {
    case Provider::Kind::Simple:
        return provider.state;
    case Provider::Kind::CopyProperties: {
        // parity: `CopyPropertiesProvider`, every property the two blocks share carried across.
        const std::optional<std::int32_t> here = world.blockState(x, y, z);
        if (!provider.state.has_value() || !here.has_value()) {
            return provider.state;
        }
        const Identifier* block = registry::blockStateDefinitions::blockOf(*provider.state);
        if (block == nullptr) {
            return provider.state;
        }
        return registry::blockStateDefinitions::stateIdOf(*block, registry::blockStateDefinitions::propertiesOf(*here),
                                                          *provider.state)
            .value_or(*provider.state);
    }
    case Provider::Kind::RuleBased:
        for (const auto& [predicate, then] : provider.rules) {
            if (test(predicate, world, x, y, z, tagged)) {
                return then == nullptr ? std::nullopt : apply(*then, world, x, y, z, tagged);
            }
        }
        return provider.fallback == nullptr ? std::nullopt : apply(*provider.fallback, world, x, y, z, tagged);
    case Provider::Kind::Unknown:
        break;
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<wire::protocol::play::Direction> directionOf(std::string_view name) {
    using wire::protocol::play::Direction;
    static constexpr std::array<std::pair<std::string_view, Direction>, 6> kNames{{{"down", Direction::Down},
                                                                                   {"up", Direction::Up},
                                                                                   {"north", Direction::North},
                                                                                   {"south", Direction::South},
                                                                                   {"west", Direction::West},
                                                                                   {"east", Direction::East}}};
    for (const auto& [text, direction] : kNames) {
        if (text == name) {
            return direction;
        }
    }
    return std::nullopt;
}

} // namespace

std::vector<RealBlockTransformers::Transform> RealBlockTransformers::parse(std::string_view json) {
    std::vector<Transform> transforms;
    const std::expected<JsonValue, wire::common::JsonError> parsed = wire::common::parseJson(json);
    const auto* list = parsed.has_value() ? parsed->get<std::vector<JsonValue>>() : nullptr;
    if (list == nullptr) {
        return transforms;
    }
    for (const JsonValue& each : *list) {
        const JsonObject* object = objectOf(&each);
        if (object == nullptr) {
            continue;
        }
        Transform transform;
        transform.provider = providerOf(object->find("block_state_provider"));
        transform.sound = textOf(object->find("sound")).value_or("");
        const std::string particle = textOf(object->find("particle")).value_or("none");
        transform.particle = particle == "scrape"    ? Particle::Scrape
                             : particle == "wax_on"  ? Particle::WaxOn
                             : particle == "wax_off" ? Particle::WaxOff
                                                     : Particle::None;
        if (const auto* faces = object->find("disallowed_faces") == nullptr
                                    ? nullptr
                                    : object->find("disallowed_faces")->get<std::vector<JsonValue>>();
            faces != nullptr) {
            for (const JsonValue& face : *faces) {
                if (const auto direction = directionOf(textOf(&face).value_or("")); direction.has_value()) {
                    transform.disallowedFaces.push_back(*direction);
                }
            }
        }
        if (const std::optional<std::string> loot = textOf(object->find("loot")); loot.has_value()) {
            if (const auto id = Identifier::parse(*loot); id.has_value()) {
                transform.loot = *id;
            }
        }
        transform.drop = textOf(object->find("drop_strategy")) == std::optional<std::string>{"clicked_face"}
                             ? Drop::ClickedFace
                             : Drop::FromMiddle;
        const auto flag = [object](std::string_view key, bool fallback) {
            const JsonValue* value = object->find(key);
            const bool* set = value == nullptr ? nullptr : value->get<bool>();
            return set == nullptr ? fallback : *set;
        };
        transform.updateFromNeighbors = flag("update_from_neighbors", true);
        transform.consumeOnUse = flag("consume_on_use", true);
        transform.copperChest = textOf(object->find("transform_type")) == std::optional<std::string>{"copper_chest"};
        if (const JsonValue* damage = object->find("item_damage_per_use"); damage != nullptr) {
            if (const std::int64_t* whole = damage->get<std::int64_t>(); whole != nullptr) {
                transform.itemDamagePerUse = static_cast<std::int32_t>(*whole);
            }
        }
        transforms.push_back(std::move(transform));
    }
    return transforms;
}

const std::vector<RealBlockTransformers::Transform>& RealBlockTransformers::of(const Identifier& id) {
    const std::lock_guard<std::mutex> guard(mutex_);
    if (const auto cached = cache_.find(id); cached != cache_.end()) {
        return cached->second;
    }
    const std::optional<std::string> text =
        readText_ ? readText_("data/" + id.namespaceName() + "/block_transformer/" + id.path() + ".json")
                  : std::nullopt;
    return cache_.emplace(id, text.has_value() ? parse(*text) : std::vector<Transform>{}).first->second;
}

std::optional<std::int32_t> RealBlockTransformers::stateFor(const Transform& transform, const WorldState& world,
                                                            std::int32_t x, std::int32_t y, std::int32_t z,
                                                            const Tagged& tagged) {
    return transform.provider == nullptr ? std::nullopt : apply(*transform.provider, world, x, y, z, tagged);
}

std::optional<Identifier> RealBlockTransformers::defaultFor(const Identifier& item) {
    const std::string_view path = item.path();
    if (path.ends_with("_axe")) {
        return Identifier::vanilla("axe");
    }
    if (path.ends_with("_hoe")) {
        return Identifier::vanilla("hoe");
    }
    if (path.ends_with("_shovel")) {
        return Identifier::vanilla("shovel");
    }
    return std::nullopt;
}

} // namespace basalt::server::runtime::world
