#include <basalt/server/runtime/world/Books.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <algorithm>
#include <utility>

namespace basalt::server::runtime::world::books {
namespace {

using wire::nbt::NbtCompound;
using wire::nbt::NbtList;
using wire::nbt::NbtTag;
using wire::nbt::NbtType;

constexpr std::string_view kWritable = "minecraft:writable_book_content";
constexpr std::string_view kWritten = "minecraft:written_book_content";
constexpr std::string_view kPages = "pages";
constexpr std::string_view kWritableBook = "minecraft:writable_book";
constexpr std::string_view kWrittenBook = "minecraft:written_book";

/// A page is a plain string wherever nothing was filtered out of it, which is every page this
/// server writes: it filters nothing.
[[nodiscard]] NbtList pageList(const std::vector<std::string>& pages) {
    NbtList list{NbtType::String};
    for (const std::string& page : pages) {
        list.push(NbtTag::ofString(page.substr(0, kMaxPageLength)));
    }
    return list;
}

} // namespace

bool isWritable(const storage::StoredItemStack& book) {
    // parity: a `writable_book` carries `writable_book_content` whether or not anything was written
    // into it, so the item's own name answers for an empty one.
    return book.id == kWritableBook || book.components.getCompound(kWritable) != nullptr;
}

std::vector<std::string> pagesOf(const storage::StoredItemStack& book) {
    std::vector<std::string> pages;
    for (const std::string_view component : {kWritable, kWritten}) {
        const NbtCompound* content = book.components.getCompound(component);
        const NbtList* listed = content == nullptr ? nullptr : content->getList(kPages);
        if (listed == nullptr) {
            continue;
        }
        for (const NbtTag& page : *listed) {
            const std::string* text = page.get<std::string>();
            pages.push_back(text == nullptr ? std::string{} : *text);
        }
        return pages;
    }
    return pages;
}

bool editPage(std::vector<std::string>& pages, const PageEdit what, const std::size_t page, const std::size_t other,
              const std::string_view text) {
    if (page >= kMaxPages || (what == PageEdit::Swap && other >= kMaxPages)) {
        return false;
    }
    switch (what) {
    case PageEdit::Replace:
        // A client may write past the end, and the pages it skipped come out blank.
        if (pages.size() <= page) {
            pages.resize(page + 1);
        }
        pages[page] = std::string{text.substr(0, kMaxPageLength)};
        return true;
    case PageEdit::Add:
        if (pages.size() >= kMaxPages) {
            return false;
        }
        if (pages.size() < page) {
            pages.resize(page);
        }
        pages.insert(pages.begin() + static_cast<std::ptrdiff_t>(std::min(page, pages.size())),
                     std::string{text.substr(0, kMaxPageLength)});
        return true;
    case PageEdit::Delete:
        if (page >= pages.size()) {
            return false;
        }
        pages.erase(pages.begin() + static_cast<std::ptrdiff_t>(page));
        return true;
    case PageEdit::Swap:
        if (page >= pages.size() || other >= pages.size()) {
            return false;
        }
        std::swap(pages[page], pages[other]);
        return true;
    }
    return false;
}

bool writePages(storage::StoredItemStack& book, const std::vector<std::string>& pages) {
    if (!isWritable(book) || pages.size() > kMaxPages) {
        return false;
    }
    NbtCompound content;
    content.set(kPages, NbtTag::ofList(pageList(pages)));
    book.components.set(kWritable, NbtTag::ofCompound(std::move(content)));
    return true;
}

std::optional<storage::StoredItemStack> sign(const storage::StoredItemStack& book, const std::string_view title,
                                             const std::string_view author) {
    if (!isWritable(book)) {
        return std::nullopt;
    }
    storage::StoredItemStack written = book;
    written.id = std::string{kWrittenBook};
    static_cast<void>(written.components.remove(kWritable));
    NbtCompound content;
    content.putString("title", std::string{title.substr(0, kMaxTitleLength)});
    content.putString("author", std::string{author});
    content.putInt("generation", 0);
    content.set(kPages, NbtTag::ofList(pageList(pagesOf(book))));
    content.putBoolean("resolved", true);
    written.components.set(kWritten, NbtTag::ofCompound(std::move(content)));
    return written;
}

} // namespace basalt::server::runtime::world::books
