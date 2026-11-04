// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.inventory;

import net.minecraft.world.Container;
import net.minecraft.world.SimpleContainer;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;

public class BeaconMenu extends AbstractContainerMenu {
    private static final int PAYMENT_SLOT = 0;
    private final Container beacon;
    private final ContainerData beaconData;
    private final ContainerLevelAccess access;

    public BeaconMenu(int containerId, Container inventory) {
        this(containerId, inventory, new SimpleContainerData(3), ContainerLevelAccess.NULL);
    }

    public BeaconMenu(int containerId, Container inventory, ContainerData beaconData, ContainerLevelAccess access) {
        super(MenuType.BEACON, containerId);
        checkContainerDataCount(beaconData, 3);
        this.beaconData = beaconData;
        this.access = access;
        this.beacon = new SimpleContainer(1);
        addSlot(new Slot(beacon, PAYMENT_SLOT, 136, 110));
        addInventoryExtendedSlots(inventory, 36, 137);
        addInventoryHotbarSlots(inventory, 36, 195);
        addDataSlots(beaconData);
    }

    public int getLevels() {
        return beaconData.get(0);
    }

    public boolean hasPayment() {
        return !beacon.getItem(PAYMENT_SLOT).isEmpty();
    }

    @Override
    public boolean stillValid(Player player) {
        return access == ContainerLevelAccess.NULL || stillValid(access, player, net.minecraft.world.level.block.Blocks.BEACON);
    }

    @Override
    public ItemStack quickMoveStack(Player player, int slotIndex) {
        Slot slot = slots.get(slotIndex);
        if (slot == null || !slot.hasItem()) {
            return ItemStack.EMPTY;
        }
        ItemStack stack = slot.getItem();
        ItemStack original = stack.copy();
        boolean moved = slotIndex == PAYMENT_SLOT ? moveItemStackTo(stack, 1, slots.size(), true)
            : moveItemStackTo(stack, PAYMENT_SLOT, 1, false);
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
