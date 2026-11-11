// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.recipes;

import java.util.Locale;

public enum RecipeCategory {
    BUILDING_BLOCKS, DECORATIONS, REDSTONE, TRANSPORTATION, TOOLS, COMBAT, FOOD, BREWING, MISC;

    public String getFolderName() {
        return name().toLowerCase(Locale.ROOT);
    }
}
