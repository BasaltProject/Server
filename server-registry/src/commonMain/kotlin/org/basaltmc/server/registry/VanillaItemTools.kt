package org.basaltmc.server.registry

/**
 * Every tool item's mining rules, from the real server jar's own item component reports
 * (`minecraft:tool`). Each rule is a block tag, whether the tool is correct for that tag's
 * drops, and how fast it mines it.
 *
 * Order matters and is preserved: vanilla applies the first rule whose tag contains the
 * block, and every tool lists its `incorrect_for_<tier>_tool` exclusion before the general
 * `mineable/<kind>` rule so the exclusion wins.
 */
internal val ALL_ITEM_TOOLS: List<Pair<String, List<ToolRule>>> =
    listOf(
        "copper_axe" to
            listOf(
                ToolRule("minecraft:incorrect_for_copper_tool", false, null),
                ToolRule("minecraft:mineable/axe", true, 5.0f),
            ),
        "copper_hoe" to
            listOf(
                ToolRule("minecraft:incorrect_for_copper_tool", false, null),
                ToolRule("minecraft:mineable/hoe", true, 5.0f),
            ),
        "copper_pickaxe" to
            listOf(
                ToolRule("minecraft:incorrect_for_copper_tool", false, null),
                ToolRule("minecraft:mineable/pickaxe", true, 5.0f),
            ),
        "copper_shovel" to
            listOf(
                ToolRule("minecraft:incorrect_for_copper_tool", false, null),
                ToolRule("minecraft:mineable/shovel", true, 5.0f),
            ),
        "copper_sword" to
            listOf(
                ToolRule("minecraft:sword_instantly_mines", false, 3.4028235E38f),
                ToolRule("minecraft:sword_efficient", false, 1.5f),
            ),
        "diamond_axe" to
            listOf(
                ToolRule("minecraft:incorrect_for_diamond_tool", false, null),
                ToolRule("minecraft:mineable/axe", true, 8.0f),
            ),
        "diamond_hoe" to
            listOf(
                ToolRule("minecraft:incorrect_for_diamond_tool", false, null),
                ToolRule("minecraft:mineable/hoe", true, 8.0f),
            ),
        "diamond_pickaxe" to
            listOf(
                ToolRule("minecraft:incorrect_for_diamond_tool", false, null),
                ToolRule("minecraft:mineable/pickaxe", true, 8.0f),
            ),
        "diamond_shovel" to
            listOf(
                ToolRule("minecraft:incorrect_for_diamond_tool", false, null),
                ToolRule("minecraft:mineable/shovel", true, 8.0f),
            ),
        "diamond_sword" to
            listOf(
                ToolRule("minecraft:sword_instantly_mines", false, 3.4028235E38f),
                ToolRule("minecraft:sword_efficient", false, 1.5f),
            ),
        "golden_axe" to
            listOf(
                ToolRule("minecraft:incorrect_for_gold_tool", false, null),
                ToolRule("minecraft:mineable/axe", true, 12.0f),
            ),
        "golden_hoe" to
            listOf(
                ToolRule("minecraft:incorrect_for_gold_tool", false, null),
                ToolRule("minecraft:mineable/hoe", true, 12.0f),
            ),
        "golden_pickaxe" to
            listOf(
                ToolRule("minecraft:incorrect_for_gold_tool", false, null),
                ToolRule("minecraft:mineable/pickaxe", true, 12.0f),
            ),
        "golden_shovel" to
            listOf(
                ToolRule("minecraft:incorrect_for_gold_tool", false, null),
                ToolRule("minecraft:mineable/shovel", true, 12.0f),
            ),
        "golden_sword" to
            listOf(
                ToolRule("minecraft:sword_instantly_mines", false, 3.4028235E38f),
                ToolRule("minecraft:sword_efficient", false, 1.5f),
            ),
        "iron_axe" to
            listOf(
                ToolRule("minecraft:incorrect_for_iron_tool", false, null),
                ToolRule("minecraft:mineable/axe", true, 6.0f),
            ),
        "iron_hoe" to
            listOf(
                ToolRule("minecraft:incorrect_for_iron_tool", false, null),
                ToolRule("minecraft:mineable/hoe", true, 6.0f),
            ),
        "iron_pickaxe" to
            listOf(
                ToolRule("minecraft:incorrect_for_iron_tool", false, null),
                ToolRule("minecraft:mineable/pickaxe", true, 6.0f),
            ),
        "iron_shovel" to
            listOf(
                ToolRule("minecraft:incorrect_for_iron_tool", false, null),
                ToolRule("minecraft:mineable/shovel", true, 6.0f),
            ),
        "iron_sword" to
            listOf(
                ToolRule("minecraft:sword_instantly_mines", false, 3.4028235E38f),
                ToolRule("minecraft:sword_efficient", false, 1.5f),
            ),
        "netherite_axe" to
            listOf(
                ToolRule("minecraft:incorrect_for_netherite_tool", false, null),
                ToolRule("minecraft:mineable/axe", true, 9.0f),
            ),
        "netherite_hoe" to
            listOf(
                ToolRule("minecraft:incorrect_for_netherite_tool", false, null),
                ToolRule("minecraft:mineable/hoe", true, 9.0f),
            ),
        "netherite_pickaxe" to
            listOf(
                ToolRule("minecraft:incorrect_for_netherite_tool", false, null),
                ToolRule("minecraft:mineable/pickaxe", true, 9.0f),
            ),
        "netherite_shovel" to
            listOf(
                ToolRule("minecraft:incorrect_for_netherite_tool", false, null),
                ToolRule("minecraft:mineable/shovel", true, 9.0f),
            ),
        "netherite_sword" to
            listOf(
                ToolRule("minecraft:sword_instantly_mines", false, 3.4028235E38f),
                ToolRule("minecraft:sword_efficient", false, 1.5f),
            ),
        "shears" to
            listOf(
                ToolRule("minecraft:shears_extreme_breaking_speed", false, 15.0f),
                ToolRule("minecraft:shears_major_breaking_speed", false, 5.0f),
                ToolRule("minecraft:shears_minor_breaking_speed", false, 2.0f),
            ),
        "stone_axe" to
            listOf(
                ToolRule("minecraft:incorrect_for_stone_tool", false, null),
                ToolRule("minecraft:mineable/axe", true, 4.0f),
            ),
        "stone_hoe" to
            listOf(
                ToolRule("minecraft:incorrect_for_stone_tool", false, null),
                ToolRule("minecraft:mineable/hoe", true, 4.0f),
            ),
        "stone_pickaxe" to
            listOf(
                ToolRule("minecraft:incorrect_for_stone_tool", false, null),
                ToolRule("minecraft:mineable/pickaxe", true, 4.0f),
            ),
        "stone_shovel" to
            listOf(
                ToolRule("minecraft:incorrect_for_stone_tool", false, null),
                ToolRule("minecraft:mineable/shovel", true, 4.0f),
            ),
        "stone_sword" to
            listOf(
                ToolRule("minecraft:sword_instantly_mines", false, 3.4028235E38f),
                ToolRule("minecraft:sword_efficient", false, 1.5f),
            ),
        "wooden_axe" to
            listOf(
                ToolRule("minecraft:incorrect_for_wooden_tool", false, null),
                ToolRule("minecraft:mineable/axe", true, 2.0f),
            ),
        "wooden_hoe" to
            listOf(
                ToolRule("minecraft:incorrect_for_wooden_tool", false, null),
                ToolRule("minecraft:mineable/hoe", true, 2.0f),
            ),
        "wooden_pickaxe" to
            listOf(
                ToolRule("minecraft:incorrect_for_wooden_tool", false, null),
                ToolRule("minecraft:mineable/pickaxe", true, 2.0f),
            ),
        "wooden_shovel" to
            listOf(
                ToolRule("minecraft:incorrect_for_wooden_tool", false, null),
                ToolRule("minecraft:mineable/shovel", true, 2.0f),
            ),
        "wooden_sword" to
            listOf(
                ToolRule("minecraft:sword_instantly_mines", false, 3.4028235E38f),
                ToolRule("minecraft:sword_efficient", false, 1.5f),
            ),
    )
