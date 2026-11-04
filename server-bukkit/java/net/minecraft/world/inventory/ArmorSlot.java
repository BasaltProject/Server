// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.inventory;

import net.minecraft.resources.Identifier;
import net.minecraft.world.Container;
import net.minecraft.world.entity.EquipmentSlot;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;
import org.basaltmc.bukkit.item.ItemData;

public class ArmorSlot extends Slot {
    private final LivingEntity owner;
    private final EquipmentSlot slot;
    private final Identifier emptyIcon;

    public ArmorSlot(Container inventory, LivingEntity owner, EquipmentSlot slot, int slotIndex, int x, int y, Identifier emptyIcon) {
        super(inventory, slotIndex, x, y);
        this.owner = owner;
        this.slot = slot;
        this.emptyIcon = emptyIcon;
    }

    @Override
    public void setByPlayer(ItemStack itemStack, ItemStack previous) {
        set(itemStack);
    }

    @Override
    public int getMaxStackSize() {
        return 1;
    }

    @Override
    public boolean mayPlace(ItemStack itemStack) {
        if (itemStack.isEmpty()) {
            return false;
        }
        String wanted = switch (slot) {
            case HEAD -> "head";
            case CHEST -> "chest";
            case LEGS -> "legs";
            case FEET -> "feet";
            default -> null;
        };
        return wanted != null && wanted.equals(org.basaltmc.bukkit.item.Nbt.string(
            ItemData.of(itemStack.getItem().itemKey().toString()).armor(), "slot", null));
    }

    @Override
    public boolean isActive() {
        return owner.isAlive();
    }

    @Override
    public boolean mayPickup(Player player) {
        return true;
    }

    @Override
    public Identifier getNoItemIcon() {
        return emptyIcon;
    }
}
