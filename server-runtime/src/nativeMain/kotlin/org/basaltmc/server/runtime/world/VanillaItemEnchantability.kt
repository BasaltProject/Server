package org.basaltmc.server.runtime.world

/** The default `minecraft:enchantable` component on vanilla 26.2 items. */
internal object VanillaItemEnchantability {
    fun valueOf(item: String): Int? {
        val path = item.removePrefix("minecraft:")
        SPECIAL[path]?.let { return it }
        armorValue(path)?.let { return it }
        return toolValue(path)
    }

    private fun armorValue(path: String): Int? {
        if (ARMOR_SUFFIXES.none(path::endsWith)) return null
        return when {
            path.startsWith("leather_") -> 15
            path.startsWith("copper_") -> 8
            path.startsWith("chainmail_") -> 12
            path.startsWith("iron_") -> 9
            path.startsWith("golden_") -> 25
            path.startsWith("diamond_") -> 10
            path.startsWith("netherite_") -> 15
            else -> null
        }
    }

    private fun toolValue(path: String): Int? {
        if (TOOL_SUFFIXES.none(path::endsWith)) return null
        return when {
            path.startsWith("wooden_") -> 15
            path.startsWith("stone_") -> 5
            path.startsWith("copper_") -> 13
            path.startsWith("iron_") -> 14
            path.startsWith("golden_") -> 22
            path.startsWith("diamond_") -> 10
            path.startsWith("netherite_") -> 15
            else -> null
        }
    }

    private val ARMOR_SUFFIXES = listOf("_helmet", "_chestplate", "_leggings", "_boots")
    private val TOOL_SUFFIXES = listOf("_sword", "_shovel", "_pickaxe", "_axe", "_hoe", "_spear")
    private val SPECIAL =
        mapOf(
            "book" to 1,
            "bow" to 1,
            "crossbow" to 1,
            "fishing_rod" to 1,
            "mace" to 15,
            "trident" to 1,
            "turtle_helmet" to 9,
        )
}
