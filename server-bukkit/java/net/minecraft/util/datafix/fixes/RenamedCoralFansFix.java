// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.fixes;
import java.util.Map;
public class RenamedCoralFansFix {
    public static final Map<String, String> RENAMED_IDS = org.basaltmc.bukkit.facade.DatafixTables.pairs("minecraft:tube_coral_fan=minecraft:tube_coral_wall_fan minecraft:brain_coral_fan=minecraft:brain_coral_wall_fan minecraft:bubble_coral_fan=minecraft:bubble_coral_wall_fan minecraft:fire_coral_fan=minecraft:fire_coral_wall_fan minecraft:horn_coral_fan=minecraft:horn_coral_wall_fan");
}
