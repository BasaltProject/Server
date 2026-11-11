// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.fixes;
import com.mojang.datafixers.DataFix;
import com.mojang.datafixers.TypeRewriteRule;
import com.mojang.datafixers.schemas.Schema;
public class ItemShulkerBoxColorFix extends DataFix {
    public static final String[] NAMES_BY_COLOR = org.basaltmc.bukkit.facade.DatafixTables.pairs("0=minecraft:white_shulker_box 1=minecraft:orange_shulker_box 2=minecraft:magenta_shulker_box 3=minecraft:light_blue_shulker_box 4=minecraft:yellow_shulker_box 5=minecraft:lime_shulker_box 6=minecraft:pink_shulker_box 7=minecraft:gray_shulker_box 8=minecraft:silver_shulker_box 9=minecraft:cyan_shulker_box 10=minecraft:purple_shulker_box 11=minecraft:blue_shulker_box 12=minecraft:brown_shulker_box 13=minecraft:green_shulker_box 14=minecraft:red_shulker_box 15=minecraft:black_shulker_box").values().toArray(new String[0]);
    public ItemShulkerBoxColorFix(Schema outputSchema, boolean changesType) {
        super(outputSchema, changesType);
    }
    public TypeRewriteRule makeRule() {
        return org.basaltmc.bukkit.facade.NativeStepRule.of(getClass().getSimpleName(), getVersionKey());
    }
}
