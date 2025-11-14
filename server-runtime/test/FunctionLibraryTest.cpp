#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/function/FunctionLibrary.hpp>
#include <basalt/wire/io/FileIO.hpp>

#include <doctest.h>

#include <filesystem>
#include <string>
#include <vector>

using namespace basalt::server::runtime::function;
using basalt::wire::common::Identifier;
namespace fileIO = basalt::wire::io::fileIO;

namespace {

/// A datapacks root of its own per test, inside the build tree and emptied before it is filled.
[[nodiscard]] std::string scratch(std::string_view name) {
    const std::filesystem::path directory = std::filesystem::path{BASALT_TEST_SCRATCH_DIR} / name;
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    std::filesystem::create_directories(directory, ignored);
    return directory.string();
}

void writeFunction(const std::string& root, const std::string& pack, const std::string& nameSpace,
                   const std::string& path, std::string_view text) {
    const std::string directory = root + "/" + pack + "/data/" + nameSpace + "/function";
    const std::size_t slash = path.rfind('/');
    fileIO::createDirectories(slash == std::string::npos ? directory : directory + "/" + path.substr(0, slash));
    fileIO::writeText(directory + "/" + path + ".mcfunction", text);
    fileIO::writeText(root + "/" + pack + "/pack.mcmeta", R"({"pack":{"pack_format":0,"description":""}})");
}

[[nodiscard]] std::vector<std::string> idsOf(const FunctionLibrary& library) {
    std::vector<std::string> named;
    for (const ServerFunction* one : library.all()) {
        named.push_back(one->id.toString());
    }
    return named;
}

} // namespace

TEST_CASE("what a `.mcfunction` keeps, and what it throws away") {
    const std::optional<std::vector<std::string>> lines = parseFunction("# a comment\n"
                                                                        "\n"
                                                                        "   say hello   \n"
                                                                        "\t\n"
                                                                        "   # an indented comment\n"
                                                                        "\tsay indented\n"
                                                                        "say goodbye\n");
    REQUIRE(lines.has_value());
    // The trim is Java's, which cuts at everything up to and including the space: a tab is not
    // whitespace to some definitions and is very much cut here.
    CHECK(*lines == std::vector<std::string>{"say hello", "say indented", "say goodbye"});

    // parity: a trailing backslash joins the next line, backslash dropped and both ends trimmed.
    const std::optional<std::vector<std::string>> joined =
        parseFunction("say one \\\n   two \\\n   three\nsay after\n");
    REQUIRE(joined.has_value());
    CHECK(*joined == std::vector<std::string>{"say one two three", "say after"});

    // A comment is only a comment once the joining is done: this one is part of the command above.
    const std::optional<std::vector<std::string>> notAComment = parseFunction("say \\\n# not a comment\n");
    REQUIRE(notAComment.has_value());
    CHECK(*notAComment == std::vector<std::string>{"say # not a comment"});

    // And a line that starts a continuation is a comment if it starts with one.
    const std::optional<std::vector<std::string>> commented = parseFunction("# still a comment \\\nsay hello\n");
    REQUIRE(commented.has_value());
    CHECK(commented->empty());

    // A macro line is kept as written: expanding it is the executor's business.
    const std::optional<std::vector<std::string>> macro = parseFunction("$say hello $(name)\n");
    REQUIRE(macro.has_value());
    CHECK(*macro == std::vector<std::string>{"$say hello $(name)"});

    // parity: a file ending on a continuation is refused whole, not run up to the break.
    CHECK_FALSE(parseFunction("say one \\\n").has_value());
    CHECK_FALSE(parseFunction("say one \\").has_value());

    // Carriage returns from a pack authored on Windows are not part of the command.
    const std::optional<std::vector<std::string>> windows = parseFunction("say hello\r\nsay goodbye\r\n");
    REQUIRE(windows.has_value());
    CHECK(*windows == std::vector<std::string>{"say hello", "say goodbye"});

    CHECK(parseFunction("")->empty());
}

TEST_CASE("functions are found under the vanilla datapack layout, nested folders and all") {
    const std::string root = scratch("functions-layout");
    writeFunction(root, "pack_a", "custom", "greet", "say hello\n");
    writeFunction(root, "pack_a", "custom", "deep/inside/here", "say deep\n");
    writeFunction(root, "pack_a", "other", "greet", "say other\n");
    // Anything that is not a function is ignored, including the pack's own metadata.
    fileIO::writeText(root + "/pack_a/data/custom/function/notes.txt", "ignored");
    // A pack with no data directory at all is still a pack, and simply has nothing in it.
    fileIO::createDirectories(root + "/pack_b");

    FunctionLibrary library{root};
    CHECK(library.reload() == 3);
    CHECK(library.availablePacks() == std::vector<std::string>{"pack_a", "pack_b"});

    const ServerFunction* greet = library.get(Identifier{"custom", "greet"});
    REQUIRE(greet != nullptr);
    CHECK(greet->lines == std::vector<std::string>{"say hello"});
    CHECK(greet->pack == "pack_a");
    CHECK(library.get(Identifier{"custom", "deep/inside/here"}) != nullptr);
    CHECK(library.get(Identifier{"other", "greet"}) != nullptr);
    CHECK(library.get(Identifier{"custom", "notes"}) == nullptr);
    CHECK(library.get(Identifier{"custom", "missing"}) == nullptr);
    CHECK(library.all().size() == 3);

    // A function that ends on a continuation is refused rather than half-loaded.
    writeFunction(root, "pack_a", "custom", "broken", "say one \\\n");
    CHECK(library.reload() == 3);
    CHECK(library.get(Identifier{"custom", "broken"}) == nullptr);

    // A reload is a reload, not a merge: a function whose file is gone goes with it.
    CHECK(fileIO::remove(root + "/pack_a/data/custom/function/greet.mcfunction"));
    CHECK(library.reload() == 2);
    CHECK(library.get(Identifier{"custom", "greet"}) == nullptr);
}

