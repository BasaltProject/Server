// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.fixes;
import java.util.Map;
public class DyeItemRenameFix {
    public static final Map<String, String> RENAMED_IDS = org.basaltmc.bukkit.facade.DatafixTables.pairs("minecraft:cactus_green=minecraft:green_dye minecraft:rose_red=minecraft:red_dye minecraft:dandelion_yellow=minecraft:yellow_dye");
}
