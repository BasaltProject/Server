package org.basaltmc.server.registry

/**
 * Every edible item: `<item>|<nutrition>|<saturation>|<can always eat>|<seconds to eat>|
 * <what eating it does>|<what it leaves behind>`, from the real item component reports.
 *
 * The last field is `apply:<effect>,<ticks>,<amplifier>,<probability>`, `remove:<effect>`
 * or `clear`, several separated by `;`: what a golden apple, a bottle of honey and a bucket
 * of milk respectively do.
 */
internal val ALL_ITEM_FOOD: List<String> =
    listOf(
        "apple|4|2.4|false|1.6||",
        "baked_potato|5|6.0|false|1.6||",
        "beef|3|1.8000001|false|1.6||",
        "beetroot|1|1.2|false|1.6||",
        "beetroot_soup|6|7.2000003|false|1.6||bowl",
        "bread|5|6.0|false|1.6||",
        "carrot|3|3.6000001|false|1.6||",
        "chicken|2|1.2|false|1.6|apply:hunger,600,0,0.3|",
        "chorus_fruit|4|2.4|true|1.6||",
        "cod|2|0.4|false|1.6||",
        "cooked_beef|8|12.8|false|1.6||",
        "cooked_chicken|6|7.2000003|false|1.6||",
        "cooked_cod|5|6.0|false|1.6||",
        "cooked_mutton|6|9.6|false|1.6||",
        "cooked_porkchop|8|12.8|false|1.6||",
        "cooked_rabbit|5|6.0|false|1.6||",
        "cooked_salmon|6|9.6|false|1.6||",
        "cookie|2|0.4|false|1.6||",
        "dried_kelp|1|0.6|false|0.8||",
        "enchanted_golden_apple|4|9.6|true|1.6|apply:regeneration,400,1,1.0;apply:resistance,6000,0,1.0;apply:fire_resistance,6000,0,1.0;apply:absorption,2400,3,1.0|",
        "glow_berries|2|0.4|false|1.6||",
        "golden_apple|4|9.6|true|1.6|apply:regeneration,100,1,1.0;apply:absorption,2400,0,1.0|",
        "golden_carrot|6|14.400001|false|1.6||",
        "honey_bottle|6|1.2|true|2.0|remove:poison|glass_bottle",
        "melon_slice|2|1.2|false|1.6||",
        "milk_bucket|0|0.0|false|1.6|clear|bucket",
        "mushroom_stew|6|7.2000003|false|1.6||bowl",
        "mutton|2|1.2|false|1.6||",
        "ominous_bottle|0|0.0|false|1.6||",
        "poisonous_potato|2|1.2|false|1.6|apply:poison,100,0,0.6|",
        "porkchop|3|1.8000001|false|1.6||",
        "potato|1|0.6|false|1.6||",
        "pufferfish|1|0.2|false|1.6|apply:poison,1200,1,1.0;apply:hunger,300,2,1.0;apply:nausea,300,0,1.0|",
        "pumpkin_pie|8|4.8|false|1.6||",
        "rabbit|3|1.8000001|false|1.6||",
        "rabbit_stew|10|12.0|false|1.6||bowl",
        "rotten_flesh|4|0.8|false|1.6|apply:hunger,600,0,0.8|",
        "salmon|2|0.4|false|1.6||",
        "spider_eye|2|3.2|false|1.6|apply:poison,100,0,1.0|",
        "suspicious_stew|6|7.2000003|true|1.6||bowl",
        "sweet_berries|2|0.4|false|1.6||",
        "tropical_fish|1|0.2|false|1.6||",
    )
