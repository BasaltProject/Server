// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity;

import it.unimi.dsi.fastutil.ints.IntList;
import net.minecraft.world.item.slot.SlotCollection;
public interface SlotProvider {
    default SlotAccess getSlot(int slot) {
        return null;
    }
    default SlotCollection getSlotsFromRange(IntList slots) {
        return SlotCollection.of(slots.intStream().mapToObj(this::getSlot).filter(java.util.Objects::nonNull).toList());
    }
}
