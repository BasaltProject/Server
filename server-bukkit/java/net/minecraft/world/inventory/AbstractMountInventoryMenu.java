// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.inventory;

import net.minecraft.world.Container;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.player.Inventory;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.Items;

public abstract class AbstractMountInventoryMenu extends AbstractContainerMenu {
    protected final Container mountContainer;
    protected final LivingEntity mount;
    protected final int SLOT_SADDLE = 0;
    protected final int SLOT_BODY_ARMOR = 1;
    protected final int SLOT_INVENTORY_START = 2;
    protected static final int INVENTORY_ROWS = 3;

    protected AbstractMountInventoryMenu(int containerId, Inventory playerInventory, Container mountInventory, LivingEntity mount) {
        super(null, containerId);
        this.mountContainer = mountInventory;
        this.mount = mount;
        mountInventory.startOpen(playerInventory.player);
        addSlot(new Slot(mountInventory, SLOT_SADDLE, 8, 18) {
            @Override
            public boolean mayPlace(ItemStack itemStack) {
                return itemStack.is(Items.SADDLE) && !hasItem();
            }

            @Override
            public int getMaxStackSize() {
                return 1;
            }
        });
        addSlot(new Slot(mountInventory, SLOT_BODY_ARMOR, 8, 36) {
            @Override
            public boolean mayPlace(ItemStack itemStack) {
                return itemStack.getItem().itemKey().getPath().endsWith("_armor") && !hasItem();
            }

            @Override
            public int getMaxStackSize() {
                return 1;
            }
        });
        int columns = (mountInventory.getContainerSize() - SLOT_INVENTORY_START) / INVENTORY_ROWS;
        for (int row = 0; row < INVENTORY_ROWS; row++) {
            for (int column = 0; column < columns; column++) {
                addSlot(new Slot(mountInventory, SLOT_INVENTORY_START + column + row * columns, 80 + column * 18, 18 + row * 18));
            }
        }
        addStandardInventorySlots(playerInventory, 8, 84);
    }

    protected abstract boolean hasInventoryChanged(Container container);

    @Override
    public boolean stillValid(Player player) {
        return !hasInventoryChanged(mountContainer) && mountContainer.stillValid(player) && mount.isAlive()
            && player.isWithinEntityInteractionRange(mount, 4.0);
    }

    @Override
    public void removed(Player player) {
        super.removed(player);
        mountContainer.stopOpen(player);
    }

    @Override
    public ItemStack quickMoveStack(Player player, int slotIndex) {
        Slot slot = slots.get(slotIndex);
        if (slot == null || !slot.hasItem()) {
            return ItemStack.EMPTY;
        }
        ItemStack stack = slot.getItem();
        ItemStack original = stack.copy();
        int mountSlots = mountContainer.getContainerSize();
        if (slotIndex < mountSlots) {
            if (!moveItemStackTo(stack, mountSlots, slots.size(), true)) {
                return ItemStack.EMPTY;
            }
        } else if (getSlot(SLOT_BODY_ARMOR).mayPlace(stack) && !getSlot(SLOT_BODY_ARMOR).hasItem()) {
            if (!moveItemStackTo(stack, SLOT_BODY_ARMOR, SLOT_BODY_ARMOR + 1, false)) {
                return ItemStack.EMPTY;
            }
        } else if (getSlot(SLOT_SADDLE).mayPlace(stack)) {
            if (!moveItemStackTo(stack, SLOT_SADDLE, SLOT_SADDLE + 1, false)) {
                return ItemStack.EMPTY;
            }
        } else if (mountSlots <= SLOT_INVENTORY_START || !moveItemStackTo(stack, SLOT_INVENTORY_START, mountSlots, false)) {
            return ItemStack.EMPTY;
        }
        if (stack.isEmpty()) {
            slot.setByPlayer(ItemStack.EMPTY);
        } else {
            slot.setChanged();
        }
        return original;
    }

    public static int getInventorySize(int inventoryColumns) {
        return inventoryColumns * INVENTORY_ROWS;
    }
}
