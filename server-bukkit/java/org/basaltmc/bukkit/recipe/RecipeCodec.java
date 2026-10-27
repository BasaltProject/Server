package org.basaltmc.bukkit.recipe;

import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import org.basaltmc.bukkit.item.Convert;
import org.basaltmc.bukkit.item.Nbt;
import org.bukkit.Material;
import org.bukkit.NamespacedKey;
import org.bukkit.Registry;
import org.bukkit.inventory.BlastingRecipe;
import org.bukkit.inventory.CampfireRecipe;
import org.bukkit.inventory.CookingRecipe;
import org.bukkit.inventory.FurnaceRecipe;
import org.bukkit.inventory.ItemStack;
import org.bukkit.inventory.Recipe;
import org.bukkit.inventory.RecipeChoice;
import org.bukkit.inventory.ShapedRecipe;
import org.bukkit.inventory.ShapelessRecipe;
import org.bukkit.inventory.SmithingRecipe;
import org.bukkit.inventory.SmithingTransformRecipe;
import org.bukkit.inventory.SmithingTrimRecipe;
import org.bukkit.inventory.SmokingRecipe;
import org.bukkit.inventory.StonecuttingRecipe;
import org.bukkit.inventory.TransmuteRecipe;

public final class RecipeCodec {
    private static final int kShaped = 0;
    private static final int kShapeless = 1;
    private static final int kCooking = 2;
    private static final int kStonecutting = 3;
    private static final int kSmithing = 4;
    private static final int kSpecial = 5;
    private static final int kBrewing = 6;

    private RecipeCodec() {}

    static Map<String, Object> ingredient(RecipeChoice choice) {
        Map<String, Object> out = Nbt.compound();
        List<Object> items = new ArrayList<>();
        List<Object> exact = new ArrayList<>();
        if (choice instanceof RecipeChoice.MaterialChoice materials) {
            for (Material material : materials.getChoices()) {
                items.add(material.getKey().toString());
            }
        } else if (choice instanceof RecipeChoice.ExactChoice stacks) {
            for (ItemStack stack : stacks.getChoices()) {
                exact.add(Convert.stack(stack));
            }
        } else if (choice instanceof RecipeChoice.ItemTypeChoice types) {
            for (io.papermc.paper.registry.TypedKey<org.bukkit.inventory.ItemType> key : types.itemTypes()) {
                items.add(key.key().asString());
            }
        } else if (choice != null && !choice.equals(RecipeChoice.empty())) {
            ItemStack single = choice.getItemStack();
            if (single != null && !single.getType().isAir()) {
                items.add(single.getType().getKey().toString());
            } else {
                throw new IllegalArgumentException("Cannot register a recipe with a choice of " + choice.getClass());
            }
        }
        out.put("items", items);
        out.put("tag", "");
        out.put("exact", exact);
        return out;
    }

    private static Map<String, Object> blank() {
        return ingredient(RecipeChoice.empty());
    }

