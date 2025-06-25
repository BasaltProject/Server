package org.basaltmc.server.registry

/**
 * What each item adds to the hand that holds it: attack damage and attack speed, as
 * `name|damage|speed`. Both are the modifier the item carries, not the total: the player's
 * own base of one damage and four speed is added on top.
 *
 * From the item component report the data generator writes (see `basalt.datagen`).
 */
internal val ALL_ITEM_WEAPONS: List<String> =
    listOf(
        "copper_axe|8.0|-3.200000047683716",
        "copper_hoe|0.0|-2.0",
        "copper_pickaxe|2.0|-2.799999952316284",
        "copper_shovel|2.5|-3.0",
        "copper_spear|1.0|-2.8235294818878174",
        "copper_sword|4.0|-2.4000000953674316",
        "diamond_axe|8.0|-3.0",
        "diamond_pickaxe|4.0|-2.799999952316284",
        "diamond_shovel|4.5|-3.0",
        "diamond_spear|3.0|-3.0476189851760864",
        "diamond_sword|6.0|-2.4000000953674316",
        "golden_axe|6.0|-3.0",
        "golden_hoe|0.0|-3.0",
        "golden_pickaxe|1.0|-2.799999952316284",
        "golden_shovel|1.5|-3.0",
        "golden_spear|0.0|-2.9473683834075928",
        "golden_sword|3.0|-2.4000000953674316",
        "iron_axe|8.0|-3.0999999046325684",
        "iron_hoe|0.0|-1.0",
        "iron_pickaxe|3.0|-2.799999952316284",
        "iron_shovel|3.5|-3.0",
        "iron_spear|2.0|-2.9473683834075928",
        "iron_sword|5.0|-2.4000000953674316",
        "mace|5.0|-3.4000000953674316",
        "netherite_axe|9.0|-3.0",
        "netherite_pickaxe|5.0|-2.799999952316284",
        "netherite_shovel|5.5|-3.0",
        "netherite_spear|4.0|-3.13043475151062",
        "netherite_sword|7.0|-2.4000000953674316",
        "stone_axe|8.0|-3.200000047683716",
        "stone_hoe|0.0|-2.0",
        "stone_pickaxe|2.0|-2.799999952316284",
        "stone_shovel|2.5|-3.0",
        "stone_spear|1.0|-2.666666626930237",
        "stone_sword|4.0|-2.4000000953674316",
        "trident|8.0|-2.9000000953674316",
        "wooden_axe|6.0|-3.200000047683716",
        "wooden_hoe|0.0|-3.0",
        "wooden_pickaxe|1.0|-2.799999952316284",
        "wooden_shovel|1.5|-3.0",
        "wooden_spear|0.0|-2.4615384340286255",
        "wooden_sword|3.0|-2.4000000953674316",
    )
