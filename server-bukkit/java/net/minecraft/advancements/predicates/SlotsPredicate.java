// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.advancements.predicates;
import com.mojang.serialization.Codec;
import it.unimi.dsi.fastutil.ints.IntList;
import java.util.Map;
import net.minecraft.world.entity.SlotAccess;
import net.minecraft.world.entity.SlotProvider;
import net.minecraft.world.inventory.SlotRange;
import net.minecraft.world.inventory.SlotRanges;
public record SlotsPredicate(Map<SlotRange, ItemPredicate> slots) {
    public static final Codec<SlotsPredicate> CODEC = Codec.unboundedMap(SlotRanges.CODEC, ItemPredicate.CODEC)
        .xmap(SlotsPredicate::new, SlotsPredicate::slots);
    public boolean matches(SlotProvider provider) {
        for (Map.Entry<SlotRange, ItemPredicate> entry : slots.entrySet()) {
            if (!anyMatches(provider, entry.getValue(), entry.getKey().slots())) {
                return false;
            }
        }
        return true;
    }
    private static boolean anyMatches(SlotProvider provider, ItemPredicate test, IntList slots) {
        for (int i = 0; i < slots.size(); i++) {
            SlotAccess access = provider.getSlot(slots.getInt(i));
            if (access != null && test.test(access.get())) {
                return true;
            }
        }
        return false;
    }
}
