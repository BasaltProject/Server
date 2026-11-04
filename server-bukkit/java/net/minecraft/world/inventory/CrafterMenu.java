// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.inventory;

import net.minecraft.world.Container;
import net.minecraft.world.entity.player.Inventory;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;

public class CrafterMenu extends AbstractContainerMenu implements ContainerListener {
    protected static final int SLOT_COUNT = 9;
    private static final int kInventoryStart = 9;
    private static final int kInventoryEnd = 45;
    private static final int kResultSlot = 45;
    private static final int kPoweredData = 9;
    private final ResultContainer resultContainer = new ResultContainer();
    private final ContainerData containerData;
    private final Player player;
    private final CraftingContainer container;

    public CrafterMenu(int containerId, Inventory inventory) {
        this(containerId, inventory, new TransientCraftingContainer(null, 3, 3), new SimpleContainerData(10));
    }

    public CrafterMenu(int containerId, Inventory inventory, CraftingContainer container, ContainerData containerData) {
        super(MenuType.CRAFTER_3x3, containerId);
        this.player = inventory.player;
        this.containerData = containerData;
        this.container = container;
        checkContainerSize(container, SLOT_COUNT);
        container.startOpen(player);
        for (int row = 0; row < 3; row++) {
            for (int column = 0; column < 3; column++) {
                addSlot(new CrafterSlot(container, column + row * 3, 26 + column * 18, 17 + row * 18, this));
            }
        }
        addStandardInventorySlots(inventory, 8, 84);
        addSlot(new NonInteractiveResultSlot(resultContainer, 0, 134, 35));
        addDataSlots(containerData);
    }

    public void setSlotState(int slotId, boolean isEnabled) {
        if (slotId < SLOT_COUNT && getSlot(slotId) instanceof CrafterSlot) {
            containerData.set(slotId, isEnabled ? 0 : 1);
            broadcastChanges();
        }
    }

    public boolean isSlotDisabled(int slotId) {
        return slotId >= 0 && slotId < SLOT_COUNT && containerData.get(slotId) == 1;
    }

    public boolean isPowered() {
        return containerData.get(kPoweredData) == 1;
    }

    public ItemStack quickMoveStack(Player player, int slotIndex) {
        ItemStack original = ItemStack.EMPTY;
        Slot slot = slots.get(slotIndex);
        if (slot != null && slot.hasItem()) {
            ItemStack moving = slot.getItem();
            original = moving.copy();
            if (slotIndex < SLOT_COUNT) {
                if (!moveItemStackTo(moving, kInventoryStart, kInventoryEnd, true)) {
                    return ItemStack.EMPTY;
                }
            } else if (slotIndex >= kResultSlot || !moveItemStackTo(moving, 0, SLOT_COUNT, false)) {
                return ItemStack.EMPTY;
            }
            if (moving.isEmpty()) {
                slot.setByPlayer(ItemStack.EMPTY);
            } else {
                slot.setChanged();
            }
            if (moving.getCount() == original.getCount()) {
                return ItemStack.EMPTY;
            }
            slot.onTake(player, moving);
        }
        return original;
    }

    public boolean stillValid(Player player) {
        return container.stillValid(player);
    }

    public Container getContainer() {
        return container;
    }

    public void slotChanged(AbstractContainerMenu container, int slotIndex, ItemStack itemStack) {
        slotsChanged(this.container);
    }

    public void dataChanged(AbstractContainerMenu container, int id, int value) {}
}
