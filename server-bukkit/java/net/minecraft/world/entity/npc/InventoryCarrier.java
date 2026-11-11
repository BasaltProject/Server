// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.npc;
import net.minecraft.world.item.ItemStack;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.SimpleContainer;
import net.minecraft.world.entity.Mob;
import net.minecraft.world.entity.item.ItemEntity;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
public interface InventoryCarrier {
    String TAG_INVENTORY = "Inventory";
    SimpleContainer getInventory();
    static void pickUpItem(ServerLevel level, Mob mob, InventoryCarrier inventoryCarrier, ItemEntity itemEntity) {
        ItemStack offered = itemEntity.getItem();
        if (!mob.wantsToPickUp(level, offered)) {
            return;
        }
        SimpleContainer bag = inventoryCarrier.getInventory();
        if (!bag.canAddItem(offered)) {
            return;
        }
        mob.onItemPickup(itemEntity);
        int before = offered.getCount();
        ItemStack leftover = bag.addItem(offered);
        mob.take(itemEntity, before - leftover.getCount());
        if (leftover.isEmpty()) {
            itemEntity.discard();
        } else {
            offered.setCount(leftover.getCount());
        }
    }
    default void readInventoryFromTag(ValueInput input) {
    }
    default void writeInventoryToTag(ValueOutput output) {
    }
}
