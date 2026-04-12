#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/world/AdvancementTree.hpp>
#include <basalt/server/runtime/world/PlayerAdvancements.hpp>
#include <basalt/server/runtime/world/PlayerProgress.hpp>
#include <basalt/server/runtime/world/VanillaAdvancementImport.hpp>

#include <doctest.h>

#include <algorithm>
#include <map>
#include <string>

using namespace basalt::server::runtime::world;
using namespace basalt::server::world::advancement;
using basalt::wire::common::Identifier;
using basalt::wire::text::Component;

namespace {

[[nodiscard]] Advancement node(std::optional<std::string> parent, bool displayed = true, bool hidden = false) {
    Advancement advancement;
    if (parent.has_value()) {
        advancement.parent = Identifier{"t", *parent};
    }
    if (displayed) {
        advancement.display = AdvancementDisplay{.title = Component::text("x"), .hidden = hidden};
    }
    advancement.criteria.emplace_back("done", AdvancementCriterion{.trigger = Identifier::vanilla("impossible")});
    advancement.requirements.groups = {{"done"}};
    return advancement;
}

[[nodiscard]] std::vector<std::string> ids(const std::vector<basalt::wire::protocol::play::AdvancementEntry>& added) {
    std::vector<std::string> named;
    for (const auto& entry : added) {
        named.push_back(entry.id);
    }
    return named;
}

} // namespace

TEST_CASE("the tree lays siblings out one row apart and centres the parent on them") {
    const RealAdvancements advancements{std::map<Identifier, Advancement>{
        {Identifier{"t", "root"}, node(std::nullopt)},
        {Identifier{"t", "a"}, node("root")},
        {Identifier{"t", "b"}, node("root")},
        {Identifier{"t", "c"}, node("a")},
    }};
    const AdvancementTree tree{advancements};
    const auto* root = tree.get(Identifier{"t", "root"});
    const auto* a = tree.get(Identifier{"t", "a"});
    const auto* b = tree.get(Identifier{"t", "b"});
    const auto* c = tree.get(Identifier{"t", "c"});
    REQUIRE((root && a && b && c));
    CHECK(root->x == 0.0F);
    CHECK(a->x == 1.0F);
    CHECK(c->x == 2.0F);
    CHECK(b->y - a->y == 1.0F);
    CHECK(root->y == (a->y + b->y) / 2.0F);
    CHECK(tree.rootOf(Identifier{"t", "c"}) == Identifier{"t", "root"});
}

TEST_CASE("progress reaches the client with a reset first, and only what may be seen") {
    const RealAdvancements advancements{std::map<Identifier, Advancement>{
        {Identifier{"t", "root"}, node(std::nullopt)},
        {Identifier{"t", "a"}, node("root")},
        {Identifier{"t", "secret"}, node("root", true, true)},
        {Identifier{"t", "far"}, node("a")},
    }};
    PlayerProgress progress;
    PlayerAdvancements sync{advancements, progress};
    progress.onChanged([&sync](std::string_view uuid, const Identifier& id) { sync.changed(uuid, id); });
    sync.join("p");
    // parity: nothing started, nothing to say, even on the first flush.
    CHECK_FALSE(sync.flush("p", true).has_value());

    REQUIRE(progress.grantCriterion("p", Identifier{"t", "root"}, "done"));
    const auto first = sync.flush("p", true);
    REQUIRE(first.has_value());
    CHECK_FALSE(first->reset);
    std::vector<std::string> added = ids(first->added);
    std::ranges::sort(added);
    // The hidden one waits until done; a grandchild is still within two of a shown root.
    CHECK(added == std::vector<std::string>{"t:a", "t:far", "t:root"});
    // parity: every advancement that turns visible carries its progress, done or not.
    REQUIRE(first->progress.size() == 3);
    for (const auto& one : first->progress) {
        CHECK(one.criteria.at(0).second.has_value() == (one.id == "t:root"));
    }
    CHECK_FALSE(sync.flush("p", true).has_value());

    // A rejoin starts with a reset and what was started.
    sync.join("p");
    const auto again = sync.flush("p", true);
    REQUIRE(again.has_value());
    CHECK(again->reset);

    REQUIRE(progress.revokeCriterion("p", Identifier{"t", "root"}, "done"));
    const auto revoked = sync.flush("p", false);
    REQUIRE(revoked.has_value());
    CHECK(revoked->removed.size() == 3);
    CHECK_FALSE(revoked->showAdvancements);
}

TEST_CASE("a selected tab has to be a displayed root") {
    const RealAdvancements advancements{std::map<Identifier, Advancement>{
        {Identifier{"t", "root"}, node(std::nullopt)},
        {Identifier{"t", "a"}, node("root")},
    }};
    PlayerProgress progress;
    PlayerAdvancements sync{advancements, progress};
    sync.join("p");
    CHECK_FALSE(sync.selectTab("p", Identifier{"t", "a"}).has_value());
    const auto chosen = sync.selectTab("p", Identifier{"t", "root"});
    REQUIRE(chosen.has_value());
    CHECK(chosen->tab == "t:root");
    CHECK_FALSE(sync.selectTab("p", Identifier{"t", "root"}).has_value());
}

TEST_CASE("a vanilla obtained time reads as epoch milliseconds, offset and all") {
    CHECK(parseObtainedTime("1970-01-01 00:00:00 +0000") == 0);
    CHECK(parseObtainedTime("2024-02-29 12:30:15 +0000") == 1709209815000);
    CHECK(parseObtainedTime("2024-02-29 14:30:15 +0200") == 1709209815000);
    CHECK_FALSE(parseObtainedTime("yesterday").has_value());
}
