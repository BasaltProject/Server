// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.inventory;

import java.util.ArrayList;
import java.util.List;
import java.util.function.Predicate;
import net.minecraft.world.item.ItemStack;

public class ItemCombinerMenuSlotDefinition {
    private final List<SlotDefinition> slots;
    private final SlotDefinition resultSlot;

    private ItemCombinerMenuSlotDefinition(List<SlotDefinition> inputSlots, SlotDefinition resultSlot) {
        this.slots = List.copyOf(inputSlots);
        this.resultSlot = resultSlot;
    }

    public static Builder create() {
        return new Builder();
    }

    public SlotDefinition getSlot(int index) {
        return slots.get(index);
    }

    public SlotDefinition getResultSlot() {
        return resultSlot;
    }

    public List<SlotDefinition> getSlots() {
        return slots;
    }

    public int getNumOfInputSlots() {
        return slots.size();
    }

    public int getResultSlotIndex() {
        return slots.size();
    }

    public static class Builder {
        private final List<SlotDefinition> slots = new ArrayList<>();
        private SlotDefinition resultSlot = new SlotDefinition(0, 0, 0, stack -> true);

        public Builder() {}

        public Builder withSlot(int slotIndex, int xPlacement, int yPlacement, Predicate<ItemStack> mayPlace) {
            slots.add(new SlotDefinition(slotIndex, xPlacement, yPlacement, mayPlace));
            return this;
        }

        public Builder withResultSlot(int slotIndex, int xPlacement, int yPlacement) {
            resultSlot = new SlotDefinition(slotIndex, xPlacement, yPlacement, stack -> false);
            return this;
        }

        public ItemCombinerMenuSlotDefinition build() {
            return new ItemCombinerMenuSlotDefinition(slots, resultSlot);
        }
    }

    public static record SlotDefinition(int slotIndex, int x, int y, Predicate<ItemStack> mayPlace) {}
}