TEST_CASE("a disabled pack takes its functions with it and gives them back") {
    const std::string root = scratch("functions-enable");
    writeFunction(root, "pack_a", "custom", "a", "say a\n");
    writeFunction(root, "pack_b", "custom", "b", "say b\n");

    FunctionLibrary library{root};
    library.reload();
    CHECK(library.isEnabled("pack_a"));
    CHECK(library.enabledPacks() == std::vector<std::string>{"pack_a", "pack_b"});

    CHECK(library.setEnabled("pack_a", false));
    // Disabling twice changes nothing the second time, which is what a command reports back.
    CHECK_FALSE(library.setEnabled("pack_a", false));
    CHECK_FALSE(library.isEnabled("pack_a"));
    CHECK(library.get(Identifier{"custom", "a"}) == nullptr);
    CHECK(idsOf(library) == std::vector<std::string>{"custom:b"});
    CHECK(library.enabledPacks() == std::vector<std::string>{"pack_b"});
    // It is still there, just off: a reload does not forget that it was disabled.
    CHECK(library.reload() == 2);
    CHECK_FALSE(library.isEnabled("pack_a"));

    CHECK(library.setEnabled("pack_a", true));
    CHECK_FALSE(library.setEnabled("pack_a", true));
    CHECK(library.get(Identifier{"custom", "a"}) != nullptr);

    // A pack nobody has is neither enabled nor disabled.
    CHECK_FALSE(library.setEnabled("nothing", false));
    CHECK_FALSE(library.setEnabled("nothing", true));
    CHECK_FALSE(library.isEnabled("nothing"));
}

TEST_CASE("where an enabled pack lands in the order, which decides who wins a name") {
    const std::string root = scratch("functions-order");
    for (const std::string& pack : {"a", "b", "c"}) {
        writeFunction(root, pack, "custom", "f", "say " + pack + "\n");
    }

    FunctionLibrary library{root};
    library.reload();
    CHECK(library.enabledPacks() == std::vector<std::string>{"a", "b", "c"});

    library.setEnabled("c", true, FunctionLibrary::Position::first());
    CHECK(library.enabledPacks() == std::vector<std::string>{"c", "a", "b"});

    library.setEnabled("c", true, FunctionLibrary::Position::last());
    CHECK(library.enabledPacks() == std::vector<std::string>{"a", "b", "c"});

    library.setEnabled("a", true, FunctionLibrary::Position::after("b"));
    CHECK(library.enabledPacks() == std::vector<std::string>{"b", "a", "c"});

    library.setEnabled("c", true, FunctionLibrary::Position::before("a"));
    CHECK(library.enabledPacks() == std::vector<std::string>{"b", "c", "a"});

    // A pack that is not in the order lands at the front either way, which is where an index of
    // minus one puts it.
    library.setEnabled("a", true, FunctionLibrary::Position::before("nothing"));
    CHECK(library.enabledPacks() == std::vector<std::string>{"a", "b", "c"});
    library.setEnabled("b", true, FunctionLibrary::Position::after("nothing"));
    CHECK(library.enabledPacks() == std::vector<std::string>{"b", "a", "c"});

    // A reload finds the same packs again and leaves the order they were put in alone, rather
    // than appending each of them to it a second time.
    library.reload();
    CHECK(library.enabledPacks() == std::vector<std::string>{"b", "a", "c"});

    // A disabled pack keeps its place in the order and comes back to it.
    library.setEnabled("a", false);
    CHECK(library.enabledPacks() == std::vector<std::string>{"b", "c"});
    library.setEnabled("a", true, FunctionLibrary::Position::after("b"));
    CHECK(library.enabledPacks() == std::vector<std::string>{"b", "a", "c"});
}

TEST_CASE("a created pack is a pack, and a name already taken is refused") {
    const std::string root = scratch("functions-create");
    FunctionLibrary library{root};
    library.reload();
    CHECK(library.availablePacks().empty());

    CHECK(library.create("fresh", "a description"));
    CHECK(fileIO::isDirectory(root + "/fresh/data"));
    const std::optional<std::string> meta = fileIO::readText(root + "/fresh/pack.mcmeta");
    REQUIRE(meta.has_value());
    CHECK(*meta == R"({"pack":{"pack_format":0,"description":"a description"}})");
    // Creating reloads, so the new pack is available without being asked twice.
    CHECK(library.availablePacks() == std::vector<std::string>{"fresh"});
    CHECK(library.isEnabled("fresh"));

    CHECK_FALSE(library.create("fresh", "another"));
    CHECK(*fileIO::readText(root + "/fresh/pack.mcmeta") ==
          R"({"pack":{"pack_format":0,"description":"a description"}})");
}

TEST_CASE("packs are looked for beside worlds and config") {
    // Where an operator drops a pack, and where a server with no configured root looks for one.
    CHECK(FunctionLibrary::kDatapacksRoot == "datapacks");
}

TEST_CASE("a missing datapacks directory is empty, not an error") {
    FunctionLibrary library{scratch("functions-none") + "/never-created"};
    CHECK(library.reload() == 0);
    CHECK(library.availablePacks().empty());
    CHECK(library.enabledPacks().empty());
    CHECK(library.all().empty());
}
