// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity;

import java.util.List;
import java.util.function.Consumer;
import java.util.function.Predicate;
import java.util.function.Supplier;
import net.minecraft.world.item.ItemStack;

public interface SlotAccess {
    ItemStack get();

    boolean set(ItemStack itemStack);

    static SlotAccess of(Supplier<ItemStack> getter, Consumer<ItemStack> setter) {
        return new SlotAccess() {
            public ItemStack get() {
                return getter.get();
            }

            public boolean set(ItemStack itemStack) {
                setter.accept(itemStack);
                return true;
            }
        };
    }

    static SlotAccess forEquipmentSlot(LivingEntity entity, EquipmentSlot slot, Predicate<ItemStack> validator) {
        return new SlotAccess() {
            public ItemStack get() {
                return entity.getItemBySlot(slot);
            }

            public boolean set(ItemStack itemStack) {
                if (!validator.test(itemStack)) {
                    return false;
                }
                entity.setItemSlot(slot, itemStack);
                return true;
            }
        };
    }

    static SlotAccess forEquipmentSlot(LivingEntity entity, EquipmentSlot slot) {
        return forEquipmentSlot(entity, slot, stack -> true);
    }

    static SlotAccess forListElement(List<ItemStack> stacks, int index) {
        return new SlotAccess() {
            public ItemStack get() {
                return stacks.get(index);
            }

            public boolean set(ItemStack itemStack) {
                stacks.set(index, itemStack);
                return true;
            }
        };
    }
}
