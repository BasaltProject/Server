// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.recipebook;
import java.util.Iterator;
import net.minecraft.util.Mth;
import net.minecraft.world.item.crafting.Recipe;
import net.minecraft.world.item.crafting.ShapedRecipe;
public interface PlaceRecipeHelper {
    static <T> void placeRecipe(int gridWidth, int gridHeight, Recipe<?> recipe, Iterable<T> entries, Output<T> output) {
        if (recipe instanceof ShapedRecipe shaped) {
            placeRecipe(gridWidth, gridHeight, shaped.getWidth(), shaped.getHeight(), entries, output);
        } else {
            placeRecipe(gridWidth, gridHeight, gridWidth, gridHeight, entries, output);
        }
    }
    static <T> void placeRecipe(int gridWidth, int gridHeight, int recipeWidth, int recipeHeight, Iterable<T> entries, Output<T> output) {
        Iterator<T> next = entries.iterator();
        int index = 0;
        for (int row = 0; row < gridHeight; row++) {
            int topPadding = Mth.floor(gridHeight / 2.0F - recipeHeight / 2.0F);
            if (recipeHeight < gridHeight / 2.0F && topPadding > row) {
                index += gridWidth;
                row++;
            }
            int leftPadding = Mth.floor(gridWidth / 2.0F - recipeWidth / 2.0F);
            boolean centered = recipeWidth < gridWidth / 2.0F;
            for (int column = 0; column < gridWidth; column++) {
                if (!next.hasNext()) {
                    return;
                }
                boolean inside = centered ? leftPadding <= column && column < leftPadding + recipeWidth : column < recipeWidth;
                int rowEnd = centered ? leftPadding + recipeWidth : recipeWidth;
                if (inside) {
                    output.addItemToSlot(next.next(), index, column, row);
                } else if (rowEnd == column) {
                    index += gridWidth - column;
                    break;
                }
                index++;
            }
        }
    }
    @FunctionalInterface
    public interface Output<T> {
        void addItemToSlot(T item, int gridIndex, int gridXPos, int gridYPos);
    }
}
