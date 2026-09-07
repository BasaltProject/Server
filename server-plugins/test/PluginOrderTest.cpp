#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/plugins/PluginOrder.hpp>

#include <doctest.h>

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

using namespace basalt::server::plugins;

namespace {

[[nodiscard]] PluginManifest manifest(std::string id, std::string version = "1.0.0",
                                      std::int32_t api = PluginManifest::kApi,
                                      std::vector<std::pair<std::string, std::string>> depends = {},
                                      std::vector<std::pair<std::string, std::string>> soft = {}) {
    PluginManifest built;
    built.name = id;
    built.id = std::move(id);
    built.version = std::move(version);
    built.api = api;
    built.main = "src/main.lua";
    for (auto& [dependencyId, requirement] : depends) {
        built.dependencies.push_back(
            PluginDependency{.id = std::move(dependencyId), .requirement = std::move(requirement), .hard = true});
    }
    for (auto& [dependencyId, requirement] : soft) {
        built.dependencies.push_back(
            PluginDependency{.id = std::move(dependencyId), .requirement = std::move(requirement), .hard = false});
    }
    return built;
}

[[nodiscard]] std::string describe(const std::vector<PluginOrdering>& orderings) {
    std::string text;
    for (const PluginOrdering& ordering : orderings) {
        if (!text.empty()) {
            text += ';';
        }
        text +=
            ordering.loads() ? "+" + ordering.manifest.id : "-" + ordering.manifest.id + "(" + ordering.refusal + ")";
    }
    return text;
}

[[nodiscard]] std::string bracketed(const std::vector<std::string>& values) {
    std::string text = "[";
    for (const std::string& value : values) {
        if (text.size() > 1) {
            text += ", ";
        }
        text += value;
    }
    return text + "]";
}

} // namespace

TEST_CASE("what loads and in what order is what the frozen kotlin decides") {
    // Nothing depends on anything: alphabetical, whatever order they were found in.
    CHECK(describe(pluginOrder::resolve({manifest("zebra"), manifest("apple"), manifest("mango")})) ==
          "+apple;+mango;+zebra");

    // A chain: the order is what the dependencies force, not the alphabet.
    CHECK(describe(pluginOrder::resolve({manifest("app", "1.0.0", 1, {{"core", ">=1.0"}}),
                                         manifest("core", "1.0.0", 1, {{"base", "*"}}), manifest("base")})) ==
          "+base;+core;+app");

    // A soft dependency does not decide whether something loads, but it does decide when.
    CHECK(describe(pluginOrder::resolve({manifest("addon", "1.0.0", 1, {}, {{"zbase", "*"}}), manifest("zbase")})) ==
          "+zbase;+addon");
    // The same soft dependency, with the other one absent: it loads anyway.
    CHECK(describe(pluginOrder::resolve({manifest("addon", "1.0.0", 1, {}, {{"gone", "*"}})})) == "+addon");

    CHECK(describe(pluginOrder::resolve({})).empty());
}

TEST_CASE("a plugin that cannot load is told why, in the frozen kotlin's own words") {
    CHECK(describe(pluginOrder::resolve({manifest("app", "1.0.0", 1, {{"core", "*"}})})) ==
          "-app(it needs 'core', which is not installed)");

    CHECK(describe(pluginOrder::resolve({manifest("app", "1.0.0", 1, {{"core", ">=2.0"}}), manifest("core", "1.5")})) ==
          "+core;-app(it needs 'core' >=2.0 and 1.5 is installed)");

    CHECK(describe(
              pluginOrder::resolve({manifest("old", "1.0.0", 0), manifest("future", "1.0.0", 2), manifest("fine")})) ==
          "+fine;-future(it was written for API 2 and this server speaks 1);"
          "-old(it was written for API 0 and this server speaks 1)");

    // An API refusal is the reason given, not the dependency it never got to check.
    CHECK(describe(pluginOrder::resolve({manifest("old", "1.0.0", 0, {{"gone", "*"}})})) ==
          "-old(it was written for API 0 and this server speaks 1)");

    // Two broken dependencies at once: the last one checked is the one named.
    CHECK(describe(pluginOrder::resolve({manifest("app", "1.0.0", 1, {{"aaa", "*"}, {"zzz", "*"}})})) ==
          "-app(it needs 'zzz', which is not installed)");
}

TEST_CASE("a refusal spreads down the chain and a cycle is named as a group") {
    // Deliberately in the order that defeats a single pass: 'far' is looked at before the plugin it
    // depends on has been refused, so the spread has to go round again.
    CHECK(describe(pluginOrder::resolve({manifest("far", "1.0.0", 1, {{"leaf", "*"}}),
                                         manifest("leaf", "1.0.0", 1, {{"mid", "*"}}),
                                         manifest("mid", "1.0.0", 1, {{"root", "*"}})})) ==
          "-far('leaf' did not load);-leaf('mid' did not load);-mid(it needs 'root', which is not installed)");

    CHECK(describe(pluginOrder::resolve({manifest("loop", "1.0.0", 1, {{"loop", "*"}})})) ==
          "-loop(it is in a dependency cycle with: loop)");

    CHECK(describe(pluginOrder::resolve({manifest("alpha", "1.0.0", 1, {{"beta", "*"}}),
                                         manifest("beta", "1.0.0", 1, {{"gamma", "*"}}),
                                         manifest("gamma", "1.0.0", 1, {{"alpha", "*"}}), manifest("free")})) ==
          "+free;-alpha(it is in a dependency cycle with: alpha, beta, gamma);"
          "-beta(it is in a dependency cycle with: alpha, beta, gamma);"
          "-gamma(it is in a dependency cycle with: alpha, beta, gamma)");
}

TEST_CASE("who breaks if one plugin goes away, furthest first") {
    const std::vector<PluginManifest> loaded{manifest("base"),
                                             manifest("core", "1.0.0", 1, {{"base", "*"}}),
                                             manifest("app", "1.0.0", 1, {{"core", "*"}}),
                                             manifest("ui", "1.0.0", 1, {{"core", "*"}}),
                                             manifest("unrelated"),
                                             manifest("softOnly", "1.0.0", 1, {}, {{"base", "*"}})};

    // Furthest first, so unloading can start at the leaves. A soft dependent is not in here: it
    // keeps working without the one that went away, which is what soft means.
    CHECK(bracketed(pluginOrder::dependents("base", loaded)) == "[ui, app, core]");
    CHECK(bracketed(pluginOrder::dependents("core", loaded)) == "[ui, app]");
    CHECK(bracketed(pluginOrder::dependents("app", loaded)) == "[]");
    CHECK(bracketed(pluginOrder::dependents("nobody", loaded)) == "[]");
}
