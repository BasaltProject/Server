// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.inventory;
import it.unimi.dsi.fastutil.ints.IntList;
import net.minecraft.util.StringRepresentable;
public interface SlotRange extends StringRepresentable {
    IntList slots();
    default int size() {
        return slots().size();
    }
    static SlotRange of(String name, IntList slots) {
        return new SlotRange() {
            public IntList slots() {
                return slots;
            }
            public String getSerializedName() {
                return name;
            }
        };
    }
}
