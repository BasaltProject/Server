// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.inventory;

import net.minecraft.world.Container;
import net.minecraft.world.SimpleContainer;
import net.minecraft.world.entity.player.Inventory;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;

public abstract class AbstractFurnaceMenu extends AbstractContainerMenu {
    public static final int INGREDIENT_SLOT = 0;
    public static final int FUEL_SLOT = 1;
    public static final int RESULT_SLOT = 2;
    public static final int SLOT_COUNT = 3;
    public static final int DATA_COUNT = 4;
    private final Container container;
    private final ContainerData data;

    protected AbstractFurnaceMenu(MenuType<?> menuType, int containerId, Inventory inventory, Container container, ContainerData data) {
        super(menuType, containerId);
        checkContainerSize(container, SLOT_COUNT);
        checkContainerDataCount(data, DATA_COUNT);
        this.container = container;
        this.data = data;
        addSlot(new Slot(container, INGREDIENT_SLOT, 56, 17));
        addSlot(new FurnaceFuelSlot(this, container, FUEL_SLOT, 56, 53));
        addSlot(new FurnaceResultSlot(inventory.player, container, RESULT_SLOT, 116, 35));
        addStandardInventorySlots(inventory, 8, 84);
        addDataSlots(data);
    }

    public float getBurnProgress() {
        int total = data.get(DATA_COOKING_TOTAL);
        return total == 0 ? 0.0F : Math.min(data.get(DATA_COOKING_PROGRESS) / (float) total, 1.0F);
    }

    public float getLitProgress() {
        int duration = data.get(DATA_LIT_DURATION);
        return duration == 0 ? 0.0F : Math.clamp(data.get(DATA_LIT_TIME) / (float) duration, 0.0F, 1.0F);
    }

    public boolean isLit() {
        return data.get(DATA_LIT_TIME) > 0;
    }

    private static final int DATA_LIT_TIME = 0;
    private static final int DATA_LIT_DURATION = 1;
    private static final int DATA_COOKING_PROGRESS = 2;
    private static final int DATA_COOKING_TOTAL = 3;

    protected boolean canSmelt(ItemStack stack) {
        return org.basaltmc.bukkit.recipe.Recipes.cooks(stack.getItem().itemKey().toString(), recipeKind());
    }

    protected String recipeKind() {
        return "furnace";
    }

    public boolean isFuel(ItemStack stack) {
        return org.basaltmc.bukkit.item.ItemData.of(stack.getItem().itemKey().toString()).burnTicks() > 0;
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
        if (slotIndex == RESULT_SLOT) {
            if (!moveItemStackTo(stack, SLOT_COUNT, slots.size(), true)) {
                return ItemStack.EMPTY;
            }
        } else if (slotIndex >= SLOT_COUNT) {
            boolean smeltable = canSmelt(stack);
            boolean fuel = !smeltable && isFuel(stack);
            if (smeltable ? !moveItemStackTo(stack, INGREDIENT_SLOT, INGREDIENT_SLOT + 1, false)
                : fuel ? !moveItemStackTo(stack, FUEL_SLOT, FUEL_SLOT + 1, false) : true) {
                int mainEnd = SLOT_COUNT + 27;
                if (slotIndex < mainEnd ? !moveItemStackTo(stack, mainEnd, slots.size(), false)
                    : !moveItemStackTo(stack, SLOT_COUNT, mainEnd, false)) {
                    return ItemStack.EMPTY;
                }
            }
        } else if (!moveItemStackTo(stack, SLOT_COUNT, slots.size(), false)) {
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
