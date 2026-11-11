// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity;
import net.minecraft.world.entity.item.ItemEntity;
import com.mojang.serialization.Codec;
import java.util.EnumMap;
import net.minecraft.world.item.ItemStack;
public class EntityEquipment {
    private EnumMap<EquipmentSlot, ItemStack> items = new EnumMap<>(EquipmentSlot.class);
    public static final Codec<EntityEquipment> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.entity.EntityEquipment.class, "CODEC");
    private EntityEquipment(EnumMap<EquipmentSlot, ItemStack> items) {
        this.items = items;
    }
    public EntityEquipment() {
    }
    public ItemStack set(EquipmentSlot slot, ItemStack itemStack) {
        ItemStack previous = items.put(slot, itemStack);
        return previous == null ? ItemStack.EMPTY : previous;
    }
    public ItemStack get(EquipmentSlot slot) {
        return items.getOrDefault(slot, ItemStack.EMPTY);
    }
    public boolean isEmpty() {
        return items.values().stream().allMatch(ItemStack::isEmpty);
    }
    public void tick(Entity owner) {
        items.forEach((slot, stack) -> {
            if (!stack.isEmpty()) {
                stack.inventoryTick(owner.level(), owner, slot);
            }
        });
    }
    public void setAll(EntityEquipment equipment) {
        items.clear();
        items.putAll(equipment.items);
    }
    public void dropAll(LivingEntity dropper) {
        for (ItemStack stack : items.values()) {
            ItemEntity drop = dropper.createItemStackToDrop(stack, true, false);
            if (drop != null) {
                dropper.level().addFreshEntity(drop);
            }
        }
        clear();
    }
    public void clear() {
        items.replaceAll((slot, stack) -> ItemStack.EMPTY);
    }
}
