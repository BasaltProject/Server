// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.fixes;
import java.util.Map;
public class RecipesRenameningFix {
    public static final Map<String, String> RECIPES = org.basaltmc.bukkit.facade.DatafixTables.pairs("minecraft:acacia_bark=minecraft:acacia_wood minecraft:birch_bark=minecraft:birch_wood minecraft:dark_oak_bark=minecraft:dark_oak_wood minecraft:jungle_bark=minecraft:jungle_wood minecraft:oak_bark=minecraft:oak_wood minecraft:spruce_bark=minecraft:spruce_wood");
}
