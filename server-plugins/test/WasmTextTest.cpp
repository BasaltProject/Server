#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "../src/wasm/WasmState.hpp"

#include <basalt/wire/io/FileIO.hpp>

#include <doctest.h>

#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

using namespace basalt::server::plugins;
using namespace basalt::server::plugins::wasm;
namespace fileIO = basalt::wire::io::fileIO;

namespace {
void put32(std::vector<std::uint8_t>& out, std::uint32_t value) {
    for (int shift = 0; shift < 32; shift += 8) {
        out.push_back(static_cast<std::uint8_t>(value >> shift));
    }
}

void putText(std::vector<std::uint8_t>& out, std::string_view text) {
    put32(out, static_cast<std::uint32_t>(text.size()));
    out.insert(out.end(), text.begin(), text.end());
}
} // namespace

TEST_CASE("a wasm text template fills its placeholders") {
    const std::string folder = (std::filesystem::path{BASALT_TEST_SCRATCH_DIR} / "wasm_text").string();
    fileIO::createDirectories(folder + "/lang");
    fileIO::writeText(folder + "/lang/en_us.toml", "hello = \"Hi {name}, you have {amount} {unit} {missing}\"\n");
    PluginText text{folder, "en_us"};
    text.load();

    std::vector<std::uint8_t> packed;
    put32(packed, 2);
    putText(packed, "name");
    putText(packed, "Ana");
    putText(packed, "amount");
    putText(packed, "7");
    const basalt::wire::wasm::GuestMemory memory{packed.data(), packed.size()};
    const std::optional<ArgumentList> arguments = readArguments(memory, 0, static_cast<std::int32_t>(packed.size()));
    REQUIRE(arguments.has_value());

    std::string rendered;
    REQUIRE(renderText(text, "@hello", arguments->view(), rendered));

    CHECK(rendered == "Hi Ana, you have 7 {unit} {missing}");

    CHECK_FALSE(renderText(text, "@nothing", arguments->view(), rendered));
    CHECK(readArguments(memory, 0, 0)->count == 0);
    CHECK_FALSE(readArguments(memory, 0, static_cast<std::int32_t>(packed.size()) - 1).has_value());
}
