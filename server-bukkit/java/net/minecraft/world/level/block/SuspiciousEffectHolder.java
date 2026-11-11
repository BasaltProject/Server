// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;
import java.util.List;
import net.minecraft.world.item.component.SuspiciousStewEffects;
import net.minecraft.world.level.ItemLike;
public interface SuspiciousEffectHolder {
    SuspiciousStewEffects getSuspiciousEffects();
    static List<SuspiciousEffectHolder> getAllEffectHolders() {
        List<SuspiciousEffectHolder> holders = new java.util.ArrayList<>();
        for (net.minecraft.world.item.Item item : net.minecraft.core.registries.BuiltInRegistries.ITEM) {
            SuspiciousEffectHolder holder = tryGet(item);
            if (holder != null) {
                holders.add(holder);
            }
        }
        return holders;
    }
    static SuspiciousEffectHolder tryGet(ItemLike item) {
        if (item.asItem() instanceof net.minecraft.world.item.BlockItem blockItem && blockItem.getBlock() instanceof SuspiciousEffectHolder holder) {
            return holder;
        }
        return item.asItem() instanceof SuspiciousEffectHolder holder ? holder : null;


    }
}
