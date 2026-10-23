package org.basaltmc.bukkit.recipe;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.NoSuchElementException;
import org.basaltmc.bukkit.InventoryNatives;
import org.basaltmc.bukkit.item.Nbt;
import org.bukkit.Keyed;
import org.bukkit.NamespacedKey;
import org.bukkit.inventory.ItemStack;
import org.bukkit.inventory.Recipe;

/** The server's recipe list, as the Bukkit calls on the server see it. */
public final class Recipes {
    private Recipes() {}

    public static boolean add(Recipe recipe) {
        if (!(recipe instanceof Keyed keyed)) {
            throw new IllegalArgumentException("Cannot register a recipe that has no key: " + recipe);
        }
        Map<String, Object> data = RecipeCodec.toNbt(recipe);
        return InventoryNatives.addRecipe(keyed.getKey().toString(), Nbt.write(data));
    }

    public static boolean addAll(Iterable<Recipe> recipes) {
        boolean all = true;
        for (Recipe recipe : recipes) {
            all &= add(recipe);
        }
        return all;
    }

    public static Recipe get(NamespacedKey key) {
        byte[] bytes = InventoryNatives.recipeData(key.toString());
        Map<String, Object> data = bytes == null ? null : Nbt.read(bytes);
        if (data == null) {
            return null;
        }
        data.put("id", key.toString());
        return RecipeCodec.fromNbt(data);
    }

    public static boolean has(NamespacedKey key) {
        return InventoryNatives.recipeData(key.toString()) != null;
    }

    public static boolean remove(NamespacedKey key) {
        return InventoryNatives.removeRecipe(key.toString());
    }

    public static boolean removeAll(Iterable<NamespacedKey> keys) {
        boolean all = true;
        for (NamespacedKey key : keys) {
            all &= remove(key);
        }
        return all;
    }

    public static void clear() {
        InventoryNatives.clearRecipes();
    }

    public static void reset() {
        InventoryNatives.resetRecipes();
    }

    public static Iterator<Recipe> iterator() {
        List<String> ids = new ArrayList<>(List.of(InventoryNatives.recipeIds()));
        return new Iterator<>() {
            private int at;
            private Recipe ready;
            private NamespacedKey last;

            private void advance() {
                while (ready == null && at < ids.size()) {
                    NamespacedKey key = NamespacedKey.fromString(ids.get(at++));
                    Recipe found = key == null ? null : get(key);
                    if (found != null) {
                        ready = found;
                    }
                }
            }

            @Override
            public boolean hasNext() {
                advance();
                return ready != null;
            }

            @Override
            public Recipe next() {
                advance();
                if (ready == null) {
                    throw new NoSuchElementException();
                }
                Recipe out = ready;
                ready = null;
                last = out instanceof Keyed keyed ? keyed.getKey() : null;
                return out;
            }

            @Override
            public void remove() {
                if (last == null) {
                    throw new IllegalStateException();
                }
                Recipes.remove(last);
                last = null;
            }
        };
    }

    public static List<Recipe> producing(ItemStack result) {
        List<Recipe> out = new ArrayList<>();
        Iterator<Recipe> all = iterator();
        while (all.hasNext()) {
            Recipe recipe = all.next();
            if (recipe.getResult() != null && result.isSimilar(recipe.getResult())) {
                out.add(recipe);
            }
        }
        return out;
    }

    /** What a crafting grid makes: the recipe, its result and what each cell is left holding. */
    public record Crafted(NamespacedKey recipe, ItemStack result, ItemStack[] remaining) {}

    public static Crafted craft(ItemStack[] matrix) {
        int width = matrix.length == 4 ? 2 : matrix.length == 9 ? 3 : 0;
        if (width == 0) {
            throw new IllegalArgumentException("Must be exactly 4 or 9 items");
        }
        List<Object> cells = new ArrayList<>();
        for (ItemStack stack : matrix) {
            cells.add(stack == null || stack.getType().isAir() ? Nbt.compound()
                : org.basaltmc.bukkit.item.Convert.stack(stack));
        }
        Map<String, Object> input = Nbt.compound();
        input.put("items", cells);
        byte[] bytes = InventoryNatives.craft(width, width, Nbt.write(input));
        Map<String, Object> made = bytes == null ? null : Nbt.read(bytes);
        if (made == null) {
            return null;
        }
        ItemStack result = org.basaltmc.bukkit.item.Convert.stack(Nbt.map(made.get("result")));
        List<Object> left = Nbt.list(made.get("remaining"));
        ItemStack[] remaining = new ItemStack[matrix.length];
        for (int at = 0; at < remaining.length; at++) {
            Map<String, Object> cell = left != null && at < left.size() ? Nbt.map(left.get(at)) : null;
            remaining[at] = cell == null || cell.isEmpty() ? null : org.basaltmc.bukkit.item.Convert.stack(cell);
        }
        return new Crafted(NamespacedKey.fromString(Nbt.string(made, "recipe", "")), result, remaining);
    }
}
