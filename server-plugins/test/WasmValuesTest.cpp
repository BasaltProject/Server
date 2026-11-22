#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "../src/wasm/WasmValues.hpp"

#include <doctest.h>

#include <array>

using namespace basalt::server::plugins::wasm;
namespace wasm = basalt::wire::wasm;

TEST_CASE("a handle is stale once its slot was removed and reused") {
    HandleTable<int> table;
    const std::int32_t first = table.add(7);
    int* found = nullptr;
    REQUIRE(table.find(first, found) == abi::kOk);
    CHECK(*found == 7);
    CHECK(table.remove(first));
    CHECK(table.find(first, found) == abi::kErrStale);
    const std::int32_t second = table.add(8);
    CHECK(second != first);
    CHECK(table.find(first, found) == abi::kErrStale);
    CHECK(table.find(0, found) == abi::kErrInvalid);
}

TEST_CASE("a value round trips through guest memory with the needed-length convention") {
    std::array<std::uint8_t, 64> bytes{};
    const abi::GuestMemory memory{bytes.data(), bytes.size()};
    const wasm::Value value{wasm::Value::Map{{"k", wasm::Value{std::int64_t{5}}}, {"s", wasm::Value{"x"}}}};
    const std::int32_t needed = putValue(memory, 0, 0, value);
    REQUIRE(needed > 0);
    CHECK(putValue(memory, 0, needed, value) == needed);
    const auto read = readValue(memory, 0, needed);
    REQUIRE(read.has_value());
    CHECK(*read == value);
    CHECK(readValue(memory, 0, needed - 1).error() == abi::kErrInvalid);
}
