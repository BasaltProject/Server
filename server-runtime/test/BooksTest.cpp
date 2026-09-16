#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/world/Books.hpp>

#include <doctest.h>

#include <string>

using namespace basalt::server::runtime;
using basalt::server::storage::StoredItemStack;

namespace {

[[nodiscard]] StoredItemStack blank() { return StoredItemStack{.id = "minecraft:writable_book", .count = 1}; }

} // namespace

TEST_CASE("pages go onto a writable book and come back in order") {
    StoredItemStack book = blank();
    // parity: one straight out of a crafting table carries the component without being written in.
    CHECK(world::books::isWritable(book));
    CHECK(world::books::pagesOf(book).empty());
    CHECK(world::books::writePages(book, {"one", "two"}));
    CHECK(world::books::pagesOf(book) == std::vector<std::string>{"one", "two"});

    // Only a writable book takes them: a signed one is read, never written.
    StoredItemStack stone{.id = "minecraft:stone", .count = 1};
    CHECK_FALSE(world::books::writePages(stone, {"one"}));
}

TEST_CASE("a page at a time, the way the other edition sends it") {
    std::vector<std::string> pages;
    CHECK(world::books::editPage(pages, world::books::PageEdit::Replace, 0, 0, "first"));
    // parity: writing past the end leaves the pages it skipped blank rather than refusing.
    CHECK(world::books::editPage(pages, world::books::PageEdit::Replace, 2, 0, "third"));
    CHECK(pages == std::vector<std::string>{"first", "", "third"});

    CHECK(world::books::editPage(pages, world::books::PageEdit::Add, 1, 0, "second"));
    CHECK(pages == std::vector<std::string>{"first", "second", "", "third"});

    CHECK(world::books::editPage(pages, world::books::PageEdit::Swap, 0, 3, ""));
    CHECK(pages[0] == "third");
    CHECK(pages[3] == "first");

    CHECK(world::books::editPage(pages, world::books::PageEdit::Delete, 2, 0, ""));
    CHECK(pages.size() == 3);
    CHECK_FALSE(world::books::editPage(pages, world::books::PageEdit::Delete, 9, 0, ""));
    CHECK_FALSE(world::books::editPage(pages, world::books::PageEdit::Swap, 0, 9, ""));
}

TEST_CASE("signing swaps one component for the other and names the author") {
    StoredItemStack book = blank();
    CHECK(world::books::writePages(book, {"one", "two"}));
    const std::optional<StoredItemStack> signed_ = world::books::sign(book, "A title", "Whiron");
    REQUIRE(signed_.has_value());
    CHECK(signed_->id == "minecraft:written_book");
    CHECK_FALSE(world::books::isWritable(*signed_));
    CHECK(world::books::pagesOf(*signed_) == std::vector<std::string>{"one", "two"});

    const basalt::wire::nbt::NbtCompound* content = signed_->components.getCompound("minecraft:written_book_content");
    REQUIRE(content != nullptr);
    CHECK(content->getString("title") == "A title");
    CHECK(content->getString("author") == "Whiron");
    CHECK(content->getInt("generation") == 0);

    // A book that is already signed is not signed again.
    CHECK_FALSE(world::books::sign(*signed_, "Another", "Whiron").has_value());
}
