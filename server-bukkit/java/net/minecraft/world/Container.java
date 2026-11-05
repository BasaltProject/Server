// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world;
import java.util.Iterator;
import java.util.List;
import java.util.Set;
import java.util.function.Predicate;
import net.minecraft.world.entity.ContainerUser;
import net.minecraft.world.entity.SlotAccess;
import net.minecraft.world.entity.SlotProvider;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.block.entity.BlockEntity;
public interface Container extends Clearable, Iterable<ItemStack>, SlotProvider {
    float DEFAULT_DISTANCE_BUFFER = 4.0F;
    int getContainerSize();
    boolean isEmpty();
    ItemStack getItem(int slot);
    ItemStack removeItem(int slot, int count);
    ItemStack removeItemNoUpdate(int slot);
    void setItem(int slot, ItemStack itemStack);
    default int getMaxStackSize() {
        return 99;
    }
    default int getMaxStackSize(ItemStack itemStack) {
        return Math.min(getMaxStackSize(), itemStack.getMaxStackSize());
    }
    void setChanged();
    boolean stillValid(Player player);
    default void startOpen(ContainerUser containerUser) {

    }
    default void stopOpen(ContainerUser containerUser) {

    }
    default List<ContainerUser> getEntitiesWithContainerOpen() {
        return java.util.List.of();
    }
    default boolean canPlaceItem(int slot, ItemStack itemStack) {
        return true;
    }
    default boolean canTakeItem(Container into, int slot, ItemStack itemStack) {
        return true;
    }
    default int countItem(Item item) {
        int total = 0;
        for (ItemStack stack : this) {
            if (stack.getItem() == item) {
                total += stack.getCount();
            }
        }
        return total;
    }
    default boolean hasAnyOf(Set<Item> item) {
        for (ItemStack stack : this) {
            if (item.contains(stack.getItem())) {
                return true;
            }
        }
        return false;
    }
    default boolean hasAnyMatching(Predicate<ItemStack> predicate) {
        for (ItemStack stack : this) {
            if (predicate.test(stack)) {
                return true;
            }
        }
        return false;
    }
    static boolean stillValidBlockEntity(BlockEntity blockEntity, Player player) {
        return stillValidBlockEntity(blockEntity, player, DEFAULT_DISTANCE_BUFFER);
    }
    static boolean stillValidBlockEntity(BlockEntity blockEntity, Player player, float distanceBuffer) {
        if (blockEntity.getLevel() == null || blockEntity.getLevel().getBlockEntity(blockEntity.getBlockPos()) != blockEntity) {
            return false;
        }
        return player.distanceToSqr(blockEntity.getBlockPos().getX() + 0.5, blockEntity.getBlockPos().getY() + 0.5,
            blockEntity.getBlockPos().getZ() + 0.5) <= (player.blockInteractionRange() + distanceBuffer)
            * (player.blockInteractionRange() + distanceBuffer);
    }
    default SlotAccess getSlot(int slot) {
        if (slot < 0 || slot >= getContainerSize()) {
            return null;
        }
        return SlotAccess.of(() -> getItem(slot), stack -> setItem(slot, stack));
    }
    default Iterator<ItemStack> iterator() {
        return new ContainerIterator(this);
    }
    public static class ContainerIterator implements Iterator<ItemStack> {
        private final Container container;
        private int index;
        public ContainerIterator(Container container) {
            this.container = container;
        }
        public boolean hasNext() {
            return index < container.getContainerSize();
        }
        public ItemStack next() {
            if (!hasNext()) {
                throw new java.util.NoSuchElementException();
            }
            return container.getItem(index++);
        }
    }
}
