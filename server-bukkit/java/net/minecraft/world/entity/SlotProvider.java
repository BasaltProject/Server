// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity;

import it.unimi.dsi.fastutil.ints.IntList;
import net.minecraft.world.item.slot.SlotCollection;
public interface SlotProvider {
    default SlotAccess getSlot(int slot) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.entity.SlotProvider#getSlot");
    }
    default SlotCollection getSlotsFromRange(IntList slots) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.entity.SlotProvider#getSlotsFromRange");
    }
}
