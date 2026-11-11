// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.ticks;
import net.minecraft.world.Container;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.block.entity.BlockEntity;
public interface ContainerSingleItem extends Container {
    ItemStack getTheItem();
    default ItemStack splitTheItem(int count) {
        return getTheItem().split(count);
    }
    void setTheItem(ItemStack itemStack);
    default ItemStack removeTheItem() {
        return splitTheItem(getMaxStackSize());
    }
    default int getContainerSize() {
        return 1;
    }
    default boolean isEmpty() {
        return getTheItem().isEmpty();
    }
    default void clearContent() {
        removeTheItem();
    }
    default ItemStack removeItemNoUpdate(int slot) {
        return removeItem(slot, getMaxStackSize());
    }
    default ItemStack getItem(int slot) {
        return slot == 0 ? getTheItem() : ItemStack.EMPTY;
    }
    default ItemStack removeItem(int slot, int count) {
        return slot == 0 ? splitTheItem(count) : ItemStack.EMPTY;
    }
    default void setItem(int slot, ItemStack itemStack) {
        if (slot == 0) {
            setTheItem(itemStack);
        }
    }
    public interface BlockContainerSingleItem extends ContainerSingleItem {
        BlockEntity getContainerBlockEntity();
        default boolean stillValid(Player player) {
            return Container.stillValidBlockEntity(getContainerBlockEntity(), player);
        }
    }
}
