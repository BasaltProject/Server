// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.inventory;

import java.util.List;
import net.minecraft.world.entity.player.Inventory;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.block.Blocks;

public class CraftingMenu extends AbstractCraftingMenu {
    public static final int RESULT_SLOT = 0;
    private static final int CRAFT_SLOT_START = 1;
    private static final int CRAFT_SLOT_END = 10;
    private static final int INV_SLOT_END = 37;
    private static final int USE_ROW_SLOT_END = 46;
    private final ContainerLevelAccess access;
    private final Player player;

    public CraftingMenu(int containerId, Inventory inventory) {
        this(containerId, inventory, ContainerLevelAccess.NULL);
    }

    public CraftingMenu(int containerId, Inventory inventory, ContainerLevelAccess access) {
        super(MenuType.CRAFTING, containerId, 3, 3);
        this.access = access;
        this.player = inventory.player;
        addResultSlot(player, 124, 35);
        addCraftingGridSlots(30, 17);
        addStandardInventorySlots(inventory, 8, 84);
    }

    @Override
    public void removed(Player player) {
        super.removed(player);
        for (int slot = 0; slot < craftSlots.getContainerSize(); slot++) {
            ItemStack stack = craftSlots.removeItemNoUpdate(slot);
            if (!stack.isEmpty()) {
                player.getInventory().add(stack);
            }
        }
    }

    @Override
    public boolean stillValid(Player player) {
        return access == ContainerLevelAccess.NULL || stillValid(access, player, Blocks.CRAFTING_TABLE);
    }

    @Override
    public ItemStack quickMoveStack(Player player, int slotIndex) {
        Slot slot = slots.get(slotIndex);
        if (slot == null || !slot.hasItem()) {
            return ItemStack.EMPTY;
        }
        ItemStack stack = slot.getItem();
        ItemStack original = stack.copy();
        boolean moved;
        if (slotIndex == RESULT_SLOT) {
            moved = moveItemStackTo(stack, CRAFT_SLOT_END, USE_ROW_SLOT_END, true);
            if (moved) {
                slot.onQuickCraft(stack, original);
            }
        } else if (slotIndex >= CRAFT_SLOT_END) {
            moved = moveItemStackTo(stack, CRAFT_SLOT_START, CRAFT_SLOT_END, false)
                || (slotIndex < INV_SLOT_END ? moveItemStackTo(stack, INV_SLOT_END, USE_ROW_SLOT_END, false)
                    : moveItemStackTo(stack, CRAFT_SLOT_END, INV_SLOT_END, false));
        } else {
            moved = moveItemStackTo(stack, CRAFT_SLOT_END, USE_ROW_SLOT_END, false);
        }
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

    @Override
    public boolean canTakeItemForPickAll(ItemStack carried, Slot target) {
        return target.container != resultSlots && super.canTakeItemForPickAll(carried, target);
    }

    @Override
    public Slot getResultSlot() {
        return slots.get(RESULT_SLOT);
    }

    @Override
    public List<Slot> getInputGridSlots() {
        return slots.subList(CRAFT_SLOT_START, CRAFT_SLOT_END);
    }
}
