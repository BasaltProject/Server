// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.recipebook;
import java.util.ArrayList;
import java.util.List;
import net.minecraft.core.Holder;
import net.minecraft.core.component.DataComponents;
import net.minecraft.util.Prediction;
import net.minecraft.world.entity.player.Inventory;
import net.minecraft.world.entity.player.StackedItemContents;
import net.minecraft.world.inventory.RecipeBookMenu;
import net.minecraft.world.inventory.Slot;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.crafting.Recipe;
import net.minecraft.world.item.crafting.RecipeHolder;
import net.minecraft.world.item.crafting.RecipeInput;
public class ServerPlaceRecipe<R extends Recipe<?>> {
    private final CraftingMenuAccess<R> menu;
    private final Inventory inventory;
    private final boolean useMaxItems;
    private final int gridWidth;
    private final int gridHeight;
    private final List<Slot> inputGridSlots;
    private final List<Slot> slotsToClear;
    public static <I extends RecipeInput, R extends Recipe<I>> RecipeBookMenu.PostPlaceAction placeRecipe(CraftingMenuAccess<R> menu, int gridWidth, int gridHeight, List<Slot> inputGridSlots, List<Slot> slotsToClear, Inventory inventory, RecipeHolder<R> recipe, boolean useMaxItems, boolean allowDroppingItemsToClear) {
        ServerPlaceRecipe<R> placer = new ServerPlaceRecipe<>(menu, inventory, useMaxItems, gridWidth, gridHeight, inputGridSlots, slotsToClear);
        if (!allowDroppingItemsToClear && !placer.gridFitsInInventory()) {
            return RecipeBookMenu.PostPlaceAction.NOTHING;
        }
        StackedItemContents available = new StackedItemContents();
        inventory.fillStackedContents(available);
        menu.fillCraftSlotsStackedContents(available);
        if (available.canCraft(recipe.value(), null)) {
            placer.fill(recipe, available);
            inventory.setChanged();
            return RecipeBookMenu.PostPlaceAction.NOTHING;
        }
        placer.emptyGrid();
        inventory.setChanged();
        return RecipeBookMenu.PostPlaceAction.PLACE_GHOST_RECIPE;
    }
    private ServerPlaceRecipe(CraftingMenuAccess<R> menu, Inventory inventory, boolean useMaxItems, int gridWidth, int gridHeight, List<Slot> inputGridSlots, List<Slot> slotsToClear) {
        this.menu = menu;
        this.inventory = inventory;
        this.useMaxItems = useMaxItems;
        this.gridWidth = gridWidth;
        this.gridHeight = gridHeight;
        this.inputGridSlots = inputGridSlots;
        this.slotsToClear = slotsToClear;
    }
    private void emptyGrid() {
        for (Slot slot : slotsToClear) {
            ItemStack returned = slot.getItem().copy();
            inventory.placeItemBackInInventory(returned, false, Prediction.SERVER_ONLY);
            slot.set(returned);
        }
        menu.clearCraftingContent();
    }
    private void fill(RecipeHolder<R> recipe, StackedItemContents available) {
        boolean alreadyPlaced = menu.recipeMatches(recipe);
        int biggest = available.getBiggestCraftableStack(recipe.value(), null);
        if (alreadyPlaced) {
            for (Slot slot : inputGridSlots) {
                ItemStack held = slot.getItem();
                if (!held.isEmpty() && Math.min(biggest, held.getMaxStackSize()) < held.getCount() + 1) {
                    return;
                }
            }
        }
        int amount = wantedAmount(biggest, alreadyPlaced);
        List<Holder<Item>> used = new ArrayList<>();
        if (!available.canCraft(recipe.value(), amount, used::add)) {
            return;
        }
        int capped = amount;
        for (Holder<Item> item : used) {
            capped = Math.min(capped, item.components().getOrDefault(DataComponents.MAX_STACK_SIZE, 1));
        }
        if (capped != amount) {
            used.clear();
            if (!available.canCraft(recipe.value(), capped, used::add)) {
                return;
            }
        }
        emptyGrid();
        int each = capped;
        PlaceRecipeHelper.placeRecipe(gridWidth, gridHeight, recipe.value(), recipe.value().placementInfo().slotsToIngredientIndex(), (ingredient, gridIndex, x, y) -> {
            if (ingredient == -1) {
                return;
            }
            Slot target = inputGridSlots.get(gridIndex);
            int left = each;
            while (left > 0 && left != -1) {
                left = pull(target, used.get(ingredient), left);
            }
        });
    }
    private int wantedAmount(int biggest, boolean alreadyPlaced) {
        if (useMaxItems) {
            return biggest;
        }
        if (!alreadyPlaced) {
            return 1;
        }
        int smallest = Integer.MAX_VALUE;
        for (Slot slot : inputGridSlots) {
            ItemStack held = slot.getItem();
            if (!held.isEmpty() && held.getCount() < smallest) {
                smallest = held.getCount();
            }
        }
        return smallest == Integer.MAX_VALUE ? smallest : smallest + 1;
    }
    private int pull(Slot target, Holder<Item> item, int count) {
        ItemStack current = target.getItem();
        int from = inventory.findSlotMatchingCraftingIngredient(item, current);
        if (from == -1) {
            return -1;
        }
        ItemStack taken = count < inventory.getItem(from).getCount() ? inventory.removeItem(from, count) : inventory.removeItemNoUpdate(from);
        if (current.isEmpty()) {
            target.set(taken);
        } else {
            current.grow(taken.getCount());
        }
        return count - taken.getCount();
    }
    private boolean gridFitsInInventory() {
        int free = 0;
        for (ItemStack held : inventory.getNonEquipmentItems()) {
            if (held.isEmpty()) {
                free++;
            }
        }
        List<ItemStack> pending = new ArrayList<>();
        for (Slot slot : inputGridSlots) {
            ItemStack stack = slot.getItem().copy();
            if (stack.isEmpty() || inventory.getSlotWithRemainingSpace(stack) != -1) {
                continue;
            }
            for (ItemStack queued : pending) {
                if (ItemStack.isSameItem(queued, stack) && queued.getCount() + stack.getCount() <= queued.getMaxStackSize()) {
                    queued.grow(stack.getCount());
                    stack.setCount(0);
                    break;
                }
            }
            if (!stack.isEmpty()) {
                if (pending.size() >= free) {
                    return false;
                }
                pending.add(stack);
            }
        }
        return true;
    }
    public interface CraftingMenuAccess<T extends Recipe<?>> {
        void fillCraftSlotsStackedContents(StackedItemContents stackedContents);
        void clearCraftingContent();
        boolean recipeMatches(RecipeHolder<T> recipe);
    }
}
