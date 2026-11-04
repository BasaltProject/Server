// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.crafting;
import it.unimi.dsi.fastutil.ints.IntList;
import java.util.List;
import java.util.Optional;
public class PlacementInfo {
    public static final int EMPTY_SLOT = -1;
    public static final PlacementInfo NOT_PLACEABLE = new PlacementInfo(List.of(), new it.unimi.dsi.fastutil.ints.IntArrayList());
    private final List<Ingredient> ingredients;
    private final IntList slotsToIngredientIndex;
    private PlacementInfo(List<Ingredient> ingredients, IntList slotsToIngredientIndex) {
        this.ingredients = ingredients;
        this.slotsToIngredientIndex = slotsToIngredientIndex;
    }
    public static PlacementInfo create(Ingredient ingredient) {
        return create(List.of(ingredient));
    }
    public static PlacementInfo createFromOptionals(List<Optional<Ingredient>> ingredients) {
        List<Ingredient> present = new java.util.ArrayList<>();
        it.unimi.dsi.fastutil.ints.IntList slots = new it.unimi.dsi.fastutil.ints.IntArrayList(ingredients.size());
        for (Optional<Ingredient> entry : ingredients) {
            if (entry.isPresent()) {
                slots.add(present.size());
                present.add(entry.get());
            } else {
                slots.add(EMPTY_SLOT);
            }
        }
        return buildPlacement(present, slots);
    }
    public static PlacementInfo create(List<Ingredient> ingredients) {
        List<Ingredient> present = new java.util.ArrayList<>(ingredients);
        it.unimi.dsi.fastutil.ints.IntList slots = new it.unimi.dsi.fastutil.ints.IntArrayList(ingredients.size());
        for (int i = 0; i < ingredients.size(); i++) {
            slots.add(i);
        }
        return buildPlacement(present, slots);
    }
    public IntList slotsToIngredientIndex() {
        return slotsToIngredientIndex;
    }
    public List<Ingredient> ingredients() {
        return ingredients;
    }
    public boolean isImpossibleToPlace() {
        return this == NOT_PLACEABLE;
    }
    private static PlacementInfo buildPlacement(List<Ingredient> ingredients, it.unimi.dsi.fastutil.ints.IntList slots) {
        for (Ingredient ingredient : ingredients) {
            if (ingredient.isEmpty()) {
                return NOT_PLACEABLE;
            }
        }
        return new PlacementInfo(ingredients, slots);
    }
}
