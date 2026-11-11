// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.recipes;

import com.google.gson.JsonArray;
import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import com.google.gson.JsonPrimitive;
import java.util.Collections;
import java.util.IdentityHashMap;
import java.util.List;
import java.util.Map;
import java.util.stream.Stream;
import net.minecraft.core.Holder;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.resources.Identifier;
import net.minecraft.tags.TagKey;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.crafting.Ingredient;
import net.minecraft.world.level.ItemLike;

public final class RecipeJson {
    private RecipeJson() {
    }

    public static Identifier itemId(ItemLike item) {
        return BuiltInRegistries.ITEM.getKey(item.asItem());
    }

    private static final Map<Ingredient, JsonElement> kTagged = Collections.synchronizedMap(new IdentityHashMap<>());

    public static Ingredient taggedIngredient(TagKey<Item> tag) {
        Ingredient marker = Ingredient.of(Stream.<ItemLike>empty());
        kTagged.put(marker, tagIngredient(tag));
        return marker;
    }

    public static JsonElement ingredient(Ingredient ingredient) {
        JsonElement tagged = kTagged.get(ingredient);
        if (tagged != null) {
            return tagged;
        }
        List<String> ids = ingredient.items().map(holder -> itemId(holder.value()).toString()).toList();
        if (ids.size() == 1) {
            return new JsonPrimitive(ids.get(0));
        }
        JsonArray array = new JsonArray();
        ids.forEach(array::add);
        return array;
    }

    public static JsonElement tagIngredient(TagKey<Item> tag) {
        return new JsonPrimitive("#" + tag.location());
    }

    public static JsonElement ingredient(ItemLike item) {
        return new JsonPrimitive(itemId(item).toString());
    }

    public static JsonObject result(ItemLike item, int count) {
        JsonObject result = new JsonObject();
        result.addProperty("id", itemId(item).toString());
        if (count != 1) {
            result.addProperty("count", count);
        }
        return result;
    }

    public static String craftingCategory(RecipeCategory category) {
        return switch (category) {
            case BUILDING_BLOCKS -> "building";
            case REDSTONE -> "redstone";
            case TOOLS, COMBAT -> "equipment";
            default -> "misc";
        };
    }

    public static String cookingCategory(RecipeCategory category) {
        return switch (category) {
            case FOOD -> "food";
            case BUILDING_BLOCKS -> "blocks";
            default -> "misc";
        };
    }

    public static void emit(RecipeOutput output, Identifier id, JsonObject json) {
        if (output instanceof JsonRecipeSink sink) {
            sink.acceptJson(id, json);
        }
    }

    public static Holder<Item> holder(ItemLike item) {
        return item.asItem().builtInRegistryHolder();
    }
}
