// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.inventory;

import java.util.ArrayList;
import java.util.List;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.player.Inventory;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.entity.player.StackedItemContents;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.crafting.RecipeHolder;
import org.basaltmc.bukkit.facade.NativeCraftingRecipe;
import org.basaltmc.bukkit.recipe.Recipes;

public abstract class AbstractCraftingMenu extends RecipeBookMenu {
    private final int width;
    private final int height;
    protected final CraftingContainer craftSlots;
    protected final ResultContainer resultSlots = new ResultContainer();

    public AbstractCraftingMenu(MenuType<?> menuType, int containerId, int width, int height) {
        super(menuType, containerId);
        this.width = width;
        this.height = height;
        this.craftSlots = new TransientCraftingContainer(this, width, height);
    }

    protected Slot addResultSlot(Player player, int x, int y) {
        return addSlot(new Slot(resultSlots, 0, x, y) {
            @Override
            public boolean mayPlace(ItemStack itemStack) {
                return false;
            }

            @Override
            public void onTake(Player taker, ItemStack carried) {
                consumeIngredients();
            }
        });
    }

    protected void addCraftingGridSlots(int left, int top) {
        for (int row = 0; row < height; row++) {
            for (int column = 0; column < width; column++) {
                addSlot(new Slot(craftSlots, column + row * width, left + column * 18, top + row * 18));
            }
        }
    }

    private void consumeIngredients() {
        Recipes.Crafted crafted = NativeCraftingRecipe.craftPublic(craftSlots.asCraftInput());
        for (int slot = 0; slot < craftSlots.getContainerSize(); slot++) {
            ItemStack in = craftSlots.getItem(slot);
            if (in.isEmpty()) {
                continue;
            }
            ItemStack remaining = crafted != null && crafted.remaining() != null && slot < crafted.remaining().length
                ? org.bukkit.craftbukkit.inventory.CraftItemStack.asNMSCopy(crafted.remaining()[slot]) : ItemStack.EMPTY;
            in.shrink(1);
            craftSlots.setItem(slot, in.isEmpty() ? remaining : in);
        }
    }

    protected void refreshResult() {
        Recipes.Crafted crafted = craftSlots.isEmpty() ? null : NativeCraftingRecipe.craftPublic(craftSlots.asCraftInput());
        resultSlots.setItem(0, crafted == null || crafted.result() == null ? ItemStack.EMPTY
            : org.bukkit.craftbukkit.inventory.CraftItemStack.asNMSCopy(crafted.result()));
    }

    @Override
    public void slotsChanged(net.minecraft.world.Container container) {
        if (container == craftSlots) {
            refreshResult();
        }
        super.slotsChanged(container);
    }

    @Override
    public PostPlaceAction handlePlacement(boolean useMaxItems, boolean allowDroppingItemsToClear, RecipeHolder<?> recipe,
            ServerLevel level, Inventory inventory) {
        return PostPlaceAction.NOTHING;
    }

    @Override
    public void fillCraftSlotsStackedContents(StackedItemContents stackedContents) {
    }

    @Override
    public RecipeBookType getRecipeBookType() {
        return RecipeBookType.CRAFTING;
    }

    public abstract Slot getResultSlot();

    public abstract List<Slot> getInputGridSlots();

    public int getGridWidth() {
        return width;
    }

    public int getGridHeight() {
        return height;
    }

    public int getSize() {
        return width * height + 1;
    }
}
