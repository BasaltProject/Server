// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.inventory;

import net.minecraft.world.Container;
import net.minecraft.world.SimpleContainer;
import net.minecraft.world.entity.player.Inventory;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.block.Blocks;

public class EnchantmentMenu extends AbstractContainerMenu {
    private final Container container;
    private final ContainerLevelAccess access;
    private final ContainerData data;

    public EnchantmentMenu(int containerId, Inventory inventory) {
        this(containerId, inventory, ContainerLevelAccess.NULL);
    }

    public EnchantmentMenu(int containerId, Inventory inventory, ContainerLevelAccess access) {
        super(MenuType.ENCHANTMENT, containerId);
        this.container = new SimpleContainer(2);
        this.access = access;
        this.data = new SimpleContainerData(0);
        addSlot(new Slot(container, 0, 15, 47));
        addSlot(new Slot(container, 1, 35, 47));
        addStandardInventorySlots(inventory, 8, 84);
    }

    public Container getContainer() {
        return container;
    }

    @Override
    public boolean stillValid(Player player) {
        return access == ContainerLevelAccess.NULL || stillValid(access, player, Blocks.ENCHANTING_TABLE);
    }

    @Override
    public void removed(Player player) {
        super.removed(player);
        for (int slot = 0; slot < container.getContainerSize(); slot++) {
            if (!container.getItem(slot).isEmpty() && !isResult(slot)) {
                player.getInventory().add(container.removeItemNoUpdate(slot));
            }
        }
    }

    private static boolean isResult(int slot) {
        for (int result : new int[] {-1}) {
            if (result == slot) {
                return true;
            }
        }
        return false;
    }

    @Override
    public ItemStack quickMoveStack(Player player, int slotIndex) {
        Slot slot = slots.get(slotIndex);
        if (slot == null || !slot.hasItem()) {
            return ItemStack.EMPTY;
        }
        ItemStack stack = slot.getItem();
        ItemStack original = stack.copy();
        int own = 2;
        boolean moved = slotIndex < own ? moveItemStackTo(stack, own, slots.size(), true)
            : moveItemStackTo(stack, 0, own, false);
        if (!moved) {
            return ItemStack.EMPTY;
        }
        if (stack.isEmpty()) {
            slot.setByPlayer(ItemStack.EMPTY);
        } else {
            slot.setChanged();
        }
        return original;
    }
}
