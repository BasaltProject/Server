// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.inventory;

import net.minecraft.world.Container;
import net.minecraft.world.SimpleContainer;
import net.minecraft.world.entity.player.Inventory;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;

public class HopperMenu extends AbstractContainerMenu {
    private final Container container;

    public HopperMenu(int containerId, Inventory inventory) {
        this(containerId, inventory, new SimpleContainer(5));
    }

    public HopperMenu(int containerId, Inventory inventory, Container container) {
        super(MenuType.HOPPER, containerId);
        checkContainerSize(container, 5);
        this.container = container;
        for (int row = 0; row < 1; row++) {
            for (int column = 0; column < 5; column++) {
                addSlot(new Slot(container, column + row * 5, 44 + column * 18, 20 + row * 18));
            }
        }
        addStandardInventorySlots(inventory, 8, 51);
    }

    @Override
    public boolean stillValid(Player player) {
        return container.stillValid(player);
    }

    @Override
    public ItemStack quickMoveStack(Player player, int slotIndex) {
        Slot slot = slots.get(slotIndex);
        if (slot == null || !slot.hasItem()) {
            return ItemStack.EMPTY;
        }
        ItemStack stack = slot.getItem();
        ItemStack original = stack.copy();
        int containerSlots = 5;
        if (slotIndex < containerSlots) {
            if (!moveItemStackTo(stack, containerSlots, slots.size(), true)) {
                return ItemStack.EMPTY;
            }
        } else if (!moveItemStackTo(stack, 0, containerSlots, false)) {
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
