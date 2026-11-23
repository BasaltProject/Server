#include "WasmEntities.hpp"

#include <format>

namespace basalt::server::plugins::wasm {
namespace entities {
namespace {
constexpr std::string_view kVanilla = "minecraft:";
constexpr std::string_view kPlayerType = "minecraft:player";
} // namespace

std::string withNamespaceId(std::string_view id) {
    return id.find(':') == std::string_view::npos ? std::string{kVanilla} + std::string{id} : std::string{id};
}

bool Target::alive() const { return entity != nullptr ? entity->alive() : player != nullptr && player->online(); }

bool Target::isPlayer() const { return player != nullptr || (entity != nullptr && entity->type() == kPlayerType); }

std::int32_t Target::id() const { return entity != nullptr ? entity->id() : player->entityId(); }

std::string Target::type() const { return entity != nullptr ? entity->type() : std::string{kPlayerType}; }

std::string Target::world() const { return entity != nullptr ? entity->world() : player->world(); }

std::string Target::uuid() const { return entity != nullptr ? entity->uuid() : player->uuid(); }

std::array<double, 3> Target::position() const {
    return entity != nullptr ? std::array<double, 3>{entity->x(), entity->y(), entity->z()}
                             : std::array<double, 3>{player->x(), player->y(), player->z()};
}

std::int32_t Table::add(Target target) {
    std::int32_t handle = slots_.add(target);
    if (handle == 0) {
        sweep();
        handle = slots_.add(target);
    }
    if (handle == 0) {
        return abi::kErrInvalid;
    }
    byKey_[target.key] = handle;
    return handle;
}

void Table::sweep() {
    std::vector<std::int32_t> dead;
    slots_.forEach([&dead](std::int32_t handle, Target& target) {
        if (!target.alive()) {
            dead.push_back(handle);
        }
    });
    for (const std::int32_t handle : dead) {
        static_cast<void>(release(handle));
    }
}

bool Table::release(std::int32_t handle) {
    Target* found = nullptr;
    if (slots_.find(handle, found) != abi::kOk) {
        return false;
    }
    byKey_.erase(found->key);
    return slots_.remove(handle);
}

std::int32_t Table::handleFor(const std::shared_ptr<PluginEntityRef>& entity) {
    const std::string key = "e:" + entity->world() + ":" + std::to_string(entity->id());
    if (const auto known = byKey_.find(key); known != byKey_.end()) {
        Target* found = nullptr;
        if (slots_.find(known->second, found) == abi::kOk) {
            return known->second;
        }
        byKey_.erase(known);
    }
    return add(Target{entity, nullptr, key});
}

std::int32_t Table::handleFor(const std::shared_ptr<PluginPlayerRef>& player) {
    const std::string key = "p:" + player->uuid();
    if (const auto known = byKey_.find(key); known != byKey_.end()) {
        Target* found = nullptr;
        if (slots_.find(known->second, found) == abi::kOk) {
            return known->second;
        }
        byKey_.erase(known);
    }
    return add(Target{nullptr, player, key});
}

std::int32_t Table::resolve(std::int32_t handle, Target*& out) {
    Target* found = nullptr;
    const std::int32_t status = slots_.find(handle, found);
    if (status != abi::kOk) {
        return status;
    }
    if (!found->alive()) {
        return abi::kErrStale;
    }
    out = found;
    return abi::kOk;
}

std::shared_ptr<PluginPlayerRef> playerOf(State& state, const Target& target) {
    if (target.player != nullptr || target.entity == nullptr || !target.isPlayer()) {
        return target.player;
    }
    std::shared_ptr<PluginPlayerRef> found = state.host->services().players().byUuid(target.entity->uuid());
    return found != nullptr && found->online() ? found : nullptr;
}

std::shared_ptr<PluginWorldRef> worldNamed(State& state, std::string_view name) {
    return state.host->services().worlds().byName(name);
}

Value pointValue(const std::array<double, 3>& point) {
    return Value{Value::List{Value{point[0]}, Value{point[1]}, Value{point[2]}}};
}

namespace {
[[nodiscard]] std::string textOf(const Value& value) {
    if (const std::string* text = value.asString()) {
        return *text;
    }
    if (const std::int64_t* whole = value.asInt()) {
        return std::to_string(*whole);
    }
    if (const double* real = value.asFloat()) {
        return std::format("{}", *real);
    }
    if (const bool* flag = value.asBool()) {
        return *flag ? "true" : "false";
    }
    return {};
}
} // namespace

std::expected<PluginEntityValue, std::int32_t> toEntityValue(const Value& value) {
    if (value.isNull()) {
        return PluginEntityValue{};
    }
    if (const bool* flag = value.asBool()) {
        return PluginEntityValue{*flag};
    }
    if (const std::int64_t* whole = value.asInt()) {
        return PluginEntityValue{*whole};
    }
    if (const double* real = value.asFloat()) {
        return PluginEntityValue{*real};
    }
    if (const std::string* text = value.asString()) {
        return PluginEntityValue{*text};
    }
    if (const wire::wasm::Blob* blob = value.asBytes()) {
        return PluginEntityValue{std::string{blob->data.begin(), blob->data.end()}};
    }
    if (const Value::List* list = value.asList()) {
        std::vector<std::string> texts;
        for (const Value& item : *list) {
            if (item.asList() != nullptr || item.asMap() != nullptr) {
                return std::unexpected(abi::kErrInvalid);
            }
            texts.push_back(textOf(item));
        }
        return PluginEntityValue{std::move(texts)};
    }
    return std::unexpected(abi::kErrInvalid);
}

std::expected<std::vector<PluginEntityValue>, std::int32_t> toArguments(const abi::GuestMemory& memory,
                                                                        std::int32_t ptr, std::int32_t len) {
    std::vector<PluginEntityValue> arguments;
    if (len == 0) {
        return arguments;
    }
    const std::expected<Value, std::int32_t> value = readValue(memory, ptr, len);
    if (!value) {
        return std::unexpected(value.error());
    }
    const Value::List* list = value->asList();
    if (list == nullptr) {
        return std::unexpected(abi::kErrInvalid);
    }
    for (const Value& item : *list) {
        std::expected<PluginEntityValue, std::int32_t> converted = toEntityValue(item);
        if (!converted) {
            return std::unexpected(converted.error());
        }
        arguments.push_back(std::move(*converted));
    }
    return arguments;
}

namespace {
enum class Kind : std::uint8_t { Text, Whole, Real, Flag };

struct Column {
    std::string_view name;
    Kind kind;
};

struct RowShape {
    std::string_view operation;
    std::vector<Column> columns;
};

const std::vector<RowShape>& rowShapes() {
    static const std::vector<RowShape> kShapes = {
        {"effects",
         {{"effect", Kind::Text},
          {"duration", Kind::Whole},
          {"amplifier", Kind::Whole},
          {"ambient", Kind::Flag},
          {"particles", Kind::Flag},
          {"icon", Kind::Flag}}},
        {"attribute_modifiers", {{"id", Kind::Text}, {"amount", Kind::Real}, {"operation", Kind::Text}}},
        {"goals",
         {{"key", Kind::Text},
          {"priority", Kind::Whole},
          {"running", Kind::Flag},
          {"plugin", Kind::Flag},
          {"usable", Kind::Flag},
          {"flags", Kind::Text}}},
        {"brain_memories", {{"key", Kind::Text}, {"present", Kind::Flag}, {"ttl", Kind::Whole}}},
        {"brain_behaviors", {{"activity", Kind::Text}, {"priority", Kind::Whole}, {"key", Kind::Text}}},
        {"combat",
         {{"type", Kind::Text},
          {"message_id", Kind::Text},
          {"direct", Kind::Whole},
          {"cause", Kind::Whole},
          {"damage", Kind::Real},
          {"fall_distance", Kind::Real},
          {"location", Kind::Text},
          {"tick", Kind::Whole}}},
    };
    return kShapes;
}

[[nodiscard]] const RowShape* shapeOf(std::string_view operation) {
    for (const RowShape& shape : rowShapes()) {
        if (shape.operation == operation) {
            return &shape;
        }
    }
    return nullptr;
}

[[nodiscard]] Value cell(const Column& column, const std::string& text) {
    try {
        switch (column.kind) {
        case Kind::Whole:
            return Value{static_cast<std::int64_t>(std::stoll(text))};
        case Kind::Real:
            return Value{std::stod(text)};
        case Kind::Flag:
            return Value{text == "1" || text == "true"};
        case Kind::Text:
            break;
        }
    } catch (...) {
        return Value{text};
    }
    return Value{text};
}

[[nodiscard]] bool bytesResult(std::string_view hint) {
    return hint == "nbt" || hint == "equipment" || hint == "held_get" || hint == "shoulder";
}

[[nodiscard]] bool idList(std::string_view hint) {
    return hint == "find_path" || hint == "path_nodes" || hint == "passengers";
}
} // namespace

std::vector<std::string_view> columnsOf(std::string_view operation) {
    std::vector<std::string_view> names;
    if (const RowShape* shape = shapeOf(operation)) {
        for (const Column& column : shape->columns) {
            names.push_back(column.name);
        }
    }
    return names;
}

Value fromEntityValue(std::string_view hint, const PluginEntityValue& value) {
    if (const bool* flag = std::get_if<bool>(&value)) {
        return Value{*flag};
    }
    if (const std::int64_t* whole = std::get_if<std::int64_t>(&value)) {
        return Value{*whole};
    }
    if (const double* real = std::get_if<double>(&value)) {
        return Value{*real};
    }
    if (const std::string* text = std::get_if<std::string>(&value)) {
        if (bytesResult(hint)) {
            return Value{wire::wasm::Blob{{text->begin(), text->end()}}};
        }
        return Value{*text};
    }
    const auto* list = std::get_if<std::vector<std::string>>(&value);
    if (list == nullptr) {
        return Value{};
    }
    Value::List out;
    if (const RowShape* shape = shapeOf(hint); shape != nullptr && !shape->columns.empty()) {
        const std::size_t stride = shape->columns.size();
        for (std::size_t at = 0; at + stride <= list->size(); at += stride) {
            Value::Map row;
            for (std::size_t column = 0; column < stride; ++column) {
                row.push_back(
                    {std::string{shape->columns[column].name}, cell(shape->columns[column], (*list)[at + column])});
            }
            out.emplace_back(std::move(row));
        }
        return Value{std::move(out)};
    }
    for (const std::string& item : *list) {
        if (idList(hint) || hint == "eye_target") {
            try {
                out.emplace_back(hint == "eye_target" ? Value{std::stod(item)}
                                                      : Value{static_cast<std::int64_t>(std::stoll(item))});
                continue;
            } catch (...) {
            }
        }
        out.emplace_back(item);
    }
    return Value{std::move(out)};
}
} // namespace entities
} // namespace basalt::server::plugins::wasm
