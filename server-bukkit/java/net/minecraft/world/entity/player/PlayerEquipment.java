// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.player;
import net.minecraft.world.entity.EntityEquipment;
import net.minecraft.world.entity.EquipmentSlot;
import net.minecraft.world.item.ItemStack;
public class PlayerEquipment extends EntityEquipment {
    private final Player player;
    private int indexOf(EquipmentSlot slot) {
        return slot == EquipmentSlot.MAINHAND ? player.getInventory().getSelectedSlot() : slot.getIndex(36);
    }
    public PlayerEquipment(Player player) {
        super();
        this.player = player;
    }
    public ItemStack set(EquipmentSlot slot, ItemStack itemStack) {
        ItemStack previous = get(slot);
        player.getInventory().setItem(indexOf(slot), itemStack);
        return previous;
    }
    public ItemStack get(EquipmentSlot slot) {
        return player.getInventory().getItem(indexOf(slot));
    }
    public boolean isEmpty() {
        for (EquipmentSlot slot : EquipmentSlot.values()) {
            if (!get(slot).isEmpty()) {
                return false;
            }
        }
        return true;
    }
}
