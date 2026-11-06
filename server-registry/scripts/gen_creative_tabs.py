#!/usr/bin/env python3
"""Writes src/CreativeTabItems.cpp: the item ids of each creative tab, in tab order, read from the decompiled source.

Usage: gen_creative_tabs.py <CreativeModeTabs.java> <Items.java> <out.cpp>
"""
import re
import sys

COLORS = ["white", "orange", "magenta", "light_blue", "yellow", "lime", "pink", "gray", "light_gray", "cyan",
          "purple", "blue", "brown", "green", "red", "black"]
STATES = ["", "exposed_", "weathered_", "oxidized_"]
STATE_NAMES = ["unaffected", "exposed", "weathered", "oxidized"]


def parse_items(path):
    text = open(path).read()
    plain, colors, coppers = {}, {}, {}
    for m in re.finditer(r'public static final Item (\w+) = named\("([a-z0-9_]+)"\);', text):
        plain[m.group(1)] = m.group(2)
    for m in re.finditer(r'ColorCollection<Item> (\w+) = ColorCollection\.from\(color -> named\(color\.getName\(\) \+ "([a-z0-9_]+)"\)\);', text):
        colors[m.group(1)] = m.group(2)
    for m in re.finditer(r'WeatheringCopperCollection<Item> (\w+) = copper\("([a-z0-9_]+)"\);', text):
        coppers[m.group(1)] = m.group(2)
    return plain, colors, coppers


def copper_ids(base):
    return [("copper_block" if i == 0 else STATES[i] + "copper") if base == "copper_block" else STATES[i] + base
            for i in range(4)]


def main():
    source = open(sys.argv[1]).read()
    plain, colors, coppers = parse_items(sys.argv[2])
    order_line = re.search(r"gameplayColorOrder = List\.of\(([^)]*)\)", source).group(1)
    order = [c.strip().split(".")[1].lower() for c in order_line.split(",")]
    keys = dict(re.findall(r'(\w+) = createKey\("(\w+)"\)', source))
    families = re.search(r"copperBlockFamilies\(final Consumer<[^>]*>[^>]*> output\) \{(.*?)\n    \}", source, re.S).group(1)
    family_items = re.findall(r"Items\.(\w+)\)", families)
    sections = re.split(r"Registry\.register\(registry, \(ResourceKey\)(\w+),", source[source.index("bootstrap("):source.index("registerColoredItems(final")])
    tabs, problems = {}, []

    def name_of(constant):
        if constant not in plain:
            problems.append("unknown item " + constant)
            return None
        return plain[constant]

    def expand_copper(constant, which):
        base = coppers[constant]
        ids = copper_ids(base)
        return [("waxed_" + i) if which == "waxed" else i for i in ids]

    for index in range(1, len(sections), 2):
        tab = keys[sections[index]]
        body = sections[index + 1]
        out = []
        current_var = None
        in_family = None
        for line in body.splitlines():
            line = line.strip()
            m = re.match(r"\w+\.accept\(\(ItemLike\)Items\.(\w+)\);", line)
            if m:
                found = name_of(m.group(1))
                if found:
                    out.append(found)
                continue
            m = re.match(r"registerColoredItems\(\w+, gameplayColorOrder, Items\.(\w+)\);", line)
            if m:
                suffix = colors[m.group(1)]
                out += [c + suffix for c in order]
                continue
            m = re.match(r"\w+ var10000 = Items\.(\w+)(?:\.(waxed|weathering)\(\))?;", line) or re.match(r"var10000 = Items\.(\w+);", line)
            if m:
                current_var = m.group(1)
                continue
            if re.match(r"var10000\.forEach\(\w+::accept\);", line) and current_var:
                if current_var in coppers:
                    out += expand_copper(current_var, "weathering") + expand_copper(current_var, "waxed")
                elif current_var in colors:
                    out += [c + colors[current_var] for c in COLORS]
                current_var = None
                continue
            m = re.match(r"WeatheringCopperCollection\.ByState var10000 = family\.(weathering|waxed)\(\);", line)
            if m:
                in_family = m.group(1)
                continue
            if in_family and re.match(r"var10000\.forEach\(\w+::accept\);", line):
                for constant in family_items:
                    out += expand_copper(constant, in_family)
                in_family = None
                continue
            m = re.match(r"WeatheringCopperCollection\.ByState var10000 = Items\.(\w+)\.waxed\(\);", line)
            if m:
                out += expand_copper(m.group(1), "waxed")
                current_var = None
                continue
            m = re.match(r"\w+\.accept\(Items\.(\w+)\.(\w+)\(\)\);", line)
            if m and m.group(1) in colors:
                out.append(m.group(2).replace("lightBlue", "light_blue").replace("lightGray", "light_gray") + colors[m.group(1)])
                continue
            m = re.match(r"\w+\.accept\(Items\.(\w+)\.waxed\(\)\.unaffected\(\)\);", line)
            if m:
                out.append(expand_copper(m.group(1), "waxed")[0])
                continue
            m = re.match(r"Items\.DYE\.forEach", line)
            if m:
                out += [c + "_dye" for c in COLORS]
                continue
        if tab == "search" or not out:
            continue
        tabs[tab] = out
    if problems:
        print("\n".join(sorted(set(problems))), file=sys.stderr)
        sys.exit(1)
    known = set(plain.values())
    for c, s in colors.items():
        known |= {x + s for x in COLORS}
    for c, b in coppers.items():
        known |= set(copper_ids(b)) | {"waxed_" + x for x in copper_ids(b)}
    bad = sorted({i for ids in tabs.values() for i in ids if i not in known})
    if bad:
        print("ids not in Items.java:", bad, file=sys.stderr)
        sys.exit(1)
    camel = lambda tab: "k" + "".join(w.capitalize() for w in tab.split("_"))
    lines = ["// Generated by scripts/gen_creative_tabs.py from the decompiled tab builder: ids only.",
             "#include <basalt/server/registry/CreativeTabs.hpp>", "", "#include <array>", "",
             "namespace basalt::server::registry {", "namespace {"]
    for tab, ids in tabs.items():
        lines.append(f"constexpr std::array<std::string_view, {len(ids)}> {camel(tab)} = {{")
        lines += [f'    "{i}",' for i in ids]
        lines.append("};")
    lines += ["} // namespace", "", "std::span<const std::string_view> creativeTabItems(std::string_view tab) {"]
    for tab in tabs:
        lines.append(f'    if (tab == "{tab}") {{ return {camel(tab)}; }}')
    lines += ["    return {};", "}", "", "} // namespace basalt::server::registry", ""]
    open(sys.argv[3], "w").write("\n".join(lines))
    for tab, ids in tabs.items():
        print(tab, len(ids))


main()