    public static Map<String, Object> toNbt(Recipe recipe) {
        Map<String, Object> out = Nbt.compound();
        ItemStack result = recipe.getResult();
        out.put("result", Convert.stack(result == null || result.getType().isAir()
            ? new ItemStack(Material.STONE) : result));
        if (recipe instanceof ShapedRecipe shaped) {
            out.put("kind", kShaped);
            String[] rows = shaped.getShape();
            int width = 0;
            for (String row : rows) {
                width = Math.max(width, row.length());
            }
            out.put("width", width);
            out.put("height", rows.length);
            Map<Character, RecipeChoice> choices = shaped.getChoiceMap();
            List<Object> cells = new ArrayList<>();
            for (String row : rows) {
                for (int at = 0; at < width; at++) {
                    char key = at < row.length() ? row.charAt(at) : ' ';
                    RecipeChoice choice = choices.get(key);
                    cells.add(key == ' ' || choice == null ? Nbt.compound() : ingredient(choice));
                }
            }
            out.put("shape", cells);
            out.put("group", shaped.getGroup());
        } else if (recipe instanceof TransmuteRecipe transmute) {
            out.put("kind", kShapeless);
            List<Object> pair = new ArrayList<>();
            pair.add(ingredient(transmute.getInput()));
            pair.add(ingredient(transmute.getMaterial()));
            out.put("ingredients", pair);
            out.put("transmute", Nbt.bool(true));
            out.put("group", transmute.getGroup());
        } else if (recipe instanceof ShapelessRecipe shapeless) {
            out.put("kind", kShapeless);
            List<Object> list = new ArrayList<>();
            for (RecipeChoice choice : shapeless.getChoiceList()) {
                list.add(ingredient(choice));
            }
            out.put("ingredients", list);
            out.put("group", shapeless.getGroup());
        } else if (recipe instanceof CookingRecipe<?> cooking) {
            out.put("kind", kCooking);
            out.put("cooker", cooking instanceof BlastingRecipe ? 1 : cooking instanceof SmokingRecipe ? 2
                : cooking instanceof CampfireRecipe ? 3 : 0);
            out.put("ingredient", ingredient(cooking.getInputChoice()));
            out.put("experience", cooking.getExperience());
            out.put("cooking_time", cooking.getCookingTime());
            out.put("group", cooking.getGroup());
        } else if (recipe instanceof StonecuttingRecipe stonecutting) {
            out.put("kind", kStonecutting);
            out.put("ingredient", ingredient(stonecutting.getInputChoice()));
            out.put("group", stonecutting.getGroup());
        } else if (recipe instanceof SmithingTransformRecipe transform) {
            out.put("kind", kSmithing);
            out.put("template", ingredient(transform.getTemplate()));
            out.put("base", ingredient(transform.getBase()));
            out.put("addition", ingredient(transform.getAddition()));
        } else if (recipe instanceof SmithingTrimRecipe trim) {
            out.put("kind", kSmithing);
            out.put("template", ingredient(trim.getTemplate()));
            out.put("base", ingredient(trim.getBase()));
            out.put("addition", ingredient(trim.getAddition()));
            if (trim.getTrimPattern() != null) {
                out.put("trim_pattern", trim.getTrimPattern().getKey().toString());
            }
        } else if (recipe instanceof org.bukkit.inventory.BrewingRecipe brewing) {
            out.put("kind", kBrewing);
            out.put("base", ingredient(brewing.getInput()));
            out.put("ingredient", ingredient(brewing.getIngredient()));
        } else {
            throw new IllegalArgumentException("Cannot register a recipe of " + recipe.getClass().getName());
        }
        return out;
    }

    private static RecipeChoice choice(Object value) {
        Map<String, Object> data = Nbt.map(value);
        if (data == null) {
            return RecipeChoice.empty();
        }
        List<Object> exact = Nbt.list(data.get("exact"));
        if (exact != null && !exact.isEmpty()) {
            List<ItemStack> stacks = Convert.stacksOf(exact);
            return new RecipeChoice.ExactChoice(stacks);
        }
        List<Object> items = Nbt.list(data.get("items"));
        List<Material> materials = new ArrayList<>();
        if (items != null) {
            for (Object id : items) {
                Material material = Material.matchMaterial(String.valueOf(id));
                if (material != null) {
                    materials.add(material);
                }
            }
        }
        return materials.isEmpty() ? RecipeChoice.empty() : new RecipeChoice.MaterialChoice(materials);
    }

    private static boolean filled(Object cell) {
        Map<String, Object> data = Nbt.map(cell);
        return data != null && !data.isEmpty();
    }

