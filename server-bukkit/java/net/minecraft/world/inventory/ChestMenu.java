// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.inventory;

import net.minecraft.world.Container;
import net.minecraft.world.SimpleContainer;
import net.minecraft.world.entity.player.Inventory;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;

public class ChestMenu extends AbstractContainerMenu {
    private final Container container;
    private final int containerRows;

    private ChestMenu(MenuType<?> menuType, int containerId, Inventory inventory, int rows) {
        this(menuType, containerId, inventory, new SimpleContainer(9 * rows), rows);
    }

    public static ChestMenu oneRow(int containerId, Inventory inventory) {
        return new ChestMenu(MenuType.GENERIC_9x1, containerId, inventory, 1);
    }

    public static ChestMenu twoRows(int containerId, Inventory inventory) {
        return new ChestMenu(MenuType.GENERIC_9x2, containerId, inventory, 2);
    }

    public static ChestMenu threeRows(int containerId, Inventory inventory) {
        return new ChestMenu(MenuType.GENERIC_9x3, containerId, inventory, 3);
    }

    public static ChestMenu fourRows(int containerId, Inventory inventory) {
        return new ChestMenu(MenuType.GENERIC_9x4, containerId, inventory, 4);
    }

    public static ChestMenu fiveRows(int containerId, Inventory inventory) {
        return new ChestMenu(MenuType.GENERIC_9x5, containerId, inventory, 5);
    }

    public static ChestMenu sixRows(int containerId, Inventory inventory) {
        return new ChestMenu(MenuType.GENERIC_9x6, containerId, inventory, 6);
    }

    public static ChestMenu threeRows(int containerId, Inventory inventory, Container container) {
        return new ChestMenu(MenuType.GENERIC_9x3, containerId, inventory, container, 3);
    }

    public static ChestMenu sixRows(int containerId, Inventory inventory, Container container) {
        return new ChestMenu(MenuType.GENERIC_9x6, containerId, inventory, container, 6);
    }

    public ChestMenu(MenuType<?> menuType, int containerId, Inventory inventory, Container container, int rows) {
        super(menuType, containerId);
        checkContainerSize(container, rows * 9);
        this.container = container;
        this.containerRows = rows;
        for (int row = 0; row < rows; row++) {
            for (int column = 0; column < 9; column++) {
                addSlot(new Slot(container, column + row * 9, 8 + column * 18, 18 + row * 18));
            }
        }
        addStandardInventorySlots(inventory, 8, 103 + (rows - 4) * 18);
    }

    public Container getContainer() {
        return container;
    }

    public int getRowCount() {
        return containerRows;
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
        int containerSlots = containerRows * 9;
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
