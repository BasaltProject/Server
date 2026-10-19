#pragma once

#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/common/logging/Logger.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::world {

struct TestInstance {
    wire::common::Identifier id;
    wire::common::Identifier type;
    std::optional<wire::common::Identifier> function;
    std::int32_t maxTicks = 0;
    bool required = true;
    /// A block based test's template, the ticks it waits before it starts, and how it is turned.
    std::optional<wire::common::Identifier> structure;
    std::int32_t setupTicks = 0;
    std::string rotation = "none";

    [[nodiscard]] bool isFunctionTest() const;
};

class RealTestInstances {
public:
    using ReadText = std::function<std::optional<std::string>(std::string_view name)>;

    using EntriesUnder = std::function<std::vector<std::string>(std::string_view prefix)>;

    struct Jar {
        ReadText readText;
        EntriesUnder entriesUnder;
    };

    explicit RealTestInstances(const Jar& jar, wire::common::Logger logger = wire::common::Logger{"Tests"});

    [[nodiscard]] const std::vector<TestInstance>& all() const noexcept { return all_; }

    [[nodiscard]] const TestInstance* find(const wire::common::Identifier& id) const;

private:
    wire::common::Logger logger_;
    std::vector<TestInstance> all_;
};

} // namespace basalt::server::runtime::world
