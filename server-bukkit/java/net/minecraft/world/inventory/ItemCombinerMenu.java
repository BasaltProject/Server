// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.inventory;

import net.minecraft.world.Container;
import net.minecraft.world.SimpleContainer;
import net.minecraft.world.entity.player.Inventory;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.block.state.BlockState;

public abstract class ItemCombinerMenu extends AbstractContainerMenu {
    private static final int kInventoryStart = 0;
    protected final ContainerLevelAccess access;
    protected final Player player;
    protected final Container inputSlots;
    protected final ResultContainer resultSlots = new ResultContainer();
    private final ItemCombinerMenuSlotDefinition definition;
    private final int resultSlotIndex;
    private final int inputSlotCount;

    public ItemCombinerMenu(MenuType<?> menuType, int containerId, Inventory inventory, ContainerLevelAccess access,
            ItemCombinerMenuSlotDefinition itemInputSlots) {
        super(menuType, containerId);
        this.access = access;
        this.player = inventory.player;
        this.definition = itemInputSlots;
        this.inputSlotCount = itemInputSlots.getNumOfInputSlots();
        this.resultSlotIndex = itemInputSlots.getResultSlotIndex();
        this.inputSlots = new SimpleContainer(inputSlotCount) {
            @Override
            public void setChanged() {
                super.setChanged();
                slotsChanged(this);
            }
        };
        for (ItemCombinerMenuSlotDefinition.SlotDefinition input : itemInputSlots.getSlots()) {
            addSlot(new Slot(inputSlots, input.slotIndex(), input.x(), input.y()) {
                @Override
                public boolean mayPlace(ItemStack stack) {
                    return input.mayPlace().test(stack);
                }
            });
        }
        ItemCombinerMenuSlotDefinition.SlotDefinition result = itemInputSlots.getResultSlot();
        addSlot(new Slot(resultSlots, result.slotIndex(), result.x(), result.y()) {
            @Override
            public boolean mayPlace(ItemStack stack) {
                return false;
            }

            @Override
            public boolean mayPickup(Player taker) {
                return ItemCombinerMenu.this.mayPickup(taker, hasItem());
            }

            @Override
            public void onTake(Player taker, ItemStack carried) {
                ItemCombinerMenu.this.onTake(taker, carried);
            }
        });
        addStandardInventorySlots(inventory, 8, 84);
    }

    protected boolean mayPickup(Player player, boolean hasItem) {
        return true;
    }

    protected abstract void onTake(Player player, ItemStack carried);

    protected abstract boolean isValidBlock(BlockState state);

    public abstract void createResult();

    @Override
    public void slotsChanged(Container container) {
        super.slotsChanged(container);
        if (container == inputSlots) {
            createResult();
        }
    }

    @Override
    public void removed(Player player) {
        super.removed(player);
        for (int slot = 0; slot < inputSlots.getContainerSize(); slot++) {
            ItemStack stack = inputSlots.removeItemNoUpdate(slot);
            if (!stack.isEmpty()) {
                player.getInventory().add(stack);
            }
        }
    }

    @Override
    public boolean stillValid(Player player) {
        return access == ContainerLevelAccess.NULL || access.evaluate((level, pos) -> isValidBlock(level.getBlockState(pos))
            && player.distanceToSqr(pos.getX() + 0.5, pos.getY() + 0.5, pos.getZ() + 0.5) <= 64.0, true);
    }

    @Override
    public ItemStack quickMoveStack(Player player, int slotIndex) {
        Slot slot = slots.get(slotIndex);
        if (slot == null || !slot.hasItem()) {
            return ItemStack.EMPTY;
        }
        ItemStack stack = slot.getItem();
        ItemStack original = stack.copy();
        int inventoryStart = inputSlotCount + 1;
        int hotbarStart = inventoryStart + 27;
        int end = slots.size();
        boolean moved;
        if (slotIndex == resultSlotIndex) {
            moved = moveItemStackTo(stack, inventoryStart, end, true);
            if (moved) {
                slot.onQuickCraft(stack, original);
            }
        } else if (slotIndex < inputSlotCount) {
            moved = moveItemStackTo(stack, inventoryStart, end, false);
        } else if (canMoveIntoInputSlots(stack) && moveItemStackTo(stack, kInventoryStart, inputSlotCount, false)) {
            moved = true;
        } else if (slotIndex < hotbarStart) {
            moved = moveItemStackTo(stack, hotbarStart, end, false);
        } else {
            moved = moveItemStackTo(stack, inventoryStart, hotbarStart, false);
        }
        if (!moved) {
            return ItemStack.EMPTY;
        }
        if (stack.isEmpty()) {
            slot.setByPlayer(ItemStack.EMPTY);
        } else {
            slot.setChanged();
        }
        if (stack.getCount() == original.getCount()) {
            return ItemStack.EMPTY;
        }
        slot.onTake(player, stack);
        return original;
    }

    protected boolean canMoveIntoInputSlots(ItemStack stack) {
        return true;
    }

    public int getResultSlot() {
        return resultSlotIndex;
    }

    protected ItemCombinerMenuSlotDefinition slotDefinition() {
        return definition;
    }
}
