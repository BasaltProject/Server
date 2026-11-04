// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.enchantment;
import java.util.function.Consumer;
import net.minecraft.world.entity.EquipmentSlot;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.item.ItemStack;
public record EnchantedItemInUse(ItemStack itemStack, EquipmentSlot inSlot, LivingEntity owner, Consumer<ItemStack> onBreak) {
    public EnchantedItemInUse(ItemStack itemStack, EquipmentSlot inSlot, LivingEntity owner) {
        this(itemStack, inSlot, owner, null);
    }
}
