#include <basalt/server/runtime/datapack/DataFiles.hpp>
#include <basalt/server/runtime/world/RealTestInstances.hpp>
#include <basalt/wire/common/json/Json.hpp>

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <expected>
#include <utility>

namespace basalt::server::runtime::world {
namespace {

using wire::common::Identifier;
using wire::common::JsonObject;
using wire::common::JsonValue;

/// parity: `GameTestInstance`'s own default, the tick budget a test that names none gets.
constexpr std::int32_t kDefaultMaxTicks = 100;

/// An id field. Only a string is one: a number would parse as an identifier without ever being one.
[[nodiscard]] std::optional<Identifier> identifierAt(const JsonObject& root, std::string_view key) {
    const JsonValue* value = root.find(key);
    if (value == nullptr) {
        return std::nullopt;
    }
    const std::string* text = value->get<std::string>();
    if (text == nullptr) {
        return std::nullopt;
    }
    const std::expected<Identifier, wire::common::IdentifierError> parsed = Identifier::parse(*text);
    return parsed.has_value() ? std::optional{*parsed} : std::nullopt;
}

/// The tick budget, written either as a number or as the text of one, as the schema allows.
[[nodiscard]] std::int32_t maxTicksAt(const JsonObject& root) {
    const JsonValue* value = root.find("max_ticks");
    if (value == nullptr) {
        return kDefaultMaxTicks;
    }
    if (const std::int64_t* whole = value->get<std::int64_t>(); whole != nullptr) {
        return static_cast<std::int32_t>(*whole);
    }
    const std::string* text = value->get<std::string>();
    if (text == nullptr) {
        return kDefaultMaxTicks;
    }
    std::int32_t parsed = 0;
    const char* first = text->data();
    const char* last = first + text->size();
    const std::from_chars_result result = std::from_chars(first, last, parsed);
    return result.ec == std::errc{} && result.ptr == last ? parsed : kDefaultMaxTicks;
}

/// parity: a test is required unless it says otherwise, so anything unreadable reads as required.
[[nodiscard]] bool requiredAt(const JsonObject& root) {
    const JsonValue* value = root.find("required");
    if (value == nullptr) {
        return true;
    }
    if (const bool* flag = value->get<bool>(); flag != nullptr) {
        return *flag;
    }
    const std::string* text = value->get<std::string>();
    if (text == nullptr) {
        return true;
    }
    return *text != "false";
}

/// One declared instance, or why the file could not be read as one.
[[nodiscard]] std::expected<TestInstance, std::string> declared(const std::string& path,
                                                                const std::optional<std::string>& text) {
    if (!text.has_value()) {
        return std::unexpected(std::string{"no such entry"});
    }
    const std::expected<JsonValue, wire::common::JsonError> root = wire::common::parseJson(*text);
    if (!root.has_value()) {
        return std::unexpected(std::string{"malformed JSON"});
    }
    const JsonObject* object = root->get<JsonObject>();
    if (object == nullptr) {
        return std::unexpected(std::string{"not a JSON object"});
    }
    const std::optional<Identifier> type = identifierAt(*object, "type");
    if (!type.has_value()) {
        return std::unexpected(std::string{"missing or malformed type"});
    }
    // A declared function that does not parse is a broken test, not a test without one.
    const bool namesFunction = object->find("function") != nullptr;
    const std::optional<Identifier> function = identifierAt(*object, "function");
    if (namesFunction && !function.has_value()) {
        return std::unexpected(std::string{"malformed function"});
    }
    return TestInstance{
        .id = Identifier::vanilla(path),
        .type = *type,
        .function = function,
        .maxTicks = maxTicksAt(*object),
        .required = requiredAt(*object),
    };
}

} // namespace

bool TestInstance::isFunctionTest() const { return type == Identifier::vanilla("function") && function.has_value(); }

RealTestInstances::RealTestInstances(const Jar& jar, wire::common::Logger logger) : logger_(std::move(logger)) {
    if (!jar.entriesUnder || !jar.readText) {
        return;
    }
    for (const auto& [id, entry] : datapack::dataFiles(jar.entriesUnder, "test_instance")) {
        std::expected<TestInstance, std::string> instance = declared(id.path(), jar.readText(entry));
        if (!instance.has_value()) {
            logger_.debug([&] { return "Skipped test instance " + entry + ": " + instance.error(); });
            continue;
        }
        instance->id = id;
        all_.push_back(std::move(*instance));
    }
    if (!all_.empty()) {
        logger_.info([&] { return "Loaded " + std::to_string(all_.size()) + " test instance(s)."; });
    }
}

const TestInstance* RealTestInstances::find(const Identifier& id) const {
    const auto found = std::ranges::find(all_, id, &TestInstance::id);
    return found == all_.end() ? nullptr : &*found;
}

} // namespace basalt::server::runtime::world