    /** A Bukkit recipe from a server recipe, or null for the kinds the Bukkit API has no class for. */
    public static Recipe fromNbt(Map<String, Object> data) {
        NamespacedKey key = NamespacedKey.fromString(Nbt.string(data, "id", ""));
        if (key == null) {
            return null;
        }
        ItemStack result = Convert.stack(Nbt.map(data.get("result")));
        if (result == null) {
            result = new ItemStack(Material.STONE);
        }
        switch (Nbt.integer(data, "kind", -1)) {
            case kShaped: {
                ShapedRecipe recipe = new ShapedRecipe(key, result);
                int width = Nbt.integer(data, "width", 0);
                int height = Nbt.integer(data, "height", 0);
                List<Object> cells = Nbt.list(data.get("shape"));
                if (cells == null || width <= 0 || height <= 0 || cells.size() < width * height) {
                    return null;
                }
                Map<Character, RecipeChoice> choices = new LinkedHashMap<>();
                char next = 'A';
                String[] rows = new String[height];
                for (int y = 0; y < height; y++) {
                    StringBuilder row = new StringBuilder();
                    for (int x = 0; x < width; x++) {
                        Object cell = cells.get(y * width + x);
                        if (filled(cell)) {
                            RecipeChoice picked = choice(cell);
                            char letter = 0;
                            for (Map.Entry<Character, RecipeChoice> entry : choices.entrySet()) {
                                if (entry.getValue().equals(picked)) {
                                    letter = entry.getKey();
                                    break;
                                }
                            }
                            if (letter == 0) {
                                letter = next++;
                                choices.put(letter, picked);
                            }
                            row.append(letter);
                        } else {
                            row.append(' ');
                        }
                    }
                    rows[y] = row.toString();
                }
                recipe.shape(rows);
                for (Map.Entry<Character, RecipeChoice> entry : choices.entrySet()) {
                    recipe.setIngredient(entry.getKey(), entry.getValue());
                }
                recipe.setGroup(Nbt.string(data, "group", ""));
                return recipe;
            }
            case kShapeless: {
                List<Object> list = Nbt.list(data.get("ingredients"));
                if (list == null) {
                    return null;
                }
                if (Nbt.flag(data, "transmute", false) && list.size() == 2) {
                    return new TransmuteRecipe(key, result.getType(), choice(list.get(0)), choice(list.get(1)));
                }
                ShapelessRecipe recipe = new ShapelessRecipe(key, result);
                for (Object entry : list) {
                    recipe.addIngredient(choice(entry));
                }
                recipe.setGroup(Nbt.string(data, "group", ""));
                return recipe;
            }
            case kCooking: {
                RecipeChoice input = choice(data.get("ingredient"));
                float experience = (float) Nbt.real(data, "experience", 0);
                int time = Nbt.integer(data, "cooking_time", 200);
                CookingRecipe<?> recipe;
                switch (Nbt.integer(data, "cooker", 0)) {
                    case 1:
                        recipe = new BlastingRecipe(key, result, input, experience, time);
                        break;
                    case 2:
                        recipe = new SmokingRecipe(key, result, input, experience, time);
                        break;
                    case 3:
                        recipe = new CampfireRecipe(key, result, input, experience, time);
                        break;
                    default:
                        recipe = new FurnaceRecipe(key, result, input, experience, time);
                        break;
                }
                recipe.setGroup(Nbt.string(data, "group", ""));
                return recipe;
            }
            case kStonecutting: {
                StonecuttingRecipe recipe = new StonecuttingRecipe(key, result, choice(data.get("ingredient")));
                recipe.setGroup(Nbt.string(data, "group", ""));
                return recipe;
            }
            case kSmithing: {
                RecipeChoice template = choice(data.get("template"));
                RecipeChoice base = choice(data.get("base"));
                RecipeChoice addition = choice(data.get("addition"));
                if (data.containsKey("trim_pattern")) {
                    NamespacedKey pattern = NamespacedKey.fromString(Nbt.string(data, "trim_pattern", ""));
                    return new SmithingTrimRecipe(key, template, base, addition,
                        pattern == null ? null : Registry.TRIM_PATTERN.get(pattern));
                }
                return new SmithingTransformRecipe(key, result, template, base, addition);
            }
            case kSpecial:
                return new SpecialRecipe(key, result);
            case kBrewing:
                return new org.bukkit.inventory.BrewingRecipe(key, result, choice(data.get("base")),
                    choice(data.get("ingredient")));
            default:
                return null;
        }
    }
}
